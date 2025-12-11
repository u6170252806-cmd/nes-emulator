#include "frontend.hpp"
#include "emulator.hpp"
#include "ppu.hpp"
#include "bus.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <mutex>
#include <atomic>
#include <algorithm>

// ===== AUDIO RING BUFFER =====
// Proper audio buffering to prevent crackling and ensure smooth playback
class AudioBuffer {
public:
    static constexpr size_t BUFFER_SIZE = 8192;  // ~185ms at 44100Hz
    
    AudioBuffer() : write_pos(0), read_pos(0), count(0) {
        buffer.fill(0.0f);
    }
    
    void write(float sample) {
        std::lock_guard<std::mutex> lock(mutex);
        if (count < BUFFER_SIZE) {
            buffer[write_pos] = sample;
            write_pos = (write_pos + 1) % BUFFER_SIZE;
            count++;
        }
    }
    
    float read() {
        std::lock_guard<std::mutex> lock(mutex);
        if (count > 0) {
            float sample = buffer[read_pos];
            read_pos = (read_pos + 1) % BUFFER_SIZE;
            count--;
            return sample;
        }
        return 0.0f;
    }
    
    size_t available() const {
        return count;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        write_pos = 0;
        read_pos = 0;
        count = 0;
        buffer.fill(0.0f);
    }
    
private:
    std::array<float, BUFFER_SIZE> buffer;
    size_t write_pos;
    size_t read_pos;
    std::atomic<size_t> count;
    std::mutex mutex;
};

// Global audio buffer for callback
static AudioBuffer g_audio_buffer;

Frontend::Frontend() : window(nullptr), renderer(nullptr), texture(nullptr), audio_device(0) {
}

Frontend::~Frontend() {
    cleanup();
}

bool Frontend::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create window with resizable flag
    window = SDL_CreateWindow(
        "NES Emulator - Loading...",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        256 * 3,  // 3x scale (768x720)
        240 * 3,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create renderer with VSync
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        // Fallback without VSync
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
            return false;
        }
    }
    
    // Set render quality to nearest neighbor for crisp pixels
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    
    // Create texture for NES screen
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        256,
        240
    );
    
    if (!texture) {
        std::cerr << "Texture creation failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Setup audio with larger buffer for stability
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_F32;
    want.channels = 1;
    want.samples = 1024;  // Larger buffer for stability
    want.callback = audio_callback;
    want.userdata = this;
    
    audio_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (audio_device == 0) {
        std::cerr << "Audio device opening failed: " << SDL_GetError() << std::endl;
        // Continue without audio - not fatal
    } else {
        std::cout << "Audio initialized: " << have.freq << " Hz, " 
                  << (int)have.channels << " channel(s), " 
                  << have.samples << " samples" << std::endl;
        SDL_PauseAudioDevice(audio_device, 0);
    }
    
    return true;
}

void Frontend::run(const std::string& rom_path) {
    emulator = std::make_unique<Emulator>();
    
    if (!emulator->load_rom(rom_path)) {
        std::cerr << "Failed to load ROM: " << rom_path << std::endl;
        return;
    }
    
    // Extract ROM name for window title
    std::string rom_name = rom_path;
    size_t last_slash = rom_name.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        rom_name = rom_name.substr(last_slash + 1);
    }
    size_t last_dot = rom_name.find_last_of(".");
    if (last_dot != std::string::npos) {
        rom_name = rom_name.substr(0, last_dot);
    }
    
    bool running = true;
    SDL_Event event;
    
    // FPS tracking
    auto last_fps_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    double fps = 0.0;
    
    // Audio timing
    const double SAMPLE_RATE = 44100.0;
    const double CPU_FREQ = 1789773.0;  // NTSC CPU frequency
    const double SAMPLES_PER_CPU_CYCLE = SAMPLE_RATE / CPU_FREQ;
    double audio_accumulator = 0.0;
    
    // Target frame time for 60 FPS
    const double TARGET_FRAME_TIME = 1.0 / 60.0;
    
    while (running) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_r && (event.key.keysym.mod & KMOD_CTRL)) {
                    // Ctrl+R to reset
                    emulator->reset();
                    g_audio_buffer.clear();
                }
            }
        }
        
        // Update controller state
        uint8_t controller = get_controller_state();
        emulator->set_controller_state(0, controller);
        
        // Run one frame and collect audio samples
        // NES runs ~29780 CPU cycles per frame (1789773 Hz / 60 fps)
        while (!emulator->get_ppu()->frame_complete()) {
            emulator->get_bus()->clock();
            
            // Generate audio samples at proper rate
            // CPU runs at 1.789773 MHz, we need 44100 samples/sec
            // So we need a sample every ~40.58 CPU cycles
            audio_accumulator += SAMPLES_PER_CPU_CYCLE;
            if (audio_accumulator >= 1.0) {
                audio_accumulator -= 1.0;
                float sample = emulator->get_audio_sample();
                // Clamp and scale audio
                sample = std::clamp(sample * 0.5f, -1.0f, 1.0f);
                g_audio_buffer.write(sample);
            }
        }
        
        // Update texture with new frame
        const uint8_t* screen = emulator->get_screen();
        SDL_UpdateTexture(texture, nullptr, screen, 256 * 3);
        
        // Render with aspect ratio preservation
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        
        // Calculate destination rectangle maintaining 256:240 aspect ratio
        float aspect = 256.0f / 240.0f;
        int dest_w, dest_h;
        if ((float)win_w / win_h > aspect) {
            dest_h = win_h;
            dest_w = (int)(win_h * aspect);
        } else {
            dest_w = win_w;
            dest_h = (int)(win_w / aspect);
        }
        
        SDL_Rect dest_rect;
        dest_rect.x = (win_w - dest_w) / 2;
        dest_rect.y = (win_h - dest_h) / 2;
        dest_rect.w = dest_w;
        dest_rect.h = dest_h;
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, &dest_rect);
        SDL_RenderPresent(renderer);
        
        // FPS calculation
        frame_count++;
        auto current_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(current_time - last_fps_time).count();
        
        if (elapsed >= 1.0) {
            fps = frame_count / elapsed;
            frame_count = 0;
            last_fps_time = current_time;
            
            // Update window title with FPS
            std::ostringstream title;
            title << "NES Emulator - " << rom_name << " | " 
                  << std::fixed << std::setprecision(1) << fps << " FPS";
            SDL_SetWindowTitle(window, title.str().c_str());
        }
        
        // Frame timing - maintain 60 FPS
        auto frame_end = std::chrono::high_resolution_clock::now();
        double frame_time = std::chrono::duration<double>(frame_end - frame_start).count();
        
        // If we're running too fast, delay
        if (frame_time < TARGET_FRAME_TIME) {
            double sleep_time = TARGET_FRAME_TIME - frame_time;
            SDL_Delay((Uint32)(sleep_time * 1000));
        }
    }
}

void Frontend::audio_callback(void* /*userdata*/, uint8_t* stream, int len) {
    float* fstream = (float*)stream;
    int samples = len / sizeof(float);
    
    for (int i = 0; i < samples; i++) {
        fstream[i] = g_audio_buffer.read();
    }
}

uint8_t Frontend::get_controller_state() {
    // Get current keyboard state - SDL_PumpEvents is called by SDL_PollEvent in main loop
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);
    uint8_t state = 0x00;
    
    // ===== NES CONTROLLER BIT ORDER =====
    // The NES controller shift register outputs buttons serially in this order:
    // Bit 7 (MSB, read first):  A
    // Bit 6:                    B
    // Bit 5:                    Select
    // Bit 4:                    Start
    // Bit 3:                    Up
    // Bit 2:                    Down
    // Bit 1:                    Left
    // Bit 0 (LSB, read last):   Right
    //
    // When CPU reads $4016, it gets bit 7 first, then the register shifts left
    // This matches the hardware behavior of the 4021 shift register in the controller
    
    // ===== PRIMARY CONTROLS (NO CONFLICTS) =====
    // Arrow keys for D-Pad
    if (keys[SDL_SCANCODE_UP])    state |= 0x08;    // Up
    if (keys[SDL_SCANCODE_DOWN])  state |= 0x04;    // Down
    if (keys[SDL_SCANCODE_LEFT])  state |= 0x02;    // Left
    if (keys[SDL_SCANCODE_RIGHT]) state |= 0x01;    // Right
    
    // Right hand buttons (X/Z like SNES layout)
    if (keys[SDL_SCANCODE_X])     state |= 0x80;    // A button (confirm/jump)
    if (keys[SDL_SCANCODE_Z])     state |= 0x40;    // B button (cancel/run)
    
    // Left hand buttons (Q/E for Select/Start - NO CONFLICTS with WASD)
    if (keys[SDL_SCANCODE_Q])     state |= 0x20;    // Select
    if (keys[SDL_SCANCODE_E])     state |= 0x10;    // Start
    
    // ===== ALTERNATIVE CONTROLS =====
    // WASD for D-Pad (alternative to arrows)
    if (keys[SDL_SCANCODE_W])     state |= 0x08;    // Up (alt)
    if (keys[SDL_SCANCODE_S])     state |= 0x04;    // Down (alt)
    if (keys[SDL_SCANCODE_A])     state |= 0x02;    // Left (alt)
    if (keys[SDL_SCANCODE_D])     state |= 0x01;    // Right (alt)
    
    // J/K for buttons (alternative to X/Z)
    if (keys[SDL_SCANCODE_K])     state |= 0x80;    // A (alt)
    if (keys[SDL_SCANCODE_J])     state |= 0x40;    // B (alt)
    
    // Enter/Shift for Start/Select (alternative)
    if (keys[SDL_SCANCODE_RETURN])  state |= 0x10;  // Start (alt)
    if (keys[SDL_SCANCODE_RSHIFT])  state |= 0x20;  // Select (alt)
    if (keys[SDL_SCANCODE_LSHIFT])  state |= 0x20;  // Select (alt)
    
    // Space as alternative Start (common in many games)
    if (keys[SDL_SCANCODE_SPACE])   state |= 0x10;  // Start (alt)
    
    return state;
}

void Frontend::cleanup() {
    if (audio_device) {
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }
    
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    
    SDL_Quit();
}

#pragma once

#include <SDL2/SDL.h>
#include <memory>
#include <string>

// Forward declaration
class Emulator;

/**
 * SDL2 Frontend - Enhanced with FPS Counter and Better Audio
 * 
 * Features:
 * - FPS counter in window title
 * - Proper audio buffering with ring buffer
 * - Aspect ratio preservation on window resize
 * - Resizable window
 * - Ctrl+R to reset emulator
 * - Better frame timing for smooth 60 FPS
 * 
 * Handles video output, audio output, and input
 */
class Frontend {
public:
    Frontend();
    ~Frontend();
    
    // Initialize SDL (video, audio, timer)
    bool init();
    
    // Run emulator with ROM
    void run(const std::string& rom_path);
    
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_AudioDeviceID audio_device;
    
    std::unique_ptr<Emulator> emulator;
    
    // Audio callback (uses global ring buffer for thread safety)
    static void audio_callback(void* userdata, uint8_t* stream, int len);
    
    // Input handling
    uint8_t get_controller_state();
    
    // Cleanup
    void cleanup();
};

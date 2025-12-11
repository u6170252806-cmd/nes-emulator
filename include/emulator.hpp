#pragma once

#include <memory>
#include <string>

// Forward declarations
class CPU;
class PPU;
class APU;
class Bus;
class Cartridge;

/**
 * Main Emulator Class
 * 
 * Coordinates all components and manages timing
 */
class Emulator {
public:
    Emulator();
    ~Emulator();
    
    // Load ROM file
    bool load_rom(const std::string& filename);
    
    // Reset system
    void reset();
    
    // Run one frame (returns true when frame is complete)
    bool run_frame();
    
    // Get frame buffer for rendering
    const uint8_t* get_screen() const;
    
    // Get audio sample
    float get_audio_sample();
    
    // Controller input (8-bit state: A, B, Select, Start, Up, Down, Left, Right)
    void set_controller_state(uint8_t controller, uint8_t state);
    
    // Get components (for debugging)
    CPU* get_cpu();
    PPU* get_ppu();
    APU* get_apu();
    Bus* get_bus();

private:
    std::unique_ptr<CPU> cpu;
    std::unique_ptr<PPU> ppu;
    std::unique_ptr<APU> apu;
    std::unique_ptr<Bus> bus;
    std::shared_ptr<Cartridge> cartridge;
    
    // Timing
    double residual_time;
};

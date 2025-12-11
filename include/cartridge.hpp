#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>

// Forward declaration
class Mapper;

/**
 * Cartridge - Handles ROM loading and mapper interface
 * 
 * Supports iNES and NES 2.0 formats
 */
class Cartridge {
public:
    Cartridge(const std::string& filename);
    ~Cartridge();
    
    // CPU memory access (PRG ROM/RAM)
    bool cpu_read(uint16_t addr, uint8_t& data);
    bool cpu_write(uint16_t addr, uint8_t data);
    
    // PPU memory access (CHR ROM/RAM)
    bool ppu_read(uint16_t addr, uint8_t& data);
    bool ppu_write(uint16_t addr, uint8_t data);
    
    // Mapper IRQ signal
    bool irq_state();
    void irq_clear();
    
    // Scanline counter (for MMC3)
    void scanline();
    
    // Mirroring mode
    enum class Mirror {
        HORIZONTAL,
        VERTICAL,
        ONESCREEN_LO,
        ONESCREEN_HI,
        FOUR_SCREEN
    };
    
    Mirror get_mirror() const;
    
    // ROM info
    uint8_t get_mapper_id() const { return mapper_id; }
    bool is_valid() const { return valid; }

private:
    bool valid;
    
    // ROM data
    std::vector<uint8_t> prg_rom;  // Program ROM
    std::vector<uint8_t> chr_rom;  // Character ROM
    std::vector<uint8_t> prg_ram;  // Program RAM (battery-backed)
    
    // Header info
    uint8_t mapper_id;
    uint8_t prg_banks;
    uint8_t chr_banks;
    Mirror mirror_mode;
    bool battery_backed;
    
    // Mapper
    std::unique_ptr<Mapper> mapper;
    
    // Load ROM file
    bool load_ines(const std::vector<uint8_t>& data, bool dirty_header = false);
    bool load_nes2(const std::vector<uint8_t>& data);
    
    // Create mapper
    void create_mapper();
};

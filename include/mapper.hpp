#pragma once

#include <cstdint>
#include <vector>

/**
 * Mapper Base Class
 * 
 * Mappers extend the NES address space and add additional features
 * like bank switching, IRQ generation, etc.
 */
class Mapper {
public:
    Mapper(uint8_t prg_banks, uint8_t chr_banks);
    virtual ~Mapper() = default;
    
    // CPU memory access
    virtual bool cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) = 0;
    virtual bool cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) = 0;
    
    // PPU memory access
    virtual bool ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) = 0;
    virtual bool ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) = 0;
    
    // Reset mapper state
    virtual void reset() = 0;
    
    // IRQ handling (for mappers like MMC3)
    virtual bool irq_state() { return false; }
    virtual void irq_clear() {}
    virtual void scanline() {}
    
    // Mirroring
    enum class Mirror {
        HORIZONTAL,
        VERTICAL,
        ONESCREEN_LO,
        ONESCREEN_HI,
        FOUR_SCREEN
    };
    
    virtual Mirror get_mirror() { return mirror; }

protected:
    uint8_t prg_banks;
    uint8_t chr_banks;
    Mirror mirror;
};

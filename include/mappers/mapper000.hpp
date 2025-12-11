#pragma once

#include "../mapper.hpp"

/**
 * Mapper 000 - NROM
 * 
 * Simple mapper with no bank switching
 * PRG ROM: 16KB or 32KB
 * CHR ROM: 8KB
 */
class Mapper000 : public Mapper {
public:
    Mapper000(uint8_t prg_banks, uint8_t chr_banks);
    
    bool cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) override;
    bool cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) override;
    bool ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) override;
    bool ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) override;
    void reset() override;
};

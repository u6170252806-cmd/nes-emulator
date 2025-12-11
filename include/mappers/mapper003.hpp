#pragma once

#include "../mapper.hpp"

/**
 * Mapper 003 - CNROM
 * 
 * Simple CHR bank switching
 * PRG ROM: 16KB or 32KB (fixed)
 * CHR ROM: Up to 2048KB (switchable 8KB banks)
 */
class Mapper003 : public Mapper {
public:
    Mapper003(uint8_t prg_banks, uint8_t chr_banks);
    
    bool cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) override;
    bool cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) override;
    bool ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) override;
    bool ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) override;
    void reset() override;

private:
    uint8_t chr_bank_select;
};

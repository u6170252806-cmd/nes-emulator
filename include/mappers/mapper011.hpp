#pragma once

#include "../mapper.hpp"

/**
 * Mapper 011 - Color Dreams
 * 
 * Used by: Crystal Mines, Bible Adventures
 * PRG ROM: Up to 128KB (switchable 32KB banks)
 * CHR ROM: Up to 128KB (switchable 8KB banks)
 */
class Mapper011 : public Mapper {
public:
    Mapper011(uint8_t prg_banks, uint8_t chr_banks);
    
    bool cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) override;
    bool cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) override;
    bool ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) override;
    bool ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) override;
    void reset() override;

private:
    uint8_t prg_bank_select;
    uint8_t chr_bank_select;
};

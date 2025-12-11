#pragma once

#include "../mapper.hpp"

/**
 * Mapper 002 - UNROM
 * 
 * Simple bank switching mapper
 * PRG ROM: 128KB or 256KB (switchable 16KB banks)
 * CHR ROM: 8KB (fixed)
 */
class Mapper002 : public Mapper {
public:
    Mapper002(uint8_t prg_banks, uint8_t chr_banks);
    
    bool cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) override;
    bool cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) override;
    bool ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) override;
    bool ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) override;
    void reset() override;

private:
    uint8_t prg_bank_select;
    std::vector<uint8_t> chr_ram;
};

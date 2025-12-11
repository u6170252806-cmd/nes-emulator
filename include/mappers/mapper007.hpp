#pragma once

#include "../mapper.hpp"

/**
 * Mapper 007 - AxROM
 * 
 * Used by: Battletoads, Marble Madness
 * PRG ROM: Up to 256KB (switchable 32KB banks)
 * CHR RAM: 8KB (fixed)
 * One-screen mirroring (switchable)
 */
class Mapper007 : public Mapper {
public:
    Mapper007(uint8_t prg_banks, uint8_t chr_banks);
    
    bool cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) override;
    bool cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) override;
    bool ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) override;
    bool ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) override;
    void reset() override;
    Mirror get_mirror() override;

private:
    uint8_t prg_bank_select;
    std::vector<uint8_t> chr_ram;
};

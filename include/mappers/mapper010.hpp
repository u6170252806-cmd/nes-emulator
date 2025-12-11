#pragma once

#include "../mapper.hpp"

/**
 * Mapper 010 - MMC4
 * 
 * Similar to MMC2 with latch-based CHR switching
 * PRG ROM: 256KB
 * CHR ROM: 128KB
 */
class Mapper010 : public Mapper {
public:
    Mapper010(uint8_t prg_banks, uint8_t chr_banks);
    
    bool cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) override;
    bool cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) override;
    bool ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) override;
    bool ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) override;
    void reset() override;
    Mirror get_mirror() override;

private:
    uint8_t prg_bank;
    uint8_t chr_bank_0_fd;
    uint8_t chr_bank_0_fe;
    uint8_t chr_bank_1_fd;
    uint8_t chr_bank_1_fe;
    
    uint8_t latch_0;
    uint8_t latch_1;
};

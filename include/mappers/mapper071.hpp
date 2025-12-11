#pragma once

#include "mapper.hpp"
#include <vector>

/**
 * Mapper 071 - Camerica/Codemasters
 * 
 * GAMES USING THIS MAPPER:
 * - Fire Hawk
 * - Micro Machines
 * - Bee 52
 * - Big Nose Freaks Out
 * - Fantastic Adventures of Dizzy
 * - Linus Spacehead's Cosmic Crusade
 * - Ultimate Stuntman
 * - Quattro Adventure/Arcade/Sports
 * 
 * MEMORY LAYOUT:
 * - PRG ROM: 16KB switchable + 16KB fixed (up to 256KB total)
 * - CHR RAM: 8KB
 * - No PRG RAM
 * 
 * BANK SWITCHING:
 * - $8000-$9FFF: Mirroring control (some variants)
 * - $C000-$FFFF: PRG bank select (16KB at $8000-$BFFF)
 * - Last 16KB bank is fixed at $C000-$FFFF
 * 
 * MIRRORING:
 * - Single-screen mirroring (controlled by register on some variants)
 */
class Mapper071 : public Mapper {
public:
    Mapper071(uint8_t prg_banks, uint8_t chr_banks);
    
    bool cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) override;
    bool cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) override;
    bool ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) override;
    bool ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) override;
    void reset() override;
    Mirror get_mirror() override;

private:
    uint8_t prg_bank_select;  // 16KB PRG bank
    std::vector<uint8_t> chr_ram;
};

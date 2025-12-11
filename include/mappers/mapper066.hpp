#pragma once

#include "mapper.hpp"

/**
 * Mapper 066 - GxROM (GNROM)
 * 
 * GAMES USING THIS MAPPER:
 * - Super Mario Bros. + Duck Hunt
 * - Doraemon
 * - Dragon Power
 * - Gumshoe
 * 
 * MEMORY LAYOUT:
 * - PRG ROM: 32KB banks (up to 128KB total)
 * - CHR ROM: 8KB banks (up to 32KB total)
 * - No PRG RAM
 * 
 * BANK SWITCHING:
 * - Write to $8000-$FFFF selects both PRG and CHR banks
 * - Bits 4-5: PRG bank select (32KB)
 * - Bits 0-1: CHR bank select (8KB)
 * 
 * MIRRORING:
 * - Fixed (set by cartridge hardware)
 */
class Mapper066 : public Mapper {
public:
    Mapper066(uint8_t prg_banks, uint8_t chr_banks);
    
    bool cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) override;
    bool cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) override;
    bool ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) override;
    bool ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) override;
    void reset() override;

private:
    uint8_t prg_bank_select;  // 32KB PRG bank (0-3)
    uint8_t chr_bank_select;  // 8KB CHR bank (0-3)
};

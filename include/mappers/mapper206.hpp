#pragma once

#include "mapper.hpp"

/**
 * Mapper 206 - Namco 108 / MIMIC-1 / DxROM
 * 
 * GAMES USING THIS MAPPER:
 * - Babel no Tou
 * - Dragon Spirit
 * - Gauntlet
 * - Karnov
 * - Mappy-Land
 * - Pac-Land
 * - Rolling Thunder
 * - Sky Kid
 * - Splatterhouse: Wanpaku Graffiti
 * 
 * MEMORY LAYOUT:
 * - PRG ROM: 8KB banks (up to 128KB total)
 * - CHR ROM: 1KB/2KB banks (up to 64KB total)
 * - No PRG RAM
 * 
 * BANK SWITCHING:
 * - Similar to MMC3 but simplified
 * - $8000: Bank select (bits 0-2 select register)
 * - $8001: Bank data
 * - No IRQ support
 * - No mirroring control (fixed by cartridge)
 * 
 * REGISTERS:
 * - R0: 2KB CHR bank at $0000-$07FF
 * - R1: 2KB CHR bank at $0800-$0FFF
 * - R2: 1KB CHR bank at $1000-$13FF
 * - R3: 1KB CHR bank at $1400-$17FF
 * - R4: 1KB CHR bank at $1800-$1BFF
 * - R5: 1KB CHR bank at $1C00-$1FFF
 * - R6: 8KB PRG bank at $8000-$9FFF
 * - R7: 8KB PRG bank at $A000-$BFFF
 * - $C000-$FFFF: Fixed to last 16KB
 */
class Mapper206 : public Mapper {
public:
    Mapper206(uint8_t prg_banks, uint8_t chr_banks);
    
    bool cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) override;
    bool cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) override;
    bool ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) override;
    bool ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) override;
    void reset() override;

private:
    uint8_t target_register;
    uint8_t registers[8];
    uint8_t chr_bank[8];
    uint8_t prg_bank[4];
    
    void update_banks();
};

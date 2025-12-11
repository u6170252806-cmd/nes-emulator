#include "mappers/mapper000.hpp"

// ===== MAPPER 000 - NROM =====
// The simplest NES mapper with no bank switching
// Used by: Super Mario Bros, Donkey Kong, Ice Climber, Balloon Fight, etc.
//
// MEMORY MAP:
// CPU $6000-$7FFF: Family Basic only (not implemented)
// CPU $8000-$BFFF: First 16KB of PRG ROM
// CPU $C000-$FFFF: Last 16KB of PRG ROM (or mirror of $8000-$BFFF if only 16KB)
// PPU $0000-$1FFF: 8KB CHR ROM/RAM
//
// VARIANTS:
// NROM-128: 16KB PRG ROM, mirrored at $C000-$FFFF
// NROM-256: 32KB PRG ROM, no mirroring

Mapper000::Mapper000(uint8_t prg_banks, uint8_t chr_banks) 
    : Mapper(prg_banks, chr_banks) {
}

bool Mapper000::cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) {
    // PRG ROM: $8000-$FFFF
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // Calculate mapped address
        // prg_banks is in 16KB units (1 = 16KB, 2 = 32KB)
        // 
        // For NROM-128 (16KB): Mirror $8000-$BFFF to $C000-$FFFF
        //   addr & 0x3FFF gives offset within 16KB
        //
        // For NROM-256 (32KB): Direct mapping
        //   addr & 0x7FFF gives offset within 32KB
        uint32_t mapped_addr;
        if (prg_banks == 1) {
            // NROM-128: 16KB PRG ROM, mirrored
            mapped_addr = addr & 0x3FFF;
        } else {
            // NROM-256: 32KB PRG ROM
            mapped_addr = addr & 0x7FFF;
        }
        
        if (prg_rom) {
            data = prg_rom[mapped_addr];
        }
        return true;
    }
    return false;
}

bool Mapper000::cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) {
    // NROM has no writable PRG RAM in the standard configuration
    // Some variants have battery-backed RAM at $6000-$7FFF but we don't implement that
    (void)addr;
    (void)data;
    (void)prg_rom;
    return false;
}

bool Mapper000::ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) {
    // CHR ROM/RAM: $0000-$1FFF (8KB)
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        if (chr_rom) {
            data = chr_rom[addr];
        }
        return true;
    }
    return false;
}

bool Mapper000::ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) {
    // CHR ROM is read-only, but CHR RAM is writable
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // chr_banks == 0 means CHR RAM (no CHR ROM on cartridge)
        if (chr_banks == 0 && chr_rom) {
            chr_rom[addr] = data;
            return true;
        }
    }
    return false;
}

void Mapper000::reset() {
    // NROM has no internal state to reset
}

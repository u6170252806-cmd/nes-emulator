#include "mappers/mapper003.hpp"

Mapper003::Mapper003(uint8_t prg_banks, uint8_t chr_banks) 
    : Mapper(prg_banks, chr_banks), chr_bank_select(0) {
}

bool Mapper003::cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // 16KB or 32KB PRG ROM
        uint16_t mapped_addr = addr & (prg_banks > 1 ? 0x7FFF : 0x3FFF);
        data = prg_rom[mapped_addr];
        return true;
    }
    return false;
}

bool Mapper003::cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) {
    (void)prg_rom;  // Unused
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // CHR bank select - mask based on number of CHR banks
        // CNROM can have up to 256 8KB banks (2MB CHR ROM)
        chr_bank_select = data;
        if (chr_banks > 0) {
            chr_bank_select = chr_bank_select % chr_banks;
        }
        return true;
    }
    return false;
}

bool Mapper003::ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        uint32_t mapped_addr = chr_bank_select * 0x2000 + addr;
        if (chr_rom) {
            data = chr_rom[mapped_addr];
        }
        return true;
    }
    return false;
}

bool Mapper003::ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) {
    // CHR ROM is read-only
    return false;
}

void Mapper003::reset() {
    chr_bank_select = 0;
}

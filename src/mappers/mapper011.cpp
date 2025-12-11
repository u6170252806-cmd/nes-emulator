#include "mappers/mapper011.hpp"

Mapper011::Mapper011(uint8_t prg_banks, uint8_t chr_banks) 
    : Mapper(prg_banks, chr_banks), prg_bank_select(0), chr_bank_select(0) {
    reset();
}

void Mapper011::reset() {
    prg_bank_select = 0;
    chr_bank_select = 0;
}

bool Mapper011::cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // 32KB switchable PRG ROM bank
        uint32_t mapped_addr = (prg_bank_select % prg_banks) * 0x8000 + (addr & 0x7FFF);
        if (prg_rom) {
            data = prg_rom[mapped_addr];
        }
        return true;
    }
    return false;
}

bool Mapper011::cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) {
    (void)prg_rom;
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // Bits 0-1: PRG bank select
        prg_bank_select = data & 0x03;
        
        // Bits 4-7: CHR bank select
        chr_bank_select = (data >> 4) & 0x0F;
        
        return true;
    }
    return false;
}

bool Mapper011::ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // 8KB switchable CHR ROM bank
        uint32_t mapped_addr = (chr_bank_select % chr_banks) * 0x2000 + addr;
        if (chr_rom) {
            data = chr_rom[mapped_addr];
        }
        return true;
    }
    return false;
}

bool Mapper011::ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) {
    (void)addr;
    (void)data;
    (void)chr_rom;
    // CHR ROM is read-only
    return false;
}

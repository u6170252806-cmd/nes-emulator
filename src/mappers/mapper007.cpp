#include "mappers/mapper007.hpp"

Mapper007::Mapper007(uint8_t prg_banks, uint8_t chr_banks) 
    : Mapper(prg_banks, chr_banks), prg_bank_select(0) {
    chr_ram.resize(8192, 0);
    reset();
}

void Mapper007::reset() {
    prg_bank_select = 0;
    mirror = Mirror::ONESCREEN_LO;
}

bool Mapper007::cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) {
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

bool Mapper007::cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) {
    (void)prg_rom;
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // Bits 0-2: PRG bank select
        prg_bank_select = data & 0x07;
        
        // Bit 4: One-screen mirroring select
        mirror = (data & 0x10) ? Mirror::ONESCREEN_HI : Mirror::ONESCREEN_LO;
        
        return true;
    }
    return false;
}

bool Mapper007::ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) {
    (void)chr_rom;
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // CHR RAM
        data = chr_ram[addr];
        return true;
    }
    return false;
}

bool Mapper007::ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) {
    (void)chr_rom;
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // CHR RAM
        chr_ram[addr] = data;
        return true;
    }
    return false;
}

Mapper::Mirror Mapper007::get_mirror() {
    return mirror;
}

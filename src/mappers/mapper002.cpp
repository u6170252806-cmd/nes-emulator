#include "mappers/mapper002.hpp"

Mapper002::Mapper002(uint8_t prg_banks, uint8_t chr_banks) 
    : Mapper(prg_banks, chr_banks), prg_bank_select(0) {
    chr_ram.resize(8192, 0);
}

bool Mapper002::cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        // Switchable 16KB PRG ROM bank
        uint32_t mapped_addr = prg_bank_select * 0x4000 + (addr & 0x3FFF);
        data = prg_rom[mapped_addr];
        return true;
    } else if (addr >= 0xC000 && addr <= 0xFFFF) {
        // Fixed 16KB PRG ROM bank (last bank)
        uint32_t mapped_addr = (prg_banks - 1) * 0x4000 + (addr & 0x3FFF);
        data = prg_rom[mapped_addr];
        return true;
    }
    return false;
}

bool Mapper002::cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // Bank select
        prg_bank_select = data & 0x0F;
        return true;
    }
    return false;
}

bool Mapper002::ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        data = chr_ram[addr];
        return true;
    }
    return false;
}

bool Mapper002::ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        chr_ram[addr] = data;
        return true;
    }
    return false;
}

void Mapper002::reset() {
    prg_bank_select = 0;
}

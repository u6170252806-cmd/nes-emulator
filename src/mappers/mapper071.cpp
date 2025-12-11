#include "mappers/mapper071.hpp"

Mapper071::Mapper071(uint8_t prg_banks, uint8_t chr_banks) 
    : Mapper(prg_banks, chr_banks) {
    chr_ram.resize(8192, 0);
    reset();
}

void Mapper071::reset() {
    prg_bank_select = 0;
    mirror = Mirror::ONESCREEN_LO;
}

bool Mapper071::cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        // Switchable 16KB PRG bank
        uint32_t mapped_addr = prg_bank_select * 0x4000 + (addr & 0x3FFF);
        data = prg_rom[mapped_addr];
        return true;
    } else if (addr >= 0xC000 && addr <= 0xFFFF) {
        // Fixed to last 16KB bank
        uint32_t mapped_addr = (prg_banks - 1) * 0x4000 + (addr & 0x3FFF);
        data = prg_rom[mapped_addr];
        return true;
    }
    return false;
}

bool Mapper071::cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) {
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        // Mirroring control (Fire Hawk variant)
        // Bit 4: 0 = one-screen low, 1 = one-screen high
        mirror = (data & 0x10) ? Mirror::ONESCREEN_HI : Mirror::ONESCREEN_LO;
        return true;
    } else if (addr >= 0xC000 && addr <= 0xFFFF) {
        // PRG bank select
        prg_bank_select = data & 0x0F;
        // Wrap to available banks
        if (prg_banks > 0) {
            prg_bank_select %= prg_banks;
        }
        return true;
    }
    return false;
}

bool Mapper071::ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // CHR RAM
        data = chr_ram[addr];
        return true;
    }
    return false;
}

bool Mapper071::ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // CHR RAM
        chr_ram[addr] = data;
        return true;
    }
    return false;
}

Mapper::Mirror Mapper071::get_mirror() {
    return mirror;
}

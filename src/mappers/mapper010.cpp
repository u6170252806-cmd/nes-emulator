#include "mappers/mapper010.hpp"

Mapper010::Mapper010(uint8_t prg_banks, uint8_t chr_banks) 
    : Mapper(prg_banks, chr_banks) {
    reset();
}

void Mapper010::reset() {
    prg_bank = 0;
    chr_bank_0_fd = 0;
    chr_bank_0_fe = 0;
    chr_bank_1_fd = 0;
    chr_bank_1_fe = 0;
    latch_0 = 0xFE;
    latch_1 = 0xFE;
    mirror = Mirror::VERTICAL;
}

bool Mapper010::cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        // Switchable 16KB PRG ROM bank
        uint32_t mapped_addr = prg_bank * 0x4000 + (addr & 0x3FFF);
        data = prg_rom[mapped_addr];
        return true;
    } else if (addr >= 0xC000 && addr <= 0xFFFF) {
        // Fixed 16KB PRG ROM (last bank)
        uint32_t mapped_addr = (prg_banks - 1) * 0x4000 + (addr & 0x3FFF);
        data = prg_rom[mapped_addr];
        return true;
    }
    return false;
}

bool Mapper010::cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) {
    if (addr >= 0xA000 && addr <= 0xAFFF) {
        // PRG ROM bank select
        prg_bank = data & 0x0F;
        return true;
    } else if (addr >= 0xB000 && addr <= 0xBFFF) {
        // CHR ROM $FD/0000 bank select
        chr_bank_0_fd = data & 0x1F;
        return true;
    } else if (addr >= 0xC000 && addr <= 0xCFFF) {
        // CHR ROM $FE/0000 bank select
        chr_bank_0_fe = data & 0x1F;
        return true;
    } else if (addr >= 0xD000 && addr <= 0xDFFF) {
        // CHR ROM $FD/1000 bank select
        chr_bank_1_fd = data & 0x1F;
        return true;
    } else if (addr >= 0xE000 && addr <= 0xEFFF) {
        // CHR ROM $FE/1000 bank select
        chr_bank_1_fe = data & 0x1F;
        return true;
    } else if (addr >= 0xF000 && addr <= 0xFFFF) {
        // Mirroring
        mirror = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
        return true;
    }
    return false;
}

bool Mapper010::ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x0FFF) {
        // Check for latch trigger
        if (addr == 0x0FD8) {
            latch_0 = 0xFD;
        } else if (addr == 0x0FE8) {
            latch_0 = 0xFE;
        }
        
        uint8_t bank = (latch_0 == 0xFD) ? chr_bank_0_fd : chr_bank_0_fe;
        uint32_t mapped_addr = bank * 0x1000 + (addr & 0x0FFF);
        data = chr_rom[mapped_addr];
        return true;
    } else if (addr >= 0x1000 && addr <= 0x1FFF) {
        // Check for latch trigger
        if (addr >= 0x1FD8 && addr <= 0x1FDF) {
            latch_1 = 0xFD;
        } else if (addr >= 0x1FE8 && addr <= 0x1FEF) {
            latch_1 = 0xFE;
        }
        
        uint8_t bank = (latch_1 == 0xFD) ? chr_bank_1_fd : chr_bank_1_fe;
        uint32_t mapped_addr = bank * 0x1000 + (addr & 0x0FFF);
        data = chr_rom[mapped_addr];
        return true;
    }
    return false;
}

bool Mapper010::ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) {
    // CHR ROM is read-only
    return false;
}

Mapper::Mirror Mapper010::get_mirror() {
    return mirror;
}

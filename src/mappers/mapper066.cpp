#include "mappers/mapper066.hpp"

Mapper066::Mapper066(uint8_t prg_banks, uint8_t chr_banks) 
    : Mapper(prg_banks, chr_banks) {
    reset();
}

void Mapper066::reset() {
    prg_bank_select = 0;
    chr_bank_select = 0;
}

bool Mapper066::cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // 32KB PRG bank
        // Each PRG bank is 16KB, so 32KB = 2 banks
        uint32_t bank = prg_bank_select * 2;  // Convert to 16KB bank index
        uint32_t mapped_addr = bank * 0x4000 + (addr & 0x7FFF);
        data = prg_rom[mapped_addr];
        return true;
    }
    return false;
}

bool Mapper066::cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // Bank select register
        // Bits 4-5: PRG bank (32KB)
        // Bits 0-1: CHR bank (8KB)
        prg_bank_select = (data >> 4) & 0x03;
        chr_bank_select = data & 0x03;
        
        // Wrap to available banks
        uint8_t max_prg_32k = prg_banks / 2;  // prg_banks is in 16KB units
        if (max_prg_32k > 0) {
            prg_bank_select %= max_prg_32k;
        }
        if (chr_banks > 0) {
            chr_bank_select %= chr_banks;
        }
        return true;
    }
    return false;
}

bool Mapper066::ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // 8KB CHR bank
        uint32_t mapped_addr = chr_bank_select * 0x2000 + addr;
        data = chr_rom[mapped_addr];
        return true;
    }
    return false;
}

bool Mapper066::ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) {
    // GxROM uses CHR ROM, not RAM - writes are ignored
    return false;
}

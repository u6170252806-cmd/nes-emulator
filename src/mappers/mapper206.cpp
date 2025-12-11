#include "mappers/mapper206.hpp"

Mapper206::Mapper206(uint8_t prg_banks, uint8_t chr_banks) 
    : Mapper(prg_banks, chr_banks) {
    reset();
}

void Mapper206::reset() {
    target_register = 0;
    
    for (int i = 0; i < 8; i++) {
        registers[i] = 0;
        chr_bank[i] = 0;
    }
    
    for (int i = 0; i < 4; i++) {
        prg_bank[i] = 0;
    }
    
    update_banks();
}

void Mapper206::update_banks() {
    // CHR banks (similar to MMC3 but without inversion)
    // R0, R1 are 2KB banks (bits 0-5, bit 0 ignored for 2KB alignment)
    // R2-R5 are 1KB banks (bits 0-5)
    chr_bank[0] = registers[0] & 0x3E;  // 2KB at $0000
    chr_bank[1] = (registers[0] & 0x3E) | 0x01;
    chr_bank[2] = registers[1] & 0x3E;  // 2KB at $0800
    chr_bank[3] = (registers[1] & 0x3E) | 0x01;
    chr_bank[4] = registers[2] & 0x3F;  // 1KB at $1000
    chr_bank[5] = registers[3] & 0x3F;  // 1KB at $1400
    chr_bank[6] = registers[4] & 0x3F;  // 1KB at $1800
    chr_bank[7] = registers[5] & 0x3F;  // 1KB at $1C00
    
    // PRG banks
    // R6: 8KB at $8000
    // R7: 8KB at $A000
    // $C000-$FFFF: Fixed to last 16KB
    prg_bank[0] = registers[6] & 0x0F;
    prg_bank[1] = registers[7] & 0x0F;
    prg_bank[2] = prg_banks * 2 - 2;  // Second-to-last 8KB
    prg_bank[3] = prg_banks * 2 - 1;  // Last 8KB
}

bool Mapper206::cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // PRG ROM - 4 banks of 8KB each
        uint8_t bank_idx = (addr - 0x8000) / 0x2000;
        uint8_t bank = prg_bank[bank_idx];
        
        // Wrap to available PRG ROM
        uint32_t total_8kb_banks = prg_banks * 2;
        if (total_8kb_banks > 0) {
            bank = bank % total_8kb_banks;
        }
        
        uint32_t mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
        data = prg_rom[mapped_addr];
        return true;
    }
    return false;
}

bool Mapper206::cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) {
    if (addr >= 0x8000 && addr <= 0x8001) {
        if (addr & 0x0001) {
            // Bank data ($8001)
            registers[target_register] = data;
            update_banks();
        } else {
            // Bank select ($8000)
            target_register = data & 0x07;
        }
        return true;
    }
    return false;
}

bool Mapper206::ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // CHR ROM - 8 banks of 1KB each
        uint8_t bank_idx = addr / 0x0400;
        uint8_t bank = chr_bank[bank_idx];
        
        // Wrap to available CHR ROM
        if (chr_banks > 0) {
            uint32_t total_1kb_banks = chr_banks * 8;
            bank = bank % total_1kb_banks;
        }
        
        uint32_t mapped_addr = bank * 0x0400 + (addr % 0x0400);
        data = chr_rom[mapped_addr];
        return true;
    }
    return false;
}

bool Mapper206::ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) {
    // Mapper 206 uses CHR ROM, not RAM
    return false;
}

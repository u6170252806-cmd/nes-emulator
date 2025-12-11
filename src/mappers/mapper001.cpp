#include "mappers/mapper001.hpp"

Mapper001::Mapper001(uint8_t prg_banks, uint8_t chr_banks) 
    : Mapper(prg_banks, chr_banks) {
    chr_ram.resize(8192, 0);
    prg_ram.resize(8192, 0);
    reset();
}

void Mapper001::reset() {
    load_register = 0x00;
    load_count = 0;
    control_register = 0x1C;
    chr_bank_0 = 0;
    chr_bank_1 = 0;
    prg_bank = 0;
    mirror = Mirror::HORIZONTAL;
}

bool Mapper001::cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        // PRG RAM
        data = prg_ram[addr & 0x1FFF];
        return true;
    } else if (addr >= 0x8000 && addr <= 0xFFFF) {
        // PRG ROM
        uint8_t prg_mode = (control_register >> 2) & 0x03;
        
        if (prg_mode == 0 || prg_mode == 1) {
            // 32KB mode
            uint32_t bank = (prg_bank >> 1) & 0x0F;
            uint32_t mapped_addr = bank * 0x8000 + (addr & 0x7FFF);
            data = prg_rom[mapped_addr];
        } else if (prg_mode == 2) {
            // Fix first bank, switch second
            if (addr >= 0x8000 && addr <= 0xBFFF) {
                data = prg_rom[addr & 0x3FFF];
            } else {
                uint32_t mapped_addr = (prg_bank & 0x0F) * 0x4000 + (addr & 0x3FFF);
                data = prg_rom[mapped_addr];
            }
        } else if (prg_mode == 3) {
            // Switch first bank, fix last
            if (addr >= 0x8000 && addr <= 0xBFFF) {
                uint32_t mapped_addr = (prg_bank & 0x0F) * 0x4000 + (addr & 0x3FFF);
                data = prg_rom[mapped_addr];
            } else {
                uint32_t mapped_addr = (prg_banks - 1) * 0x4000 + (addr & 0x3FFF);
                data = prg_rom[mapped_addr];
            }
        }
        return true;
    }
    return false;
}

bool Mapper001::cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        // PRG RAM (8KB battery-backed SRAM on many cartridges)
        // Used for save data in games like Zelda, Metroid, Final Fantasy
        prg_ram[addr & 0x1FFF] = data;
        return true;
    } else if (addr >= 0x8000 && addr <= 0xFFFF) {
        // ===== MMC1 SERIAL PORT - UNIQUE WRITE MECHANISM =====
        //
        // HARDWARE DETAILS:
        // - MMC1 uses a 5-bit serial shift register for configuration
        // - Data is written one bit at a time (bit 0 of data byte)
        // - After 5 writes, the register is full and updates internal state
        // - Writing bit 7 = 1 resets the shift register (clears it)
        //
        // WHY SERIAL?
        // - Saves pins on the MMC1 chip (fewer address lines needed)
        // - Allows more complex configuration with simple interface
        // - Common technique in 1980s hardware design
        //
        // TIMING:
        // - Each write takes normal CPU cycles
        // - No special timing requirements
        // - Games typically write all 5 bits in sequence
        
        if (data & 0x80) {
            // RESET: Bit 7 set clears the shift register
            // This is used to:
            // 1. Initialize the mapper on startup
            // 2. Cancel a partial write sequence
            // 3. Reset to known state after errors
            load_register = 0x00;
            load_count = 0;
            // Reset also sets PRG mode to 3 (16KB mode, fix last bank)
            // This ensures the reset vector at $FFFC-$FFFD is always accessible
            control_register |= 0x0C;
        } else {
            // SERIAL WRITE: Shift in one bit (bit 0 of data)
            // The shift register is 5 bits wide
            // Bits are shifted right, new bit enters at bit 4
            load_register >>= 1;
            load_register |= (data & 0x01) << 4;
            load_count++;
            
            if (load_count == 5) {
                // REGISTER UPDATE: After 5 bits, write to internal register
                // The target register is determined by address bits 13-14
                // $8000-$9FFF: Control (mirroring, PRG/CHR modes)
                // $A000-$BFFF: CHR bank 0
                // $C000-$DFFF: CHR bank 1
                // $E000-$FFFF: PRG bank
                uint8_t target = (addr >> 13) & 0x03;
                
                switch (target) {
                    case 0: // $8000-$9FFF: Control register
                        control_register = load_register & 0x1F;
                        // Bits 0-1: Mirroring mode
                        // Bit 2-3: PRG ROM bank mode
                        // Bit 4: CHR ROM bank mode
                        switch (control_register & 0x03) {
                            case 0: mirror = Mirror::ONESCREEN_LO; break;  // One-screen, lower bank
                            case 1: mirror = Mirror::ONESCREEN_HI; break;  // One-screen, upper bank
                            case 2: mirror = Mirror::VERTICAL; break;      // Vertical (horizontal scrolling)
                            case 3: mirror = Mirror::HORIZONTAL; break;    // Horizontal (vertical scrolling)
                        }
                        break;
                    case 1: // $A000-$BFFF: CHR bank 0 (low 4KB)
                        chr_bank_0 = load_register & 0x1F;
                        break;
                    case 2: // $C000-$DFFF: CHR bank 1 (high 4KB)
                        chr_bank_1 = load_register & 0x1F;
                        break;
                    case 3: // $E000-$FFFF: PRG bank select
                        prg_bank = load_register & 0x0F;
                        // Bit 4 can be used for PRG RAM enable/disable (not implemented here)
                        break;
                }
                
                // Clear shift register for next write sequence
                load_register = 0x00;
                load_count = 0;
            }
        }
        return true;
    }
    return false;
}

bool Mapper001::ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        uint8_t chr_mode = (control_register >> 4) & 0x01;
        
        if (chr_banks == 0) {
            // CHR RAM
            data = chr_ram[addr];
        } else {
            if (chr_mode == 0) {
                // 8KB mode
                uint32_t bank = (chr_bank_0 >> 1) & 0x1F;
                uint32_t mapped_addr = bank * 0x2000 + addr;
                data = chr_rom[mapped_addr];
            } else {
                // 4KB mode
                if (addr >= 0x0000 && addr <= 0x0FFF) {
                    uint32_t mapped_addr = chr_bank_0 * 0x1000 + (addr & 0x0FFF);
                    data = chr_rom[mapped_addr];
                } else {
                    uint32_t mapped_addr = chr_bank_1 * 0x1000 + (addr & 0x0FFF);
                    data = chr_rom[mapped_addr];
                }
            }
        }
        return true;
    }
    return false;
}

bool Mapper001::ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        if (chr_banks == 0) {
            // CHR RAM
            chr_ram[addr] = data;
            return true;
        }
    }
    return false;
}

Mapper::Mirror Mapper001::get_mirror() {
    return mirror;
}

#include "mappers/mapper004.hpp"

Mapper004::Mapper004(uint8_t prg_banks, uint8_t chr_banks) 
    : Mapper(prg_banks, chr_banks) {
    prg_ram.resize(8192, 0);
    reset();
}

void Mapper004::reset() {
    target_register = 0x00;
    prg_bank_mode = false;
    chr_inversion = false;
    
    for (int i = 0; i < 8; i++) {
        registers[i] = 0;
        chr_bank[i] = 0;
    }
    
    for (int i = 0; i < 4; i++) {
        prg_bank[i] = 0;
    }
    
    irq_counter = 0x00;
    irq_latch = 0x00;
    irq_enable = false;
    irq_active = false;
    irq_reload = false;
    
    mirror = Mirror::HORIZONTAL;
    
    update_banks();
}

void Mapper004::update_banks() {
    // ===== MMC3 BANK SWITCHING - COMPREHENSIVE IMPLEMENTATION =====
    //
    // MEMORY LAYOUT:
    // - PRG ROM: 4 banks of 8KB each ($8000-$9FFF, $A000-$BFFF, $C000-$DFFF, $E000-$FFFF)
    // - CHR ROM: 8 banks of 1KB each ($0000-$03FF, $0400-$07FF, ..., $1C00-$1FFF)
    //
    // BANK REGISTERS:
    // R0, R1: 2KB CHR banks (used as pairs for 2KB switching)
    // R2-R5: 1KB CHR banks (for fine-grained control)
    // R6, R7: 8KB PRG banks (switchable)
    //
    // INVERSION MODES:
    // - CHR inversion (bit 7 of bank select): Swaps which registers control which CHR areas
    // - PRG bank mode (bit 6 of bank select): Changes which PRG banks are fixed/switchable
    
    // ===== CHR BANK CONFIGURATION =====
    // CHR ROM is divided into 8 banks of 1KB each
    // Registers R0-R1 control 2KB regions (2 banks each)
    // Registers R2-R5 control 1KB regions (1 bank each)
    
    if (!chr_inversion) {
        // NORMAL MODE: R0-R1 control $0000-$0FFF, R2-R5 control $1000-$1FFF
        // This is the default mode used by most games
        // Good for: Background tiles in $0000-$0FFF, sprites in $1000-$1FFF
        
        chr_bank[0] = registers[0] & 0xFE;  // R0 controls $0000-$07FF (2KB, even bank)
        chr_bank[1] = registers[0] | 0x01;  // R0 controls $0000-$07FF (2KB, odd bank)
        chr_bank[2] = registers[1] & 0xFE;  // R1 controls $0800-$0FFF (2KB, even bank)
        chr_bank[3] = registers[1] | 0x01;  // R1 controls $0800-$0FFF (2KB, odd bank)
        chr_bank[4] = registers[2];         // R2 controls $1000-$13FF (1KB)
        chr_bank[5] = registers[3];         // R3 controls $1400-$17FF (1KB)
        chr_bank[6] = registers[4];         // R4 controls $1800-$1BFF (1KB)
        chr_bank[7] = registers[5];         // R5 controls $1C00-$1FFF (1KB)
    } else {
        // INVERTED MODE: R2-R5 control $0000-$0FFF, R0-R1 control $1000-$1FFF
        // Used by some games for special effects or different tile arrangements
        // Good for: Fine-grained control of background tiles, sprite animations
        
        chr_bank[0] = registers[2];         // R2 controls $0000-$03FF (1KB)
        chr_bank[1] = registers[3];         // R3 controls $0400-$07FF (1KB)
        chr_bank[2] = registers[4];         // R4 controls $0800-$0BFF (1KB)
        chr_bank[3] = registers[5];         // R5 controls $0C00-$0FFF (1KB)
        chr_bank[4] = registers[0] & 0xFE;  // R0 controls $1000-$17FF (2KB, even bank)
        chr_bank[5] = registers[0] | 0x01;  // R0 controls $1000-$17FF (2KB, odd bank)
        chr_bank[6] = registers[1] & 0xFE;  // R1 controls $1800-$1FFF (2KB, even bank)
        chr_bank[7] = registers[1] | 0x01;  // R1 controls $1800-$1FFF (2KB, odd bank)
    }
    
    // ===== PRG BANK CONFIGURATION =====
    // PRG ROM is divided into 4 banks of 8KB each
    // R6, R7: Switchable 8KB banks
    // Last 2 banks: One is fixed to second-to-last, one is fixed to last
    // Bank mode determines which banks are switchable vs fixed
    
    if (!prg_bank_mode) {
        // NORMAL MODE: $8000 and $A000 are switchable, $C000 and $E000 are fixed
        // This is the default mode used by most games
        // Good for: Code in fixed banks, data/levels in switchable banks
        
        prg_bank[0] = registers[6];         // $8000-$9FFF: Switchable (R6)
        prg_bank[1] = registers[7];         // $A000-$BFFF: Switchable (R7)
        prg_bank[2] = prg_banks * 2 - 2;    // $C000-$DFFF: Fixed to second-to-last bank
        prg_bank[3] = prg_banks * 2 - 1;    // $E000-$FFFF: Fixed to last bank (vectors here!)
    } else {
        // SWAPPED MODE: $8000 is fixed, $A000 and $C000 are switchable, $E000 is fixed
        // Used by some games for different code organization
        // Good for: Keeping interrupt handlers in $8000, more switchable space
        
        prg_bank[0] = prg_banks * 2 - 2;    // $8000-$9FFF: Fixed to second-to-last bank
        prg_bank[1] = registers[7];         // $A000-$BFFF: Switchable (R7)
        prg_bank[2] = registers[6];         // $C000-$DFFF: Switchable (R6)
        prg_bank[3] = prg_banks * 2 - 1;    // $E000-$FFFF: Fixed to last bank (vectors here!)
    }
}

bool Mapper004::cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        // PRG RAM (8KB)
        data = prg_ram[addr & 0x1FFF];
        return true;
    } else if (addr >= 0x8000 && addr <= 0xFFFF) {
        // PRG ROM - 4 banks of 8KB each
        uint8_t bank_idx = (addr - 0x8000) / 0x2000;  // Which 8KB bank (0-3)
        uint8_t bank = prg_bank[bank_idx];
        
        // Wrap to available PRG ROM (each PRG bank is 16KB = 2 * 8KB)
        uint32_t total_8kb_banks = prg_banks * 2;
        bank = bank % total_8kb_banks;
        
        uint32_t mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
        if (prg_rom) {
            data = prg_rom[mapped_addr];
        }
        return true;
    }
    return false;
}

bool Mapper004::cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        // PRG RAM
        prg_ram[addr & 0x1FFF] = data;
        return true;
    } else if (addr >= 0x8000 && addr <= 0x9FFF) {
        if (addr & 0x0001) {
            // Bank data
            registers[target_register] = data;
            update_banks();
        } else {
            // Bank select
            target_register = data & 0x07;
            prg_bank_mode = (data & 0x40) != 0;
            chr_inversion = (data & 0x80) != 0;
            update_banks();
        }
        return true;
    } else if (addr >= 0xA000 && addr <= 0xBFFF) {
        if (addr & 0x0001) {
            // PRG RAM protect (not implemented)
        } else {
            // Mirroring
            mirror = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
        }
        return true;
    } else if (addr >= 0xC000 && addr <= 0xDFFF) {
        if (addr & 0x0001) {
            // IRQ reload
            irq_reload = true;
        } else {
            // IRQ latch
            irq_latch = data;
        }
        return true;
    } else if (addr >= 0xE000 && addr <= 0xFFFF) {
        if (addr & 0x0001) {
            // IRQ enable
            irq_enable = true;
        } else {
            // IRQ disable
            irq_enable = false;
            irq_active = false;
        }
        return true;
    }
    return false;
}

bool Mapper004::ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // MMC3 uses 1KB CHR banks
        uint8_t bank_idx = addr / 0x0400;  // Which 1KB bank (0-7)
        uint8_t bank = chr_bank[bank_idx];
        
        // Wrap bank number to available CHR ROM
        if (chr_banks > 0) {
            uint32_t total_1kb_banks = chr_banks * 8;  // Each CHR bank is 8KB = 8 * 1KB
            bank = bank % total_1kb_banks;
        }
        
        uint32_t mapped_addr = bank * 0x0400 + (addr % 0x0400);
        if (chr_rom) {
            data = chr_rom[mapped_addr];
        }
        return true;
    }
    return false;
}

bool Mapper004::ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        if (chr_banks == 0) {
            // CHR RAM
            uint8_t bank = chr_bank[addr / 0x0400];
            uint32_t mapped_addr = bank * 0x0400 + (addr % 0x0400);
            chr_rom[mapped_addr] = data;
            return true;
        }
    }
    return false;
}

bool Mapper004::irq_state() {
    return irq_active;
}

void Mapper004::irq_clear() {
    irq_active = false;
}

void Mapper004::scanline() {
    // ===== MMC3 IRQ COUNTER - CYCLE ACCURATE BEHAVIOR =====
    // 
    // HARDWARE DETAILS:
    // - The MMC3 has a scanline counter that triggers IRQs
    // - Counter decrements on A12 rising edge (0->1 transition)
    // - A12 is connected to PPU address bus bit 12
    // - A12 goes high when PPU fetches from pattern tables ($1000-$1FFF)
    // - This happens during sprite/background tile fetching
    // 
    // TIMING:
    // - Counter is clocked once per scanline (during rendering)
    // - Typical games use this for split-screen effects (status bars, etc.)
    // - IRQ fires at specific scanline for precise timing
    // 
    // COUNTER BEHAVIOR:
    // 1. If reload flag is set OR counter is 0, reload from latch
    // 2. Otherwise, decrement counter
    // 3. If counter becomes 0 AND IRQ enabled, trigger IRQ
    // 
    // EDGE CASES:
    // - Writing to reload register sets reload flag
    // - Counter reloads on NEXT clock after reaching 0
    // - IRQ fires when counter transitions 1->0, not when it's 0
    
    if (irq_reload || irq_counter == 0) {
        // Reload counter from latch
        // This happens when:
        // 1. Game writes to $C001 (sets reload flag)
        // 2. Counter has counted down to 0
        irq_counter = irq_latch;
        irq_reload = false;
    } else {
        // Decrement counter
        irq_counter--;
    }
    
    // Trigger IRQ when counter reaches 0 (transition from 1->0)
    // Only if IRQ is enabled via $E001 write
    if (irq_counter == 0 && irq_enable) {
        irq_active = true;
    }
}

Mapper::Mirror Mapper004::get_mirror() {
    return mirror;
}

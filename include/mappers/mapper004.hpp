#pragma once

#include "../mapper.hpp"

/**
 * Mapper 004 - MMC3
 * 
 * Advanced mapper with IRQ counter
 * PRG ROM: Up to 512KB
 * CHR ROM: Up to 256KB
 * PRG RAM: 8KB
 */
class Mapper004 : public Mapper {
public:
    Mapper004(uint8_t prg_banks, uint8_t chr_banks);
    
    bool cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) override;
    bool cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) override;
    bool ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) override;
    bool ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) override;
    void reset() override;
    Mirror get_mirror() override;
    
    bool irq_state() override;
    void irq_clear() override;
    void scanline() override;

private:
    uint8_t target_register;
    bool prg_bank_mode;
    bool chr_inversion;
    
    uint8_t registers[8];
    uint8_t chr_bank[8];
    uint8_t prg_bank[4];
    
    std::vector<uint8_t> prg_ram;
    
    // IRQ counter
    uint8_t irq_counter;
    uint8_t irq_latch;
    bool irq_enable;
    bool irq_active;
    bool irq_reload;
    
    void update_banks();
};

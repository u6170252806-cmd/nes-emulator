#pragma once

#include "../mapper.hpp"

/**
 * Mapper 001 - MMC1
 * 
 * Complex mapper with serial port for configuration
 * PRG ROM: Up to 512KB (32 banks)
 * CHR ROM: Up to 128KB (32 banks)
 * PRG RAM: 8KB or 32KB
 */
class Mapper001 : public Mapper {
public:
    Mapper001(uint8_t prg_banks, uint8_t chr_banks);
    
    bool cpu_read(uint16_t addr, uint8_t& data, uint8_t* prg_rom) override;
    bool cpu_write(uint16_t addr, uint8_t data, uint8_t* prg_rom) override;
    bool ppu_read(uint16_t addr, uint8_t& data, uint8_t* chr_rom) override;
    bool ppu_write(uint16_t addr, uint8_t data, uint8_t* chr_rom) override;
    void reset() override;
    Mirror get_mirror() override;

private:
    uint8_t load_register;
    uint8_t load_count;
    uint8_t control_register;
    uint8_t chr_bank_0;
    uint8_t chr_bank_1;
    uint8_t prg_bank;
    
    std::vector<uint8_t> chr_ram;
    std::vector<uint8_t> prg_ram;
};

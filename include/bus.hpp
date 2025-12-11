#pragma once

#include <cstdint>
#include <array>
#include <memory>

// Forward declarations
class CPU;
class PPU;
class APU;
class Cartridge;

/**
 * System Bus - Connects all components
 * 
 * CPU Memory Map:
 * $0000-$07FF: 2KB internal RAM
 * $0800-$1FFF: Mirrors of $0000-$07FF
 * $2000-$2007: PPU registers
 * $2008-$3FFF: Mirrors of $2000-$2007
 * $4000-$4017: APU and I/O registers
 * $4018-$401F: APU and I/O test mode
 * $4020-$FFFF: Cartridge space (PRG ROM, PRG RAM, mapper registers)
 */
class Bus {
public:
    Bus();
    ~Bus();
    
    // Connect components
    void connect_cpu(CPU* cpu_ptr);
    void connect_ppu(PPU* ppu_ptr);
    void connect_apu(APU* apu_ptr);
    void insert_cartridge(std::shared_ptr<Cartridge> cart);
    
    // CPU memory access
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t data);
    
    // System clock - advances all components
    void clock();
    
    // Reset system
    void reset();
    
    // Controller input
    void set_controller_state(uint8_t controller, uint8_t state);
    
    // Get components (for debugging)
    CPU* get_cpu() { return cpu; }
    PPU* get_ppu() { return ppu; }
    APU* get_apu() { return apu; }
    Cartridge* get_cartridge() { return cartridge.get(); }

private:
    // Components
    CPU* cpu;
    PPU* ppu;
    APU* apu;
    std::shared_ptr<Cartridge> cartridge;
    
    // 2KB internal RAM
    std::array<uint8_t, 2048> ram;
    
    // Controllers
    uint8_t controller_state[2];
    uint8_t controller_shift[2];
    
    // System clock counter
    uint64_t system_clock_counter;
    
    // DMA (Direct Memory Access)
    bool dma_transfer;
    uint8_t dma_page;
    uint8_t dma_addr;
    uint8_t dma_data;
    bool dma_dummy;  // DMA needs dummy cycle on odd CPU cycles
    
    // DMC DMA
    static uint8_t dmc_read_callback(uint16_t addr);
    static Bus* dmc_bus_instance;
};

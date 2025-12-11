#include "bus.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "cartridge.hpp"
#include <cstring>

Bus* Bus::dmc_bus_instance = nullptr;

Bus::Bus() : cpu(nullptr), ppu(nullptr), apu(nullptr), system_clock_counter(0) {
    ram.fill(0x00);
    controller_state[0] = 0x00;
    controller_state[1] = 0x00;
    controller_shift[0] = 0x00;
    controller_shift[1] = 0x00;
    dma_transfer = false;
    dma_page = 0x00;
    dma_addr = 0x00;
    dma_data = 0x00;
    dma_dummy = true;
    
    dmc_bus_instance = this;
}

Bus::~Bus() {
    dmc_bus_instance = nullptr;
}

void Bus::connect_cpu(CPU* cpu_ptr) {
    cpu = cpu_ptr;
}

void Bus::connect_ppu(PPU* ppu_ptr) {
    ppu = ppu_ptr;
}

void Bus::connect_apu(APU* apu_ptr) {
    apu = apu_ptr;
    if (apu) {
        apu->set_dmc_read_callback(&Bus::dmc_read_callback);
    }
}

void Bus::insert_cartridge(std::shared_ptr<Cartridge> cart) {
    cartridge = cart;
    if (ppu) {
        ppu->connect_cartridge(cart.get());
    }
}

void Bus::reset() {
    if (cpu) cpu->reset();
    if (ppu) ppu->reset();
    if (apu) apu->reset();
    system_clock_counter = 0;
    dma_transfer = false;
}

uint8_t Bus::cpu_read(uint16_t addr) {
    uint8_t data = 0x00;
    
    if (cartridge && cartridge->cpu_read(addr, data)) {
        // Cartridge handled the read
    } else if (addr >= 0x0000 && addr <= 0x1FFF) {
        // 2KB internal RAM, mirrored 4 times
        data = ram[addr & 0x07FF];
    } else if (addr >= 0x2000 && addr <= 0x3FFF) {
        // PPU registers, mirrored every 8 bytes
        if (ppu) {
            data = ppu->cpu_read(addr & 0x0007);
        }
    } else if (addr >= 0x4000 && addr <= 0x4015) {
        // APU registers
        if (apu) {
            data = apu->cpu_read(addr);
        }
    } else if (addr == 0x4016) {
        // Controller 1
        data = (controller_shift[0] & 0x80) > 0;
        controller_shift[0] <<= 1;
    } else if (addr == 0x4017) {
        // Controller 2
        data = (controller_shift[1] & 0x80) > 0;
        controller_shift[1] <<= 1;
    }
    
    return data;
}

void Bus::cpu_write(uint16_t addr, uint8_t data) {
    if (cartridge && cartridge->cpu_write(addr, data)) {
        // Cartridge handled the write
    } else if (addr >= 0x0000 && addr <= 0x1FFF) {
        // 2KB internal RAM, mirrored 4 times
        ram[addr & 0x07FF] = data;
    } else if (addr >= 0x2000 && addr <= 0x3FFF) {
        // PPU registers, mirrored every 8 bytes
        if (ppu) {
            ppu->cpu_write(addr & 0x0007, data);
        }
    } else if ((addr >= 0x4000 && addr <= 0x4013) || addr == 0x4015 || addr == 0x4017) {
        // APU registers
        if (apu) {
            apu->cpu_write(addr, data);
        }
    } else if (addr == 0x4014) {
        // OAM DMA
        dma_page = data;
        dma_addr = 0x00;
        dma_transfer = true;
    } else if (addr == 0x4016) {
        // Controller strobe
        controller_shift[0] = controller_state[0];
        controller_shift[1] = controller_state[1];
    }
}

void Bus::clock() {
    // PPU runs 3x faster than CPU
    if (ppu) {
        ppu->clock();
    }
    
    // CPU runs every 3 PPU cycles
    if (system_clock_counter % 3 == 0) {
        // Handle DMA
        if (dma_transfer) {
            // DMA takes 513 or 514 cycles (depending on odd/even CPU cycle)
            if (dma_dummy) {
                // Wait for alignment
                if (system_clock_counter % 2 == 1) {
                    dma_dummy = false;
                }
            } else {
                // Transfer data
                if (system_clock_counter % 2 == 0) {
                    // Read cycle
                    dma_data = cpu_read((dma_page << 8) | dma_addr);
                } else {
                    // Write cycle - write directly to OAM
                    if (ppu) {
                        // Write to OAM at current address
                        ppu->cpu_write(0x0004, dma_data);
                    }
                    dma_addr++;
                    
                    if (dma_addr == 0x00) {
                        // DMA complete (256 bytes transferred)
                        dma_transfer = false;
                        dma_dummy = true;
                    }
                }
            }
        } else {
            // Normal CPU operation
            if (cpu) {
                cpu->clock();
            }
        }
        
        // APU runs at CPU speed
        if (apu) {
            apu->clock();
        }
    }
    
    // Check for NMI from PPU
    if (ppu && ppu->nmi) {
        ppu->nmi = false;
        if (cpu) {
            cpu->nmi();
        }
    }
    
    // Check for IRQ from cartridge (mapper)
    if (cartridge && cartridge->irq_state()) {
        cartridge->irq_clear();
        if (cpu) {
            cpu->irq();
        }
    }
    
    system_clock_counter++;
}

void Bus::set_controller_state(uint8_t controller, uint8_t state) {
    if (controller < 2) {
        controller_state[controller] = state;
    }
}

uint8_t Bus::dmc_read_callback(uint16_t addr) {
    if (dmc_bus_instance) {
        return dmc_bus_instance->cpu_read(addr);
    }
    return 0x00;
}

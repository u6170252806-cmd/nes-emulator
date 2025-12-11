#include "emulator.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "bus.hpp"
#include "cartridge.hpp"
#include <iostream>

Emulator::Emulator() : residual_time(0.0) {
    bus = std::make_unique<Bus>();
    cpu = std::make_unique<CPU>(bus.get());
    ppu = std::make_unique<PPU>(bus.get());
    apu = std::make_unique<APU>();
    
    bus->connect_cpu(cpu.get());
    bus->connect_ppu(ppu.get());
    bus->connect_apu(apu.get());
}

Emulator::~Emulator() = default;

bool Emulator::load_rom(const std::string& filename) {
    cartridge = std::make_shared<Cartridge>(filename);
    
    if (!cartridge->is_valid()) {
        std::cerr << "Failed to load ROM or unsupported format" << std::endl;
        cartridge.reset();
        return false;
    }
    
    std::cout << "ROM loaded successfully. Mapper: " << (int)cartridge->get_mapper_id() << std::endl;
    
    bus->insert_cartridge(cartridge);
    reset();
    
    return true;
}

void Emulator::reset() {
    bus->reset();
    residual_time = 0.0;
}

bool Emulator::run_frame() {
    // Run until frame is complete
    while (!ppu->frame_complete()) {
        bus->clock();
    }
    
    return true;
}

const uint8_t* Emulator::get_screen() const {
    return ppu->get_screen();
}

float Emulator::get_audio_sample() {
    return apu->get_output_sample();
}

void Emulator::set_controller_state(uint8_t controller, uint8_t state) {
    bus->set_controller_state(controller, state);
}

CPU* Emulator::get_cpu() {
    return cpu.get();
}

PPU* Emulator::get_ppu() {
    return ppu.get();
}

APU* Emulator::get_apu() {
    return apu.get();
}

Bus* Emulator::get_bus() {
    return bus.get();
}

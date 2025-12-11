#include "cartridge.hpp"
#include "mapper.hpp"
#include "mappers/mapper000.hpp"
#include "mappers/mapper001.hpp"
#include "mappers/mapper002.hpp"
#include "mappers/mapper003.hpp"
#include "mappers/mapper004.hpp"
#include "mappers/mapper007.hpp"
#include "mappers/mapper009.hpp"
#include "mappers/mapper010.hpp"
#include "mappers/mapper011.hpp"
#include "mappers/mapper066.hpp"
#include "mappers/mapper071.hpp"
#include "mappers/mapper206.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <algorithm>

// ============================================================================
// CARTRIDGE - NES ROM LOADER AND MAPPER INTERFACE
// ============================================================================
// 
// NES CARTRIDGE FORMATS:
// 1. iNES (most common): 16-byte header + optional trainer + PRG ROM + CHR ROM
// 2. NES 2.0: Extended iNES with more metadata
// 3. UNIF: Unified NES Image Format (less common)
// 
// iNES HEADER FORMAT (16 bytes):
// Bytes 0-3: "NES" + $1A (magic number)
// Byte 4: PRG ROM size in 16KB units
// Byte 5: CHR ROM size in 8KB units (0 = CHR RAM)
// Byte 6: Flags 6 (mapper low nibble, mirroring, battery, trainer)
// Byte 7: Flags 7 (mapper high nibble, VS/Playchoice, NES 2.0 identifier)
// Byte 8: PRG RAM size (rarely used in iNES 1.0)
// Byte 9: TV system (rarely used)
// Bytes 10-15: Unused padding (should be zero)
// 
// MAPPER SUPPORT:
// The NES uses memory mappers to extend the 64KB address space
// Different mappers provide different bank switching capabilities
// This emulator supports the most common mappers covering ~90% of games

Cartridge::Cartridge(const std::string& filename) : valid(false) {
    std::cout << "Loading ROM: " << filename << std::endl;
    
    // ===== FILE LOADING =====
    // Open file in binary mode to read raw bytes
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << filename << std::endl;
        std::cerr << "  - Check if the file exists" << std::endl;
        std::cerr << "  - Check file permissions" << std::endl;
        std::cerr << "  - Check for special characters in filename" << std::endl;
        return;
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    std::streampos file_size_pos = file.tellg();
    if (file_size_pos < 0) {
        std::cerr << "Error: Cannot determine file size" << std::endl;
        return;
    }
    size_t file_size = static_cast<size_t>(file_size_pos);
    file.seekg(0, std::ios::beg);
    
    // Validate minimum file size (header + some data)
    if (file_size < 16) {
        std::cerr << "Error: File too small (" << file_size << " bytes)" << std::endl;
        std::cerr << "  - Minimum NES ROM size is 16 bytes (header only)" << std::endl;
        return;
    }
    
    // Read entire file into memory
    std::vector<uint8_t> data(file_size);
    file.read(reinterpret_cast<char*>(data.data()), file_size);
    
    if (!file) {
        std::cerr << "Error: Failed to read file (read " << file.gcount() << " of " << file_size << " bytes)" << std::endl;
        return;
    }
    file.close();
    
    std::cout << "File size: " << file_size << " bytes" << std::endl;
    
    // ===== HEADER VALIDATION =====
    // Check for "NES" + MS-DOS EOF marker ($1A)
    if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) {
        std::cerr << "Error: Invalid NES header magic" << std::endl;
        std::cerr << "  - Expected: 4E 45 53 1A (NES\\x1A)" << std::endl;
        std::cerr << "  - Got: " << std::hex 
                  << (int)data[0] << " " << (int)data[1] << " " 
                  << (int)data[2] << " " << (int)data[3] << std::dec << std::endl;
        return;
    }
    
    // ===== FORMAT DETECTION =====
    // Check for NES 2.0 format (bits 2-3 of byte 7 == 0b10)
    if ((data[7] & 0x0C) == 0x08) {
        std::cout << "Detected NES 2.0 format" << std::endl;
        valid = load_nes2(data);
    } else {
        // Check for dirty headers (bytes 12-15 should be zero in clean iNES)
        bool dirty_header = false;
        for (int i = 12; i < 16; i++) {
            if (data[i] != 0) {
                dirty_header = true;
                break;
            }
        }
        if (dirty_header) {
            std::cout << "Warning: Dirty iNES header detected (bytes 12-15 not zero)" << std::endl;
            std::cout << "  - Ignoring upper mapper bits to avoid misdetection" << std::endl;
        }
        valid = load_ines(data, dirty_header);
    }
    
    if (valid) {
        create_mapper();
    }
}

Cartridge::~Cartridge() = default;

bool Cartridge::load_ines(const std::vector<uint8_t>& data, bool dirty_header) {
    // ===== iNES HEADER PARSING =====
    prg_banks = data[4];
    chr_banks = data[5];
    
    // Mapper number from flags 6 and 7
    uint8_t mapper1 = (data[6] >> 4);
    uint8_t mapper2 = dirty_header ? 0 : (data[7] >> 4);  // Ignore if dirty header
    mapper_id = mapper1 | (mapper2 << 4);
    
    std::cout << "PRG ROM: " << (int)prg_banks << " x 16KB = " << (prg_banks * 16) << "KB" << std::endl;
    std::cout << "CHR " << (chr_banks == 0 ? "RAM" : "ROM") << ": " 
              << (chr_banks == 0 ? 8 : chr_banks * 8) << "KB" << std::endl;
    std::cout << "Mapper: " << (int)mapper_id << std::endl;
    
    // ===== MIRRORING =====
    // Bit 0 of flags 6: 0 = horizontal, 1 = vertical
    // Bit 3 of flags 6: 1 = four-screen VRAM (overrides bit 0)
    if (data[6] & 0x08) {
        mirror_mode = Mirror::FOUR_SCREEN;
        std::cout << "Mirroring: Four-screen" << std::endl;
    } else {
        mirror_mode = (data[6] & 0x01) ? Mirror::VERTICAL : Mirror::HORIZONTAL;
        std::cout << "Mirroring: " << ((data[6] & 0x01) ? "Vertical" : "Horizontal") << std::endl;
    }
    
    // Battery-backed RAM (for save games)
    battery_backed = (data[6] & 0x02) != 0;
    if (battery_backed) {
        std::cout << "Battery-backed RAM: Yes (save support)" << std::endl;
    }
    
    // ===== TRAINER =====
    // 512-byte trainer data (rarely used, for cheat devices)
    size_t offset = 16;
    bool has_trainer = (data[6] & 0x04) != 0;
    if (has_trainer) {
        offset += 512;
        std::cout << "Trainer: Present (512 bytes)" << std::endl;
    }
    
    // ===== PRG ROM =====
    size_t prg_size = prg_banks * 16384;
    if (offset + prg_size > data.size()) {
        std::cerr << "Error: PRG ROM size (" << prg_size << " bytes) exceeds file size" << std::endl;
        std::cerr << "  - File has " << (data.size() - offset) << " bytes available" << std::endl;
        return false;
    }
    prg_rom.resize(prg_size);
    std::memcpy(prg_rom.data(), &data[offset], prg_size);
    offset += prg_size;
    
    // ===== CHR ROM/RAM =====
    if (chr_banks == 0) {
        // CHR RAM (8KB default, some games use more)
        chr_rom.resize(8192, 0);
        std::cout << "Using 8KB CHR RAM" << std::endl;
    } else {
        size_t chr_size = chr_banks * 8192;
        if (offset + chr_size > data.size()) {
            // Some ROMs have truncated CHR data - use what's available
            std::cerr << "Warning: CHR ROM truncated (expected " << chr_size 
                      << " bytes, got " << (data.size() - offset) << ")" << std::endl;
            chr_size = data.size() - offset;
        }
        chr_rom.resize(chr_size);
        if (chr_size > 0) {
            std::memcpy(chr_rom.data(), &data[offset], chr_size);
        }
    }
    
    // ===== PRG RAM =====
    // Default 8KB, some mappers support more
    prg_ram.resize(8192, 0);
    
    return true;
}

bool Cartridge::load_nes2(const std::vector<uint8_t>& data) {
    // ===== NES 2.0 HEADER PARSING =====
    // NES 2.0 extends iNES with more accurate metadata
    
    // PRG ROM size (can be larger than iNES)
    uint16_t prg_rom_lsb = data[4];
    uint16_t prg_rom_msb = data[9] & 0x0F;
    if (prg_rom_msb == 0x0F) {
        // Exponent-multiplier notation (rare)
        uint8_t exponent = (prg_rom_lsb >> 2) & 0x3F;
        uint8_t multiplier = prg_rom_lsb & 0x03;
        prg_banks = ((1 << exponent) * (multiplier * 2 + 1)) / 16384;
    } else {
        prg_banks = prg_rom_lsb | (prg_rom_msb << 8);
    }
    
    // CHR ROM size
    uint16_t chr_rom_lsb = data[5];
    uint16_t chr_rom_msb = (data[9] >> 4) & 0x0F;
    if (chr_rom_msb == 0x0F) {
        uint8_t exponent = (chr_rom_lsb >> 2) & 0x3F;
        uint8_t multiplier = chr_rom_lsb & 0x03;
        chr_banks = ((1 << exponent) * (multiplier * 2 + 1)) / 8192;
    } else {
        chr_banks = chr_rom_lsb | (chr_rom_msb << 8);
    }
    
    // Mapper number (12 bits in NES 2.0)
    uint8_t mapper_lo = (data[6] >> 4);
    uint8_t mapper_mid = (data[7] >> 4);
    uint8_t mapper_hi = data[8] & 0x0F;
    mapper_id = mapper_lo | (mapper_mid << 4) | (mapper_hi << 8);
    
    // Submapper (for variants of the same mapper)
    uint8_t submapper = (data[8] >> 4) & 0x0F;
    
    std::cout << "NES 2.0 ROM detected" << std::endl;
    std::cout << "PRG ROM: " << (prg_banks * 16) << "KB" << std::endl;
    std::cout << "CHR " << (chr_banks == 0 ? "RAM" : "ROM") << ": " 
              << (chr_banks == 0 ? 8 : chr_banks * 8) << "KB" << std::endl;
    std::cout << "Mapper: " << mapper_id;
    if (submapper > 0) {
        std::cout << " (submapper " << (int)submapper << ")";
    }
    std::cout << std::endl;
    
    // Use iNES loader for the rest (compatible)
    return load_ines(data, false);
}

void Cartridge::create_mapper() {
    // ===== MAPPER CREATION =====
    // Create the appropriate mapper based on mapper ID
    // Each mapper handles bank switching differently
    
    switch (mapper_id) {
        case 0:
            mapper = std::make_unique<Mapper000>(prg_banks, chr_banks);
            std::cout << "Using Mapper 0 (NROM) - No bank switching" << std::endl;
            break;
            
        case 1:
            mapper = std::make_unique<Mapper001>(prg_banks, chr_banks);
            std::cout << "Using Mapper 1 (MMC1) - Nintendo MMC1" << std::endl;
            std::cout << "  Games: Zelda, Metroid, Final Fantasy, Mega Man 2" << std::endl;
            break;
            
        case 2:
            mapper = std::make_unique<Mapper002>(prg_banks, chr_banks);
            std::cout << "Using Mapper 2 (UxROM) - PRG bank switching" << std::endl;
            std::cout << "  Games: Mega Man, Castlevania, Contra, Duck Tales" << std::endl;
            break;
            
        case 3:
            mapper = std::make_unique<Mapper003>(prg_banks, chr_banks);
            std::cout << "Using Mapper 3 (CNROM) - CHR bank switching" << std::endl;
            std::cout << "  Games: Gradius, Arkanoid, Solomon's Key" << std::endl;
            break;
            
        case 4:
            mapper = std::make_unique<Mapper004>(prg_banks, chr_banks);
            std::cout << "Using Mapper 4 (MMC3) - Nintendo MMC3 with IRQ" << std::endl;
            std::cout << "  Games: Super Mario Bros 3, Kirby, Mega Man 3-6" << std::endl;
            break;
            
        case 7:
            mapper = std::make_unique<Mapper007>(prg_banks, chr_banks);
            std::cout << "Using Mapper 7 (AxROM) - 32KB PRG switching" << std::endl;
            std::cout << "  Games: Battletoads, Marble Madness, Wizards & Warriors" << std::endl;
            break;
            
        case 9:
            mapper = std::make_unique<Mapper009>(prg_banks, chr_banks);
            std::cout << "Using Mapper 9 (MMC2) - Punch-Out!! mapper" << std::endl;
            std::cout << "  Games: Punch-Out!!, Mike Tyson's Punch-Out!!" << std::endl;
            break;
            
        case 10:
            mapper = std::make_unique<Mapper010>(prg_banks, chr_banks);
            std::cout << "Using Mapper 10 (MMC4) - Fire Emblem mapper" << std::endl;
            std::cout << "  Games: Fire Emblem, Fire Emblem Gaiden" << std::endl;
            break;
            
        case 11:
            mapper = std::make_unique<Mapper011>(prg_banks, chr_banks);
            std::cout << "Using Mapper 11 (Color Dreams) - Unlicensed" << std::endl;
            std::cout << "  Games: Bible Adventures, Crystal Mines, Menace Beach" << std::endl;
            break;
            
        case 66:
            mapper = std::make_unique<Mapper066>(prg_banks, chr_banks);
            std::cout << "Using Mapper 66 (GxROM) - Simple PRG+CHR switching" << std::endl;
            std::cout << "  Games: Super Mario Bros + Duck Hunt, Doraemon" << std::endl;
            break;
            
        case 71:
            mapper = std::make_unique<Mapper071>(prg_banks, chr_banks);
            std::cout << "Using Mapper 71 (Camerica) - Codemasters games" << std::endl;
            std::cout << "  Games: Micro Machines, Fire Hawk, Bee 52, Dizzy" << std::endl;
            break;
            
        case 206:
            mapper = std::make_unique<Mapper206>(prg_banks, chr_banks);
            std::cout << "Using Mapper 206 (Namco 108/DxROM)" << std::endl;
            std::cout << "  Games: Gauntlet, Pac-Land, Rolling Thunder, Sky Kid" << std::endl;
            break;
            
        default:
            // Unsupported mapper - try mapper 0 as fallback
            std::cerr << "Warning: Unsupported mapper " << (int)mapper_id << std::endl;
            std::cerr << "  - Falling back to mapper 0 (NROM)" << std::endl;
            std::cerr << "  - Game may not work correctly" << std::endl;
            mapper = std::make_unique<Mapper000>(prg_banks, chr_banks);
            break;
    }
}

bool Cartridge::cpu_read(uint16_t addr, uint8_t& data) {
    if (mapper && !prg_rom.empty()) {
        return mapper->cpu_read(addr, data, prg_rom.data());
    }
    return false;
}

bool Cartridge::cpu_write(uint16_t addr, uint8_t data) {
    if (mapper && !prg_rom.empty()) {
        return mapper->cpu_write(addr, data, prg_rom.data());
    }
    return false;
}

bool Cartridge::ppu_read(uint16_t addr, uint8_t& data) {
    if (mapper && !chr_rom.empty()) {
        return mapper->ppu_read(addr, data, chr_rom.data());
    }
    return false;
}

bool Cartridge::ppu_write(uint16_t addr, uint8_t data) {
    if (mapper && !chr_rom.empty()) {
        return mapper->ppu_write(addr, data, chr_rom.data());
    }
    return false;
}

bool Cartridge::irq_state() {
    if (mapper) {
        return mapper->irq_state();
    }
    return false;
}

void Cartridge::irq_clear() {
    if (mapper) {
        mapper->irq_clear();
    }
}

void Cartridge::scanline() {
    if (mapper) {
        mapper->scanline();
    }
}

Cartridge::Mirror Cartridge::get_mirror() const {
    if (mapper) {
        auto m = mapper->get_mirror();
        switch (m) {
            case Mapper::Mirror::HORIZONTAL: return Mirror::HORIZONTAL;
            case Mapper::Mirror::VERTICAL: return Mirror::VERTICAL;
            case Mapper::Mirror::ONESCREEN_LO: return Mirror::ONESCREEN_LO;
            case Mapper::Mirror::ONESCREEN_HI: return Mirror::ONESCREEN_HI;
            case Mapper::Mirror::FOUR_SCREEN: return Mirror::FOUR_SCREEN;
        }
    }
    return mirror_mode;
}

#pragma once

#include <cstdint>
#include <array>

// Forward declaration
class Bus;
class Cartridge;

/**
 * NES PPU (Picture Processing Unit) - Cycle Accurate
 * 
 * The PPU runs at 3x the CPU clock (5.369318 MHz NTSC)
 * Renders 256x240 pixels at 60 Hz
 * 262 scanlines per frame (NTSC)
 * 341 PPU cycles per scanline
 * 
 * Scanline breakdown:
 * - Scanlines 0-239: Visible scanlines
 * - Scanline 240: Post-render (idle)
 * - Scanlines 241-260: VBlank
 * - Scanline 261: Pre-render
 */
class PPU {
public:
    explicit PPU(Bus* bus);
    
    // Connect cartridge for CHR ROM/RAM access
    void connect_cartridge(Cartridge* cart);
    
    // Reset PPU to initial state
    void reset();
    
    // Execute one PPU cycle
    void clock();
    
    // CPU interface - PPU registers ($2000-$2007)
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t data);
    
    // PPU memory bus (CHR ROM/RAM, nametables, palette)
    uint8_t ppu_read(uint16_t addr);
    void ppu_write(uint16_t addr, uint8_t data);
    
    // Frame buffer (256x240 pixels, RGB format)
    const uint8_t* get_screen() const { return screen.data(); }
    
    // Check if frame is complete
    bool frame_complete();
    
    // NMI signal to CPU
    bool nmi;

private:
    Bus* bus;
    Cartridge* cartridge;
    
    // PPU registers
    union {
        struct {
            uint8_t nametable_x : 1;
            uint8_t nametable_y : 1;
            uint8_t increment : 1;
            uint8_t sprite_table : 1;
            uint8_t background_table : 1;
            uint8_t sprite_size : 1;
            uint8_t master_slave : 1;
            uint8_t nmi_enable : 1;
        };
        uint8_t reg;
    } control;
    
    union {
        struct {
            uint8_t grayscale : 1;
            uint8_t show_background_left : 1;
            uint8_t show_sprites_left : 1;
            uint8_t show_background : 1;
            uint8_t show_sprites : 1;
            uint8_t emphasize_red : 1;
            uint8_t emphasize_green : 1;
            uint8_t emphasize_blue : 1;
        };
        uint8_t reg;
    } mask;
    
    union {
        struct {
            uint8_t unused : 5;
            uint8_t sprite_overflow : 1;
            uint8_t sprite_zero_hit : 1;
            uint8_t vblank : 1;
        };
        uint8_t reg;
    } status;
    
    // Internal registers
    uint8_t oam_addr;           // OAM address register
    uint8_t data_buffer;        // Data buffer for reads
    
    // Loopy registers (scrolling)
    union LoopyRegister {
        struct {
            uint16_t coarse_x : 5;
            uint16_t coarse_y : 5;
            uint16_t nametable_x : 1;
            uint16_t nametable_y : 1;
            uint16_t fine_y : 3;
            uint16_t unused : 1;
        };
        uint16_t reg;
    };
    
    LoopyRegister vram_addr;    // Current VRAM address (15 bits)
    LoopyRegister tram_addr;    // Temporary VRAM address (15 bits)
    uint8_t fine_x;             // Fine X scroll (3 bits)
    bool address_latch;         // First or second write toggle
    
    // Scanline and cycle counters
    int16_t scanline;           // Current scanline (-1 to 260)
    int16_t cycle;              // Current cycle (0 to 340)
    uint64_t frame_count;       // Total frames rendered
    bool frame_ready;           // Frame complete flag
    
    // Background rendering
    uint8_t bg_next_tile_id;
    uint8_t bg_next_tile_attrib;
    uint8_t bg_next_tile_lsb;
    uint8_t bg_next_tile_msb;
    
    uint16_t bg_shifter_pattern_lo;
    uint16_t bg_shifter_pattern_hi;
    uint16_t bg_shifter_attrib_lo;
    uint16_t bg_shifter_attrib_hi;
    
    // Sprite rendering
    struct ObjectAttributeEntry {
        uint8_t y;
        uint8_t id;
        uint8_t attribute;
        uint8_t x;
    };
    
    std::array<ObjectAttributeEntry, 64> oam;
    std::array<ObjectAttributeEntry, 8> sprite_scanline;
    uint8_t sprite_count;
    
    std::array<uint8_t, 8> sprite_shifter_pattern_lo;
    std::array<uint8_t, 8> sprite_shifter_pattern_hi;
    
    bool sprite_zero_hit_possible;
    bool sprite_zero_being_rendered;
    
    // Memory
    std::array<uint8_t, 2048> nametable;  // 2KB internal VRAM
    std::array<uint8_t, 32> palette;      // 32 bytes palette RAM
    
    // Frame buffer (256 * 240 * 3 for RGB)
    std::array<uint8_t, 256 * 240 * 3> screen;
    
    // NES color palette (64 colors)
    static const uint8_t palette_colors[64][3];
    
    // Helper functions
    void increment_scroll_x();
    void increment_scroll_y();
    void transfer_address_x();
    void transfer_address_y();
    void load_background_shifters();
    void update_shifters();
    
    // Rendering
    uint8_t get_pixel();
    void render_pixel();
};

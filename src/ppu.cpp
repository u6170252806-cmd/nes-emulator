#include "ppu.hpp"
#include "bus.hpp"
#include "cartridge.hpp"
#include <cstring>

PPU::PPU(Bus* bus) : nmi(false), bus(bus), cartridge(nullptr) {
    reset();
}

void PPU::connect_cartridge(Cartridge* cart) {
    cartridge = cart;
}

void PPU::reset() {
    control.reg = 0x00;
    mask.reg = 0x00;
    status.reg = 0x00;
    oam_addr = 0x00;
    data_buffer = 0x00;
    
    vram_addr.reg = 0x0000;
    tram_addr.reg = 0x0000;
    fine_x = 0x00;
    address_latch = false;
    
    scanline = 0;
    cycle = 0;
    frame_count = 0;
    frame_ready = false;
    
    bg_next_tile_id = 0x00;
    bg_next_tile_attrib = 0x00;
    bg_next_tile_lsb = 0x00;
    bg_next_tile_msb = 0x00;
    
    bg_shifter_pattern_lo = 0x0000;
    bg_shifter_pattern_hi = 0x0000;
    bg_shifter_attrib_lo = 0x0000;
    bg_shifter_attrib_hi = 0x0000;
    
    sprite_count = 0;
    sprite_zero_hit_possible = false;
    sprite_zero_being_rendered = false;
    
    nametable.fill(0x00);
    palette.fill(0x00);
    oam.fill({0xFF, 0xFF, 0xFF, 0xFF});
    sprite_scanline.fill({0xFF, 0xFF, 0xFF, 0xFF});
    sprite_shifter_pattern_lo.fill(0x00);
    sprite_shifter_pattern_hi.fill(0x00);
    screen.fill(0x00);
}

bool PPU::frame_complete() {
    bool result = frame_ready;
    frame_ready = false;
    return result;
}

void PPU::clock() {
    // ===== PPU TIMING - CYCLE ACCURATE =====
    // PPU renders 256x240 pixels at 60 Hz (NTSC)
    // 
    // SCANLINE BREAKDOWN:
    // Scanline -1 (261): Pre-render scanline
    //   - Clears VBlank, sprite 0 hit, sprite overflow flags at cycle 1
    //   - Resets Y scroll at cycles 280-304
    //   - Odd frame skip: cycle 0 is skipped on odd frames when rendering enabled
    //
    // Scanlines 0-239: Visible scanlines
    //   - Cycles 1-256: Render pixels
    //   - Cycles 257-320: Sprite evaluation for next scanline
    //   - Cycles 321-336: Fetch first two tiles for next scanline
    //
    // Scanline 240: Post-render (idle)
    //
    // Scanlines 241-260: VBlank
    //   - VBlank flag set at cycle 1 of scanline 241
    //   - NMI triggered if enabled
    //
    // CYCLES PER SCANLINE: 341 (0-340)
    // TOTAL SCANLINES: 262 (-1 to 260, or 261 to 260 wrapping)
    
    // Visible scanlines + pre-render scanline
    if (scanline >= -1 && scanline < 240) {
        // ===== ODD FRAME SKIP =====
        // On odd frames, when rendering is enabled, cycle 0 of the pre-render
        // scanline is skipped. This makes the frame 1 cycle shorter.
        // This is important for proper timing in some games.
        if (scanline == -1 && cycle == 0 && (frame_count & 1)) {
            if (mask.show_background || mask.show_sprites) {
                cycle = 1;  // Skip to cycle 1
            }
        }
        
        // ===== BACKGROUND TILE FETCHING =====
        // Background tiles are fetched during cycles 1-256 and 321-336
        // Each tile fetch takes 8 cycles:
        //   Cycle 1: Nametable byte
        //   Cycle 3: Attribute byte
        //   Cycle 5: Pattern table low byte
        //   Cycle 7: Pattern table high byte
        //   Cycle 8: Increment horizontal scroll
        if ((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338)) {
            update_shifters();
            
            switch ((cycle - 1) % 8) {
                case 0:
                    // Load shifters with previously fetched tile data
                    load_background_shifters();
                    // Fetch nametable byte (tile ID)
                    // Address: 0x2000 | (v & 0x0FFF)
                    bg_next_tile_id = ppu_read(0x2000 | (vram_addr.reg & 0x0FFF));
                    break;
                case 2:
                    // Fetch attribute byte (palette selection)
                    // Attribute table is at 0x23C0 in each nametable
                    // Each byte controls a 4x4 tile area (32x32 pixels)
                    bg_next_tile_attrib = ppu_read(0x23C0 | (vram_addr.nametable_y << 11)
                                                          | (vram_addr.nametable_x << 10)
                                                          | ((vram_addr.coarse_y >> 2) << 3)
                                                          | (vram_addr.coarse_x >> 2));
                    // Select the correct 2-bit palette from the attribute byte
                    // based on which quadrant of the 4x4 area we're in
                    if (vram_addr.coarse_y & 0x02) bg_next_tile_attrib >>= 4;
                    if (vram_addr.coarse_x & 0x02) bg_next_tile_attrib >>= 2;
                    bg_next_tile_attrib &= 0x03;
                    break;
                case 4:
                    // Fetch pattern table low byte
                    // Address: (pattern table select << 12) | (tile ID << 4) | fine_y
                    bg_next_tile_lsb = ppu_read((control.background_table << 12)
                                               + ((uint16_t)bg_next_tile_id << 4)
                                               + (vram_addr.fine_y) + 0);
                    break;
                case 6:
                    // Fetch pattern table high byte (8 bytes after low byte)
                    bg_next_tile_msb = ppu_read((control.background_table << 12)
                                               + ((uint16_t)bg_next_tile_id << 4)
                                               + (vram_addr.fine_y) + 8);
                    break;
                case 7:
                    // Increment horizontal scroll (coarse X)
                    increment_scroll_x();
                    break;
            }
        }
        
        // ===== VERTICAL SCROLL INCREMENT =====
        // At cycle 256, increment the vertical scroll (fine Y, then coarse Y)
        if (cycle == 256) {
            increment_scroll_y();
        }
        
        // ===== HORIZONTAL SCROLL RESET =====
        // At cycle 257, copy horizontal scroll bits from t to v
        if (cycle == 257) {
            load_background_shifters();
            transfer_address_x();
        }
        
        // ===== SPRITE EVALUATION FOR NEXT SCANLINE =====
        // On real hardware, this happens during cycles 65-256
        // We do it at cycle 257 for simplicity but with accurate behavior
        // 
        // SPRITE EVALUATION PROCESS:
        // 1. Clear secondary OAM (8 sprite slots)
        // 2. Read through all 64 sprites in primary OAM
        // 3. For each sprite, check if it's on the NEXT scanline
        // 4. If yes and we have room, copy to secondary OAM
        // 5. If more than 8 sprites found, set overflow flag
        //
        // SPRITE Y COORDINATE:
        // The Y coordinate in OAM is the scanline BEFORE the sprite appears
        // So a sprite with Y=0 first appears on scanline 1
        // A sprite with Y=239 would appear on scanline 240 (off-screen)
        if (cycle == 257 && scanline >= 0) {
            // Clear sprite data for this scanline
            sprite_count = 0;
            sprite_zero_hit_possible = false;
            sprite_shifter_pattern_lo.fill(0x00);
            sprite_shifter_pattern_hi.fill(0x00);
            
            // Clear secondary OAM (sprite scanline buffer)
            // Fill with 0xFF which is "no sprite" (Y=255 is off-screen)
            for (int i = 0; i < 8; i++) {
                sprite_scanline[i].y = 0xFF;
                sprite_scanline[i].id = 0xFF;
                sprite_scanline[i].attribute = 0xFF;
                sprite_scanline[i].x = 0xFF;
            }
            
            // Determine sprite height (8 or 16 pixels)
            uint8_t sprite_height = control.sprite_size ? 16 : 8;
            
            // Evaluate all 64 sprites in OAM
            uint8_t sprites_found = 0;
            for (uint8_t oam_entry = 0; oam_entry < 64; oam_entry++) {
                // Get sprite Y coordinate
                uint8_t sprite_y = oam[oam_entry].y;
                
                // Calculate difference between current scanline and sprite Y
                // We're evaluating for the CURRENT scanline
                int16_t diff = (int16_t)scanline - (int16_t)sprite_y;
                
                // Check if sprite is in range for this scanline
                // diff must be >= 0 (sprite has started) and < sprite_height
                if (diff >= 0 && diff < sprite_height) {
                    if (sprites_found < 8) {
                        // Copy sprite to secondary OAM
                        sprite_scanline[sprites_found] = oam[oam_entry];
                        
                        // Check if this is sprite 0 (for sprite 0 hit detection)
                        if (oam_entry == 0) {
                            sprite_zero_hit_possible = true;
                        }
                    }
                    sprites_found++;
                    
                    // Stop if we've found 9 sprites (8 + overflow)
                    if (sprites_found > 8) break;
                }
            }
            
            // Set sprite overflow flag if more than 8 sprites on scanline
            // Note: Real hardware has a bug in overflow detection, but we
            // implement correct behavior for better compatibility
            status.sprite_overflow = (sprites_found > 8);
            
            // Store actual count (clamped to 8)
            sprite_count = (sprites_found > 8) ? 8 : sprites_found;
        }
        
        // ===== SPRITE PATTERN FETCHING =====
        // During cycles 257-320, the PPU fetches sprite patterns for the next scanline
        // We do this at cycle 340 for simplicity
        // 
        // SPRITE PATTERN ADDRESS CALCULATION:
        // For 8x8 sprites:
        //   Address = (sprite_table << 12) | (tile_id << 4) | row
        //   Where row is 0-7 for the sprite row being rendered
        //
        // For 8x16 sprites:
        //   The tile ID's bit 0 selects the pattern table
        //   Tile ID & 0xFE is the top tile, (Tile ID & 0xFE) + 1 is the bottom tile
        //   Address = (tile_id[0] << 12) | (tile_number << 4) | row
        if (cycle == 340) {
            for (uint8_t i = 0; i < sprite_count; i++) {
                uint8_t sprite_pattern_bits_lo, sprite_pattern_bits_hi;
                uint16_t sprite_pattern_addr_lo, sprite_pattern_addr_hi;
                
                // Calculate which row of the sprite we're rendering
                uint8_t sprite_row = scanline - sprite_scanline[i].y;
                
                if (!control.sprite_size) {
                    // ===== 8x8 SPRITES =====
                    if (!(sprite_scanline[i].attribute & 0x80)) {
                        // Normal (not flipped vertically)
                        sprite_pattern_addr_lo = (control.sprite_table << 12)
                                               | (sprite_scanline[i].id << 4)
                                               | (sprite_row & 0x07);
                    } else {
                        // Flipped vertically - read rows in reverse order
                        sprite_pattern_addr_lo = (control.sprite_table << 12)
                                               | (sprite_scanline[i].id << 4)
                                               | (7 - (sprite_row & 0x07));
                    }
                } else {
                    // ===== 8x16 SPRITES =====
                    // Pattern table is selected by bit 0 of tile ID
                    // Tile ID & 0xFE is the top tile number
                    uint8_t pattern_table = sprite_scanline[i].id & 0x01;
                    uint8_t tile_id = sprite_scanline[i].id & 0xFE;
                    
                    if (!(sprite_scanline[i].attribute & 0x80)) {
                        // Normal (not flipped vertically)
                        if (sprite_row < 8) {
                            // Top half of sprite
                            sprite_pattern_addr_lo = (pattern_table << 12)
                                                   | (tile_id << 4)
                                                   | (sprite_row & 0x07);
                        } else {
                            // Bottom half of sprite (next tile)
                            sprite_pattern_addr_lo = (pattern_table << 12)
                                                   | ((tile_id + 1) << 4)
                                                   | (sprite_row & 0x07);
                        }
                    } else {
                        // Flipped vertically
                        if (sprite_row < 8) {
                            // Top half becomes bottom tile, rows reversed
                            sprite_pattern_addr_lo = (pattern_table << 12)
                                                   | ((tile_id + 1) << 4)
                                                   | (7 - (sprite_row & 0x07));
                        } else {
                            // Bottom half becomes top tile, rows reversed
                            sprite_pattern_addr_lo = (pattern_table << 12)
                                                   | (tile_id << 4)
                                                   | (7 - (sprite_row & 0x07));
                        }
                    }
                }
                
                // High byte is 8 bytes after low byte in pattern table
                sprite_pattern_addr_hi = sprite_pattern_addr_lo + 8;
                
                // Fetch the pattern bytes
                sprite_pattern_bits_lo = ppu_read(sprite_pattern_addr_lo);
                sprite_pattern_bits_hi = ppu_read(sprite_pattern_addr_hi);
                
                // ===== HORIZONTAL FLIP =====
                // If attribute bit 6 is set, flip the sprite horizontally
                if (sprite_scanline[i].attribute & 0x40) {
                    // Reverse bits in byte using parallel bit swap
                    auto flip_byte = [](uint8_t b) -> uint8_t {
                        b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
                        b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
                        b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
                        return b;
                    };
                    sprite_pattern_bits_lo = flip_byte(sprite_pattern_bits_lo);
                    sprite_pattern_bits_hi = flip_byte(sprite_pattern_bits_hi);
                }
                
                // Store in shifters for rendering
                sprite_shifter_pattern_lo[i] = sprite_pattern_bits_lo;
                sprite_shifter_pattern_hi[i] = sprite_pattern_bits_hi;
            }
        }
        
        // Pre-render scanline operations
        if (scanline == -1 && cycle >= 280 && cycle < 305) {
            // Reset Y
            transfer_address_y();
        }
    }
    
    // Post-render scanline (idle)
    if (scanline == 240) {
        // Do nothing
    }
    
    // VBlank scanlines
    if (scanline >= 241 && scanline < 261) {
        if (scanline == 241 && cycle == 1) {
            // Enter VBlank
            status.vblank = 1;
            if (control.nmi_enable) {
                nmi = true;
            }
        }
    }
    
    // Pre-render scanline
    if (scanline == -1 && cycle == 1) {
        // Clear VBlank, sprite 0 hit, and sprite overflow flags
        status.vblank = 0;
        status.sprite_zero_hit = 0;
        status.sprite_overflow = 0;
        sprite_zero_being_rendered = false;
    }
    
    // Render pixel
    if (scanline >= 0 && scanline < 240 && cycle >= 1 && cycle < 257) {
        render_pixel();
    }
    
    // MMC3 scanline counter - triggered at cycle 260 when rendering is enabled
    if (cycle == 260 && (mask.show_background || mask.show_sprites)) {
        if (scanline >= 0 && scanline < 240) {
            if (cartridge) {
                cartridge->scanline();
            }
        }
    }
    
    // Advance cycle and scanline
    cycle++;
    if (cycle >= 341) {
        cycle = 0;
        scanline++;
        
        if (scanline >= 261) {
            scanline = -1;
            frame_ready = true;
            frame_count++;
        }
    }
}

void PPU::render_pixel() {
    uint8_t bg_pixel = 0x00;
    uint8_t bg_palette = 0x00;
    
    // Background rendering
    if (mask.show_background) {
        if (mask.show_background_left || cycle >= 9) {
            uint16_t bit_mux = 0x8000 >> fine_x;
            
            uint8_t p0_pixel = (bg_shifter_pattern_lo & bit_mux) > 0;
            uint8_t p1_pixel = (bg_shifter_pattern_hi & bit_mux) > 0;
            bg_pixel = (p1_pixel << 1) | p0_pixel;
            
            uint8_t bg_pal0 = (bg_shifter_attrib_lo & bit_mux) > 0;
            uint8_t bg_pal1 = (bg_shifter_attrib_hi & bit_mux) > 0;
            bg_palette = (bg_pal1 << 1) | bg_pal0;
        }
    }
    
    // ===== SPRITE RENDERING =====
    // Sprites are rendered with priority based on their position in OAM
    // Lower OAM index = higher priority (sprite 0 is highest)
    // Sprites can be behind or in front of background based on attribute bit 5
    
    uint8_t fg_pixel = 0x00;
    uint8_t fg_palette = 0x00;
    uint8_t fg_priority = 0x00;
    
    if (mask.show_sprites) {
        // Check if we should render sprites in leftmost 8 pixels
        if (mask.show_sprites_left || cycle >= 9) {
            sprite_zero_being_rendered = false;
            
            // Check each sprite in priority order (0 = highest priority)
            for (uint8_t i = 0; i < sprite_count && i < 8; i++) {
                // Check if this sprite's X position means it should be rendered now
                // sprite_scanline[i].x counts down from initial X position
                // When it reaches 0, we start outputting pixels from the shifters
                if (sprite_scanline[i].x == 0) {
                    // Get pixel from sprite shifters (bit 7 is current pixel)
                    uint8_t fg_pixel_lo = (sprite_shifter_pattern_lo[i] & 0x80) > 0;
                    uint8_t fg_pixel_hi = (sprite_shifter_pattern_hi[i] & 0x80) > 0;
                    fg_pixel = (fg_pixel_hi << 1) | fg_pixel_lo;
                    
                    // Get palette (sprites use palettes 4-7)
                    fg_palette = (sprite_scanline[i].attribute & 0x03) + 0x04;
                    
                    // Get priority (0 = in front of background, 1 = behind)
                    // Attribute bit 5: 0 = sprite in front, 1 = sprite behind
                    fg_priority = (sprite_scanline[i].attribute & 0x20) == 0;
                    
                    // If this sprite has a non-transparent pixel, use it
                    // (pixel value 0 is transparent for sprites)
                    if (fg_pixel != 0) {
                        // Check if this is sprite 0 for sprite 0 hit detection
                        if (i == 0) {
                            sprite_zero_being_rendered = true;
                        }
                        // Stop checking - first non-transparent sprite wins
                        break;
                    }
                }
            }
        }
    }
    
    // Combine background and sprite
    uint8_t pixel = 0x00;
    uint8_t palette_idx = 0x00;
    
    if (bg_pixel == 0 && fg_pixel == 0) {
        pixel = 0x00;
        palette_idx = 0x00;
    } else if (bg_pixel == 0 && fg_pixel > 0) {
        pixel = fg_pixel;
        palette_idx = fg_palette;
    } else if (bg_pixel > 0 && fg_pixel == 0) {
        pixel = bg_pixel;
        palette_idx = bg_palette;
    } else if (bg_pixel > 0 && fg_pixel > 0) {
        if (fg_priority) {
            pixel = fg_pixel;
            palette_idx = fg_palette;
        } else {
            pixel = bg_pixel;
            palette_idx = bg_palette;
        }
        
        // ===== SPRITE 0 HIT DETECTION =====
        // Sprite 0 hit occurs when:
        // 1. Both background and sprite rendering are enabled
        // 2. A non-transparent background pixel overlaps with a non-transparent sprite 0 pixel
        // 3. The pixel is not in the leftmost 8 pixels (if clipping is enabled)
        // 4. The pixel is not at X=255 (hardware quirk)
        //
        // This is used by games like Super Mario Bros for the status bar split
        // The game waits for sprite 0 hit to know when to change scroll
        if (sprite_zero_hit_possible && sprite_zero_being_rendered) {
            if (mask.show_background && mask.show_sprites) {
                // Check left edge clipping
                // If either background or sprite left clipping is disabled,
                // sprite 0 hit can occur in the leftmost 8 pixels
                bool left_clip = !(mask.show_background_left && mask.show_sprites_left);
                
                // Sprite 0 hit cannot occur at X=255 (cycle 256)
                if (cycle < 256) {
                    if (left_clip) {
                        // Left clipping enabled - hit only at cycles 9-255
                        if (cycle >= 9) {
                            status.sprite_zero_hit = 1;
                        }
                    } else {
                        // No left clipping - hit at cycles 2-255
                        // (cycle 1 is the first visible pixel)
                        if (cycle >= 2) {
                            status.sprite_zero_hit = 1;
                        }
                    }
                }
            }
        }
    }
    
    // Get color from palette
    uint8_t color_idx = ppu_read(0x3F00 + (palette_idx << 2) + pixel) & 0x3F;
    
    // Write to screen buffer
    int x = cycle - 1;
    int y = scanline;
    int idx = (y * 256 + x) * 3;
    screen[idx + 0] = palette_colors[color_idx][0];
    screen[idx + 1] = palette_colors[color_idx][1];
    screen[idx + 2] = palette_colors[color_idx][2];
    
    // Update sprite shifters
    for (uint8_t i = 0; i < sprite_count && i < 8; i++) {
        if (sprite_scanline[i].x > 0) {
            sprite_scanline[i].x--;
        } else {
            sprite_shifter_pattern_lo[i] <<= 1;
            sprite_shifter_pattern_hi[i] <<= 1;
        }
    }
}

void PPU::increment_scroll_x() {
    if (mask.show_background || mask.show_sprites) {
        if (vram_addr.coarse_x == 31) {
            vram_addr.coarse_x = 0;
            vram_addr.nametable_x = ~vram_addr.nametable_x;
        } else {
            vram_addr.coarse_x++;
        }
    }
}

void PPU::increment_scroll_y() {
    if (mask.show_background || mask.show_sprites) {
        if (vram_addr.fine_y < 7) {
            vram_addr.fine_y++;
        } else {
            vram_addr.fine_y = 0;
            
            if (vram_addr.coarse_y == 29) {
                vram_addr.coarse_y = 0;
                vram_addr.nametable_y = ~vram_addr.nametable_y;
            } else if (vram_addr.coarse_y == 31) {
                vram_addr.coarse_y = 0;
            } else {
                vram_addr.coarse_y++;
            }
        }
    }
}

void PPU::transfer_address_x() {
    if (mask.show_background || mask.show_sprites) {
        vram_addr.nametable_x = tram_addr.nametable_x;
        vram_addr.coarse_x = tram_addr.coarse_x;
    }
}

void PPU::transfer_address_y() {
    if (mask.show_background || mask.show_sprites) {
        vram_addr.fine_y = tram_addr.fine_y;
        vram_addr.nametable_y = tram_addr.nametable_y;
        vram_addr.coarse_y = tram_addr.coarse_y;
    }
}

void PPU::load_background_shifters() {
    bg_shifter_pattern_lo = (bg_shifter_pattern_lo & 0xFF00) | bg_next_tile_lsb;
    bg_shifter_pattern_hi = (bg_shifter_pattern_hi & 0xFF00) | bg_next_tile_msb;
    
    bg_shifter_attrib_lo = (bg_shifter_attrib_lo & 0xFF00) | ((bg_next_tile_attrib & 0b01) ? 0xFF : 0x00);
    bg_shifter_attrib_hi = (bg_shifter_attrib_hi & 0xFF00) | ((bg_next_tile_attrib & 0b10) ? 0xFF : 0x00);
}

void PPU::update_shifters() {
    if (mask.show_background) {
        bg_shifter_pattern_lo <<= 1;
        bg_shifter_pattern_hi <<= 1;
        bg_shifter_attrib_lo <<= 1;
        bg_shifter_attrib_hi <<= 1;
    }
}

uint8_t PPU::cpu_read(uint16_t addr) {
    uint8_t data = 0x00;
    addr &= 0x0007;
    
    switch (addr) {
        case 0x0000: // Control - write only
            break;
        case 0x0001: // Mask - write only
            break;
        case 0x0002: // Status ($2002)
            // Reading status returns:
            // Bits 7-5: VBlank, Sprite 0 Hit, Sprite Overflow
            // Bits 4-0: Stale PPU bus contents (from last read/write)
            //
            // SIDE EFFECTS:
            // 1. VBlank flag is cleared
            // 2. Address latch (w) is reset
            //
            // RACE CONDITION:
            // If status is read at the exact cycle VBlank is set (scanline 241, cycle 1),
            // the VBlank flag may not be set yet. We don't emulate this edge case.
            data = (status.reg & 0xE0) | (data_buffer & 0x1F);
            status.vblank = 0;  // Clear VBlank flag
            address_latch = false;  // Reset address latch
            break;
        case 0x0003: // OAM Address - write only
            break;
        case 0x0004: // OAM Data
            // Reading OAM data returns the byte at the current OAM address
            // During rendering (scanlines 0-239, cycles 1-64), this returns 0xFF
            // due to secondary OAM clear, but we don't emulate that detail
            data = ((uint8_t*)oam.data())[oam_addr];
            // Note: OAM address is NOT incremented on read (only on write)
            break;
        case 0x0005: // Scroll - write only
            break;
        case 0x0006: // PPU Address - write only
            break;
        case 0x0007: // PPU Data
            data = data_buffer;
            data_buffer = ppu_read(vram_addr.reg);
            
            // Palette reads are immediate
            if (vram_addr.reg >= 0x3F00) {
                data = data_buffer;
            }
            
            vram_addr.reg += (control.increment ? 32 : 1);
            break;
    }
    
    return data;
}

void PPU::cpu_write(uint16_t addr, uint8_t data) {
    addr &= 0x0007;
    
    switch (addr) {
        case 0x0000: // Control
            control.reg = data;
            tram_addr.nametable_x = control.nametable_x;
            tram_addr.nametable_y = control.nametable_y;
            break;
        case 0x0001: // Mask
            mask.reg = data;
            break;
        case 0x0002: // Status - read only
            break;
        case 0x0003: // OAM Address
            oam_addr = data;
            break;
        case 0x0004: // OAM Data
            ((uint8_t*)oam.data())[oam_addr] = data;
            oam_addr++;  // Auto-increment OAM address
            break;
        case 0x0005: // Scroll
            if (!address_latch) {
                fine_x = data & 0x07;
                tram_addr.coarse_x = data >> 3;
                address_latch = true;
            } else {
                tram_addr.fine_y = data & 0x07;
                tram_addr.coarse_y = data >> 3;
                address_latch = false;
            }
            break;
        case 0x0006: // PPU Address
            if (!address_latch) {
                tram_addr.reg = (uint16_t)((data & 0x3F) << 8) | (tram_addr.reg & 0x00FF);
                address_latch = true;
            } else {
                tram_addr.reg = (tram_addr.reg & 0xFF00) | data;
                vram_addr = tram_addr;
                address_latch = false;
            }
            break;
        case 0x0007: // PPU Data
            ppu_write(vram_addr.reg, data);
            vram_addr.reg += (control.increment ? 32 : 1);
            break;
    }
}

uint8_t PPU::ppu_read(uint16_t addr) {
    addr &= 0x3FFF;
    uint8_t data = 0x00;
    
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // Pattern tables (CHR ROM/RAM)
        if (cartridge) {
            cartridge->ppu_read(addr, data);
        }
    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // Nametables
        addr &= 0x0FFF;
        
        Cartridge::Mirror mirror = Cartridge::Mirror::HORIZONTAL;
        if (cartridge) {
            mirror = cartridge->get_mirror();
        }
        
        uint16_t mapped_addr = 0;
        if (mirror == Cartridge::Mirror::VERTICAL) {
            // Vertical: $2000=$2800, $2400=$2C00
            mapped_addr = addr & 0x07FF;
        } else if (mirror == Cartridge::Mirror::HORIZONTAL) {
            // Horizontal: $2000=$2400, $2800=$2C00
            if (addr < 0x0800) {
                mapped_addr = addr & 0x03FF;
            } else {
                mapped_addr = 0x0400 + (addr & 0x03FF);
            }
        } else if (mirror == Cartridge::Mirror::ONESCREEN_LO) {
            mapped_addr = addr & 0x03FF;
        } else if (mirror == Cartridge::Mirror::ONESCREEN_HI) {
            mapped_addr = 0x0400 + (addr & 0x03FF);
        } else {
            // Four screen or default
            mapped_addr = addr & 0x07FF;
        }
        data = nametable[mapped_addr];
    } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // Palette
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;
        data = palette[addr] & (mask.grayscale ? 0x30 : 0x3F);
    }
    
    return data;
}

void PPU::ppu_write(uint16_t addr, uint8_t data) {
    addr &= 0x3FFF;
    
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // Pattern tables (CHR ROM/RAM)
        if (cartridge) {
            cartridge->ppu_write(addr, data);
        }
    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // Nametables
        addr &= 0x0FFF;
        
        Cartridge::Mirror mirror = Cartridge::Mirror::HORIZONTAL;
        if (cartridge) {
            mirror = cartridge->get_mirror();
        }
        
        uint16_t mapped_addr = 0;
        if (mirror == Cartridge::Mirror::VERTICAL) {
            mapped_addr = addr & 0x07FF;
        } else if (mirror == Cartridge::Mirror::HORIZONTAL) {
            if (addr < 0x0800) {
                mapped_addr = addr & 0x03FF;
            } else {
                mapped_addr = 0x0400 + (addr & 0x03FF);
            }
        } else if (mirror == Cartridge::Mirror::ONESCREEN_LO) {
            mapped_addr = addr & 0x03FF;
        } else if (mirror == Cartridge::Mirror::ONESCREEN_HI) {
            mapped_addr = 0x0400 + (addr & 0x03FF);
        } else {
            mapped_addr = addr & 0x07FF;
        }
        nametable[mapped_addr] = data;
    } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // Palette
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;
        palette[addr] = data;
    }
}



// NES Color Palette (64 colors, RGB)
const uint8_t PPU::palette_colors[64][3] = {
    {84, 84, 84}, {0, 30, 116}, {8, 16, 144}, {48, 0, 136}, {68, 0, 100}, {92, 0, 48}, {84, 4, 0}, {60, 24, 0},
    {32, 42, 0}, {8, 58, 0}, {0, 64, 0}, {0, 60, 0}, {0, 50, 60}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {152, 150, 152}, {8, 76, 196}, {48, 50, 236}, {92, 30, 228}, {136, 20, 176}, {160, 20, 100}, {152, 34, 32}, {120, 60, 0},
    {84, 90, 0}, {40, 114, 0}, {8, 124, 0}, {0, 118, 40}, {0, 102, 120}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {236, 238, 236}, {76, 154, 236}, {120, 124, 236}, {176, 98, 236}, {228, 84, 236}, {236, 88, 180}, {236, 106, 100}, {212, 136, 32},
    {160, 170, 0}, {116, 196, 0}, {76, 208, 32}, {56, 204, 108}, {56, 180, 204}, {60, 60, 60}, {0, 0, 0}, {0, 0, 0},
    {236, 238, 236}, {168, 204, 236}, {188, 188, 236}, {212, 178, 236}, {236, 174, 236}, {236, 174, 212}, {236, 180, 176}, {228, 196, 144},
    {204, 210, 120}, {180, 222, 120}, {168, 226, 144}, {152, 226, 180}, {160, 214, 228}, {160, 162, 160}, {0, 0, 0}, {0, 0, 0}
};

#include "apu.hpp"
#include <cmath>
#include <cstring>

// Lookup tables
const uint8_t APU::length_table[32] = {
    10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

const uint16_t APU::noise_period_table[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

const uint16_t APU::dmc_rate_table[16] = {
    428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106,  84,  72,  54
};

const uint8_t APU::duty_table[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0},  // 12.5%
    {0, 1, 1, 0, 0, 0, 0, 0},  // 25%
    {0, 1, 1, 1, 1, 0, 0, 0},  // 50%
    {1, 0, 0, 1, 1, 1, 1, 1},  // 25% negated
};

const uint8_t APU::triangle_sequence[32] = {
    15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

APU::APU() : cycles(0) {
    reset();
    init_mixer_tables();
}

void APU::reset() {
    // Reset pulse channels
    pulse1 = {};
    pulse2 = {};
    pulse1.enabled = false;
    pulse2.enabled = false;
    
    // Reset triangle channel
    triangle = {};
    triangle.enabled = false;
    
    // Reset noise channel
    noise = {};
    noise.enabled = false;
    noise.shift_register = 1;
    
    // Reset DMC
    dmc = {};
    dmc.enabled = false;
    dmc.sample_buffer_empty = true;
    
    // Reset frame counter
    frame_counter_mode = false;
    irq_inhibit = false;
    frame_counter = 0;
    frame_step = 0;
    cycles = 0;
}

void APU::clock() {
    // ===== APU FRAME COUNTER =====
    // The APU frame counter generates clocks for envelope, sweep, and length counters
    // It runs at 240 Hz (4-step mode) or 192 Hz (5-step mode)
    // 
    // 4-step sequence (mode 0): Generates IRQ
    // Step 1: 3728.5 cycles - Quarter frame
    // Step 2: 7456.5 cycles - Quarter + Half frame
    // Step 3: 11185.5 cycles - Quarter frame
    // Step 4: 14914 cycles - Quarter + Half frame + IRQ
    //
    // 5-step sequence (mode 1): No IRQ
    // Step 1: 3728.5 cycles - Quarter frame
    // Step 2: 7456.5 cycles - Quarter + Half frame
    // Step 3: 11185.5 cycles - Quarter frame
    // Step 4: 14914 cycles - Nothing
    // Step 5: 18640.5 cycles - Quarter + Half frame
    
    bool quarter_frame = false;
    bool half_frame = false;
    
    // Use integer comparison with tolerance for half-cycle timing
    uint64_t c = cycles;
    
    if (frame_counter_mode == false) {
        // 4-step sequence
        if (c == 3728 || c == 3729) quarter_frame = true;
        if (c == 7456 || c == 7457) { quarter_frame = true; half_frame = true; }
        if (c == 11185 || c == 11186) quarter_frame = true;
        if (c == 14914 || c == 14915) { 
            quarter_frame = true; 
            half_frame = true;
            cycles = 0;
            // IRQ would be triggered here if !irq_inhibit
        }
    } else {
        // 5-step sequence
        if (c == 3728 || c == 3729) quarter_frame = true;
        if (c == 7456 || c == 7457) { quarter_frame = true; half_frame = true; }
        if (c == 11185 || c == 11186) quarter_frame = true;
        // Step 4 at 14914 does nothing in 5-step mode
        if (c == 18640 || c == 18641) { 
            quarter_frame = true; 
            half_frame = true;
            cycles = 0;
        }
    }
    
    if (quarter_frame) {
        clock_quarter_frame();
    }
    if (half_frame) {
        clock_half_frame();
    }
    
    // Clock channel timers
    // Pulse channels clock every other CPU cycle (APU cycle)
    if (cycles % 2 == 0) {
        pulse1.clock_timer();
        pulse2.clock_timer();
    }
    
    // Triangle clocks every CPU cycle
    triangle.clock_timer();
    
    // Noise clocks every CPU cycle
    noise.clock_timer();
    
    // DMC clocks every CPU cycle
    dmc.clock_timer();
    
    cycles++;
}

void APU::clock_quarter_frame() {
    pulse1.clock_envelope();
    pulse2.clock_envelope();
    triangle.clock_linear_counter();
    noise.clock_envelope();
}

void APU::clock_half_frame() {
    pulse1.clock_length();
    pulse1.clock_sweep(true);
    pulse2.clock_length();
    pulse2.clock_sweep(false);
    triangle.clock_length();
    noise.clock_length();
}

uint8_t APU::cpu_read(uint16_t addr) {
    uint8_t data = 0x00;
    
    switch (addr) {
        case 0x4015: // Status
            data |= (pulse1.length_counter > 0) ? 0x01 : 0x00;
            data |= (pulse2.length_counter > 0) ? 0x02 : 0x00;
            data |= (triangle.length_counter > 0) ? 0x04 : 0x00;
            data |= (noise.length_counter > 0) ? 0x08 : 0x00;
            data |= (dmc.bytes_remaining > 0) ? 0x10 : 0x00;
            break;
    }
    
    return data;
}

void APU::cpu_write(uint16_t addr, uint8_t data) {
    switch (addr) {
        // Pulse 1
        case 0x4000:
            pulse1.duty_cycle = (data >> 6) & 0x03;
            pulse1.halt_length = (data >> 5) & 0x01;
            pulse1.constant_volume = (data >> 4) & 0x01;
            pulse1.volume = data & 0x0F;
            break;
        case 0x4001:
            pulse1.sweep_enabled = (data >> 7) & 0x01;
            pulse1.sweep_period = (data >> 4) & 0x07;
            pulse1.sweep_negate = (data >> 3) & 0x01;
            pulse1.sweep_shift = data & 0x07;
            pulse1.sweep_reload = true;
            break;
        case 0x4002:
            pulse1.timer_period = (pulse1.timer_period & 0xFF00) | data;
            break;
        case 0x4003:
            pulse1.timer_period = (pulse1.timer_period & 0x00FF) | ((data & 0x07) << 8);
            pulse1.length_counter = length_table[(data >> 3) & 0x1F];
            pulse1.sequence_counter = 0;
            pulse1.envelope_start = true;
            break;
            
        // Pulse 2
        case 0x4004:
            pulse2.duty_cycle = (data >> 6) & 0x03;
            pulse2.halt_length = (data >> 5) & 0x01;
            pulse2.constant_volume = (data >> 4) & 0x01;
            pulse2.volume = data & 0x0F;
            break;
        case 0x4005:
            pulse2.sweep_enabled = (data >> 7) & 0x01;
            pulse2.sweep_period = (data >> 4) & 0x07;
            pulse2.sweep_negate = (data >> 3) & 0x01;
            pulse2.sweep_shift = data & 0x07;
            pulse2.sweep_reload = true;
            break;
        case 0x4006:
            pulse2.timer_period = (pulse2.timer_period & 0xFF00) | data;
            break;
        case 0x4007:
            pulse2.timer_period = (pulse2.timer_period & 0x00FF) | ((data & 0x07) << 8);
            pulse2.length_counter = length_table[(data >> 3) & 0x1F];
            pulse2.sequence_counter = 0;
            pulse2.envelope_start = true;
            break;
            
        // Triangle
        case 0x4008:
            triangle.control_flag = (data >> 7) & 0x01;
            triangle.linear_counter_load = data & 0x7F;
            break;
        case 0x400A:
            triangle.timer_period = (triangle.timer_period & 0xFF00) | data;
            break;
        case 0x400B:
            triangle.timer_period = (triangle.timer_period & 0x00FF) | ((data & 0x07) << 8);
            triangle.length_counter = length_table[(data >> 3) & 0x1F];
            triangle.linear_counter_reload = true;
            break;
            
        // Noise
        case 0x400C:
            noise.halt_length = (data >> 5) & 0x01;
            noise.constant_volume = (data >> 4) & 0x01;
            noise.volume = data & 0x0F;
            break;
        case 0x400E:
            noise.mode = (data >> 7) & 0x01;
            noise.timer_period = noise_period_table[data & 0x0F];
            break;
        case 0x400F:
            noise.length_counter = length_table[(data >> 3) & 0x1F];
            noise.envelope_start = true;
            break;
            
        // DMC
        case 0x4010:
            dmc.irq_enabled = (data >> 7) & 0x01;
            dmc.loop = (data >> 6) & 0x01;
            dmc.rate = data & 0x0F;
            dmc.timer_period = dmc_rate_table[dmc.rate];
            break;
        case 0x4011:
            dmc.output_level = data & 0x7F;
            break;
        case 0x4012:
            dmc.sample_address = 0xC000 + (data * 64);
            break;
        case 0x4013:
            dmc.sample_length = (data * 16) + 1;
            break;
            
        // Status
        case 0x4015:
            pulse1.enabled = data & 0x01;
            pulse2.enabled = data & 0x02;
            triangle.enabled = data & 0x04;
            noise.enabled = data & 0x08;
            dmc.enabled = data & 0x10;
            
            if (!pulse1.enabled) pulse1.length_counter = 0;
            if (!pulse2.enabled) pulse2.length_counter = 0;
            if (!triangle.enabled) triangle.length_counter = 0;
            if (!noise.enabled) noise.length_counter = 0;
            if (!dmc.enabled) {
                dmc.bytes_remaining = 0;
            } else if (dmc.bytes_remaining == 0) {
                dmc.start_sample();
            }
            break;
            
        // Frame counter
        case 0x4017:
            frame_counter_mode = (data >> 7) & 0x01;
            irq_inhibit = (data >> 6) & 0x01;
            if (frame_counter_mode) {
                clock_quarter_frame();
                clock_half_frame();
            }
            break;
    }
}

float APU::get_output_sample() {
    // ===== NES AUDIO MIXING - HIGH QUALITY =====
    // The NES uses non-linear mixing for its audio channels
    // This creates the characteristic NES sound
    
    // Pulse channels (square waves)
    // Combined output ranges from 0-30
    uint8_t pulse_out = pulse1.output + pulse2.output;
    
    // TND channels (Triangle, Noise, DMC)
    // Triangle output: 0-15, Noise output: 0-15, DMC output: 0-127
    // Combined with weights: 3*tri + 2*noise + dmc
    // This gives a range of 0-202
    uint8_t tri_out = triangle.output;
    uint8_t noise_out = noise.output;
    uint8_t dmc_out = dmc.output_level;
    
    // Clamp values to prevent overflow
    if (pulse_out > 30) pulse_out = 30;
    uint16_t tnd_out = 3 * tri_out + 2 * noise_out + dmc_out;
    if (tnd_out > 202) tnd_out = 202;
    
    // Use lookup tables for non-linear mixing
    float pulse_sample = pulse_table[pulse_out];
    float tnd_sample = tnd_table[tnd_out];
    
    // Combine channels
    float output = pulse_sample + tnd_sample;
    
    // ===== IMPROVED LOW-PASS FILTER =====
    // Two-stage filter for better audio quality
    // First stage: 14kHz cutoff (removes harsh high frequencies)
    // Second stage: smoothing for anti-aliasing
    static float filter1 = 0.0f;
    static float filter2 = 0.0f;
    
    // First order low-pass filter (RC filter simulation)
    // Coefficient ~0.815 gives ~14kHz cutoff at 44.1kHz sample rate
    const float alpha1 = 0.815f;
    filter1 = filter1 * alpha1 + output * (1.0f - alpha1);
    
    // Second stage smoothing (gentler, ~0.6 coefficient)
    const float alpha2 = 0.6f;
    filter2 = filter2 * alpha2 + filter1 * (1.0f - alpha2);
    
    // High-pass filter to remove DC offset (very gentle)
    static float hp_prev_in = 0.0f;
    static float hp_prev_out = 0.0f;
    const float hp_alpha = 0.995f;  // Very high = minimal bass cut
    float hp_out = hp_alpha * (hp_prev_out + filter2 - hp_prev_in);
    hp_prev_in = filter2;
    hp_prev_out = hp_out;
    
    // Scale output to reasonable level
    // NES audio is quite loud, so we reduce it
    float final_output = hp_out * 0.85f;
    
    // Soft clipping to prevent harsh distortion
    if (final_output > 0.95f) final_output = 0.95f;
    if (final_output < -0.95f) final_output = -0.95f;
    
    return final_output;
}

void APU::set_dmc_read_callback(uint8_t (*callback)(uint16_t)) {
    dmc.read_callback = callback;
}

void APU::init_mixer_tables() {
    // Pulse mixer
    for (int i = 0; i < 31; i++) {
        pulse_table[i] = 95.52f / (8128.0f / i + 100.0f);
    }
    
    // TND mixer
    for (int i = 0; i < 203; i++) {
        tnd_table[i] = 163.67f / (24329.0f / i + 100.0f);
    }
}

// Pulse channel methods
void APU::Pulse::clock_timer() {
    // Pulse timer counts down and reloads from period
    // When it reaches 0, advance the duty cycle sequencer
    if (timer_counter == 0) {
        timer_counter = timer_period;
        sequence_counter = (sequence_counter + 1) % 8;
    } else {
        timer_counter--;
    }
    
    // Output is determined by:
    // 1. Channel must be enabled
    // 2. Length counter must be > 0
    // 3. Timer period must be >= 8 (to avoid ultrasonic frequencies)
    // 4. Timer period must be < 0x7FF (sweep unit muting)
    // 5. Duty cycle sequencer determines if output is high or low
    // 6. Volume is either constant or from envelope
    
    bool muted = !enabled || length_counter == 0 || timer_period < 8;
    
    // Check sweep unit muting (target period would be > 0x7FF)
    if (sweep_enabled && sweep_shift > 0) {
        uint16_t change = timer_period >> sweep_shift;
        uint16_t target = sweep_negate ? (timer_period - change) : (timer_period + change);
        if (target > 0x7FF) muted = true;
    }
    
    if (!muted) {
        output = duty_table[duty_cycle][sequence_counter] * (constant_volume ? volume : envelope_volume);
    } else {
        output = 0;
    }
}

void APU::Pulse::clock_envelope() {
    if (envelope_start) {
        envelope_start = false;
        envelope_volume = 15;
        envelope_counter = volume;
    } else {
        if (envelope_counter == 0) {
            envelope_counter = volume;
            if (envelope_volume == 0) {
                if (halt_length) {
                    envelope_volume = 15;
                }
            } else {
                envelope_volume--;
            }
        } else {
            envelope_counter--;
        }
    }
}

void APU::Pulse::clock_sweep(bool channel_one) {
    if (sweep_counter == 0 && sweep_enabled && sweep_shift > 0 && timer_period >= 8) {
        uint16_t change = timer_period >> sweep_shift;
        if (sweep_negate) {
            timer_period -= change;
            if (channel_one) timer_period--;  // Pulse 1 uses one's complement
        } else {
            timer_period += change;
        }
    }
    
    if (sweep_counter == 0 || sweep_reload) {
        sweep_counter = sweep_period;
        sweep_reload = false;
    } else {
        sweep_counter--;
    }
}

void APU::Pulse::clock_length() {
    if (!halt_length && length_counter > 0) {
        length_counter--;
    }
}

// Triangle channel methods
void APU::Triangle::clock_timer() {
    if (timer_counter == 0) {
        timer_counter = timer_period;
        if (length_counter > 0 && linear_counter > 0) {
            sequence_counter = (sequence_counter + 1) % 32;
        }
    } else {
        timer_counter--;
    }
    
    if (enabled && length_counter > 0 && linear_counter > 0) {
        output = triangle_sequence[sequence_counter];
    } else {
        output = 0;
    }
}

void APU::Triangle::clock_linear_counter() {
    if (linear_counter_reload) {
        linear_counter = linear_counter_load;
    } else if (linear_counter > 0) {
        linear_counter--;
    }
    
    if (!control_flag) {
        linear_counter_reload = false;
    }
}

void APU::Triangle::clock_length() {
    if (!control_flag && length_counter > 0) {
        length_counter--;
    }
}

// Noise channel methods
void APU::Noise::clock_timer() {
    if (timer_counter == 0) {
        timer_counter = timer_period;
        
        uint16_t feedback = shift_register & 0x0001;
        if (mode) {
            feedback ^= (shift_register >> 6) & 0x0001;
        } else {
            feedback ^= (shift_register >> 1) & 0x0001;
        }
        
        shift_register >>= 1;
        shift_register |= (feedback << 14);
    } else {
        timer_counter--;
    }
    
    if (enabled && length_counter > 0 && (shift_register & 0x0001) == 0) {
        output = constant_volume ? volume : envelope_volume;
    } else {
        output = 0;
    }
}

void APU::Noise::clock_envelope() {
    if (envelope_start) {
        envelope_start = false;
        envelope_volume = 15;
        envelope_counter = volume;
    } else {
        if (envelope_counter == 0) {
            envelope_counter = volume;
            if (envelope_volume == 0) {
                if (halt_length) {
                    envelope_volume = 15;
                }
            } else {
                envelope_volume--;
            }
        } else {
            envelope_counter--;
        }
    }
}

void APU::Noise::clock_length() {
    if (!halt_length && length_counter > 0) {
        length_counter--;
    }
}

// DMC methods
void APU::DMC::clock_timer() {
    if (timer_counter == 0) {
        timer_counter = timer_period;
        
        if (!silence) {
            if (shift_register & 0x01) {
                if (output_level <= 125) output_level += 2;
            } else {
                if (output_level >= 2) output_level -= 2;
            }
        }
        
        shift_register >>= 1;
        bits_remaining--;
        
        if (bits_remaining == 0) {
            bits_remaining = 8;
            if (sample_buffer_empty) {
                silence = true;
            } else {
                silence = false;
                shift_register = sample_buffer;
                sample_buffer_empty = true;
                
                // Load next sample
                if (bytes_remaining > 0) {
                    if (read_callback) {
                        sample_buffer = read_callback(current_address);
                        sample_buffer_empty = false;
                    }
                    current_address++;
                    if (current_address == 0x0000) current_address = 0x8000;
                    bytes_remaining--;
                    
                    if (bytes_remaining == 0) {
                        if (loop) {
                            start_sample();
                        }
                    }
                }
            }
        }
    } else {
        timer_counter--;
    }
}

void APU::DMC::start_sample() {
    current_address = sample_address;
    bytes_remaining = sample_length;
}

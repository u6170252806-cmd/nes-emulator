#pragma once

#include <cstdint>
#include <array>

/**
 * NES APU (Audio Processing Unit) - Cycle Accurate
 * 
 * The APU runs at CPU clock (1.789773 MHz NTSC)
 * 5 audio channels:
 * - Pulse 1 (square wave with sweep)
 * - Pulse 2 (square wave with sweep)
 * - Triangle (triangle wave)
 * - Noise (pseudo-random noise)
 * - DMC (Delta Modulation Channel - sample playback)
 * 
 * Frame counter runs at 240 Hz (quarter frame) or 192 Hz (half frame)
 */
class APU {
public:
    APU();
    
    // Reset APU to initial state
    void reset();
    
    // Execute one APU cycle (CPU clock rate)
    void clock();
    
    // CPU interface - APU registers ($4000-$4017)
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t data);
    
    // Get audio sample (mixed output, -1.0 to 1.0)
    float get_output_sample();
    
    // DMC needs to read from CPU memory
    void set_dmc_read_callback(uint8_t (*callback)(uint16_t));

private:
    // Pulse channel (2 instances)
    struct Pulse {
        bool enabled;
        uint8_t duty_cycle;
        bool halt_length;
        bool constant_volume;
        uint8_t volume;
        
        // Sweep unit
        bool sweep_enabled;
        uint8_t sweep_period;
        bool sweep_negate;
        uint8_t sweep_shift;
        uint8_t sweep_counter;
        bool sweep_reload;
        
        // Timer
        uint16_t timer_period;
        uint16_t timer_counter;
        
        // Length counter
        uint8_t length_counter;
        
        // Envelope
        uint8_t envelope_counter;
        uint8_t envelope_volume;
        bool envelope_start;
        
        // Sequencer
        uint8_t sequence_counter;
        
        uint8_t output;
        
        void clock_timer();
        void clock_envelope();
        void clock_sweep(bool channel_one);
        void clock_length();
    };
    
    // Triangle channel
    struct Triangle {
        bool enabled;
        bool control_flag;
        uint8_t linear_counter_load;
        
        uint16_t timer_period;
        uint16_t timer_counter;
        
        uint8_t length_counter;
        uint8_t linear_counter;
        bool linear_counter_reload;
        
        uint8_t sequence_counter;
        uint8_t output;
        
        void clock_timer();
        void clock_linear_counter();
        void clock_length();
    };
    
    // Noise channel
    struct Noise {
        bool enabled;
        bool halt_length;
        bool constant_volume;
        uint8_t volume;
        
        bool mode;
        uint16_t timer_period;
        uint16_t timer_counter;
        
        uint8_t length_counter;
        
        uint8_t envelope_counter;
        uint8_t envelope_volume;
        bool envelope_start;
        
        uint16_t shift_register;
        uint8_t output;
        
        void clock_timer();
        void clock_envelope();
        void clock_length();
    };
    
    // DMC (Delta Modulation Channel)
    struct DMC {
        bool enabled;
        bool irq_enabled;
        bool loop;
        uint8_t rate;
        
        uint8_t output_level;
        uint16_t sample_address;
        uint16_t sample_length;
        
        uint16_t current_address;
        uint16_t bytes_remaining;
        
        uint8_t sample_buffer;
        bool sample_buffer_empty;
        
        uint8_t shift_register;
        uint8_t bits_remaining;
        bool silence;
        
        uint16_t timer_period;
        uint16_t timer_counter;
        
        uint8_t (*read_callback)(uint16_t);
        
        void clock_timer();
        void start_sample();
    };
    
    Pulse pulse1;
    Pulse pulse2;
    Triangle triangle;
    Noise noise;
    DMC dmc;
    
    // Frame counter
    bool frame_counter_mode;  // false = 4-step, true = 5-step
    bool irq_inhibit;
    uint16_t frame_counter;
    uint8_t frame_step;
    
    // Cycle counter
    uint64_t cycles;
    
    // Clock frame counter and length/envelope counters
    void clock_frame_counter();
    void clock_quarter_frame();
    void clock_half_frame();
    
    // Lookup tables
    static const uint8_t length_table[32];
    static const uint16_t noise_period_table[16];
    static const uint16_t dmc_rate_table[16];
    static const uint8_t duty_table[4][8];
    static const uint8_t triangle_sequence[32];
    
    // Mixing
    float pulse_table[31];
    float tnd_table[203];
    void init_mixer_tables();
};

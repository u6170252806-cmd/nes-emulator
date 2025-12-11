#pragma once

#include <cstdint>
#include <functional>

// Forward declaration
class Bus;

/**
 * 6502 CPU Emulation - Cycle Accurate Implementation
 * 
 * TECHNICAL SPECIFICATIONS:
 * - Clock Speed: 1.789773 MHz (NTSC), 1.662607 MHz (PAL)
 * - Data Bus: 8-bit
 * - Address Bus: 16-bit (64KB address space)
 * - Registers: A (accumulator), X, Y (index), SP (stack), PC (program counter), P (status)
 * - Stack: 256 bytes at $0100-$01FF, grows downward
 * 
 * INSTRUCTION TIMING:
 * - Each instruction takes 2-7 cycles
 * - Page boundary crosses add 1 cycle for some instructions
 * - Branch instructions add 1-2 cycles when taken
 * 
 * INTERRUPT HANDLING:
 * - NMI: Non-maskable, triggered by PPU VBlank
 * - IRQ: Maskable via I flag, triggered by mappers (e.g., MMC3)
 * - BRK: Software interrupt
 * 
 * ADDRESSING MODES (12 total):
 * - Implied, Immediate, Zero Page, Zero Page X/Y
 * - Absolute, Absolute X/Y, Indirect
 * - Indexed Indirect (X), Indirect Indexed (Y), Relative
 * 
 * OPCODES:
 * - 151 official opcodes (56 unique instructions)
 * - 105 illegal/undocumented opcodes (used by some games)
 */
class CPU {
public:
    explicit CPU(Bus* bus);
    
    // Reset the CPU to initial state
    void reset();
    
    // Execute one CPU cycle (not instruction - instructions take multiple cycles)
    void clock();
    
    // Trigger interrupts
    void irq();  // Maskable interrupt
    void nmi();  // Non-maskable interrupt
    
    // ===== TIMING AND STATE INSPECTION =====
    
    // Get total cycles executed since power-on/reset
    uint64_t get_cycles() const { return total_cycles; }
    
    // Check if current instruction is complete (for debugging/stepping)
    bool is_instruction_complete() const { return cycles_remaining == 0; }
    
    // Get cycles remaining for current instruction
    uint8_t get_cycles_remaining() const { return cycles_remaining; }
    
    // Get current opcode being executed (for debugging/disassembly)
    uint8_t get_current_opcode() const { return opcode; }
    
    // Get current effective address (for debugging)
    uint16_t get_addr_abs() const { return addr_abs; }
    
    // Get current relative address (for branch debugging)
    uint16_t get_addr_rel() const { return addr_rel; }
    
    // Get fetched data (for debugging)
    uint8_t get_fetched() const { return fetched; }
    
    // ===== FLAG INSPECTION (for debugging) =====
    
    bool get_carry() const { return get_flag(C); }
    bool get_zero() const { return get_flag(Z); }
    bool get_interrupt_disable() const { return get_flag(I); }
    bool get_decimal() const { return get_flag(D); }
    bool get_break() const { return get_flag(B); }
    bool get_overflow() const { return get_flag(V); }
    bool get_negative() const { return get_flag(N); }
    
    // Registers (public for debugging/testing)
    uint8_t A;      // Accumulator
    uint8_t X;      // X register
    uint8_t Y;      // Y register
    uint8_t SP;     // Stack pointer
    uint16_t PC;    // Program counter
    uint8_t P;      // Status register
    
    // Status flags
    enum Flags {
        C = (1 << 0),  // Carry
        Z = (1 << 1),  // Zero
        I = (1 << 2),  // Interrupt disable
        D = (1 << 3),  // Decimal mode (not used on NES)
        B = (1 << 4),  // Break
        U = (1 << 5),  // Unused (always 1)
        V = (1 << 6),  // Overflow
        N = (1 << 7),  // Negative
    };

private:
    Bus* bus;
    
    // Cycle tracking
    uint64_t total_cycles;
    uint8_t cycles_remaining;  // Cycles left for current instruction
    
    // Instruction execution state
    uint16_t addr_abs;    // Absolute address for current instruction
    uint16_t addr_rel;    // Relative address for branches
    uint8_t opcode;       // Current opcode
    uint8_t fetched;      // Fetched data
    
    // Memory access
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t data);
    
    // Stack operations
    void push(uint8_t data);
    uint8_t pop();
    void push16(uint16_t data);
    uint16_t pop16();
    
    // Flag operations
    void set_flag(Flags flag, bool value);
    bool get_flag(Flags flag) const;
    
    // Addressing modes (return 1 if extra cycle needed on page boundary cross)
    uint8_t IMP(); uint8_t IMM(); uint8_t ZP0(); uint8_t ZPX();
    uint8_t ZPY(); uint8_t REL(); uint8_t ABS(); uint8_t ABX();
    uint8_t ABY(); uint8_t IND(); uint8_t IZX(); uint8_t IZY();
    
    // Opcodes (return 1 if extra cycle may be needed)
    uint8_t ADC(); uint8_t AND(); uint8_t ASL(); uint8_t BCC();
    uint8_t BCS(); uint8_t BEQ(); uint8_t BIT(); uint8_t BMI();
    uint8_t BNE(); uint8_t BPL(); uint8_t BRK(); uint8_t BVC();
    uint8_t BVS(); uint8_t CLC(); uint8_t CLD(); uint8_t CLI();
    uint8_t CLV(); uint8_t CMP(); uint8_t CPX(); uint8_t CPY();
    uint8_t DEC(); uint8_t DEX(); uint8_t DEY(); uint8_t EOR();
    uint8_t INC(); uint8_t INX(); uint8_t INY(); uint8_t JMP();
    uint8_t JSR(); uint8_t LDA(); uint8_t LDX(); uint8_t LDY();
    uint8_t LSR(); uint8_t NOP(); uint8_t ORA(); uint8_t PHA();
    uint8_t PHP(); uint8_t PLA(); uint8_t PLP(); uint8_t ROL();
    uint8_t ROR(); uint8_t RTI(); uint8_t RTS(); uint8_t SBC();
    uint8_t SEC(); uint8_t SED(); uint8_t SEI(); uint8_t STA();
    uint8_t STX(); uint8_t STY(); uint8_t TAX(); uint8_t TAY();
    uint8_t TSX(); uint8_t TXA(); uint8_t TXS(); uint8_t TYA();
    
    // ===== ILLEGAL/UNDOCUMENTED OPCODES =====
    // These opcodes are not officially documented but exist in the 6502
    // Many games (especially unlicensed) use these for various purposes
    // 
    // CATEGORIES:
    // 1. Combined operations (SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC)
    // 2. Immediate operations (ANC, ALR, ARR, XAA, AXS)
    // 3. Unstable operations (AHX, SHY, SHX, TAS, LAS)
    // 4. NOPs with various addressing modes
    // 5. JAM/KIL - halt the CPU
    
    uint8_t XXX();  // Unknown/illegal - acts as NOP
    uint8_t JAM();  // Halt CPU (also known as KIL, HLT)
    
    // Combined operations (perform two operations in one instruction)
    uint8_t SLO();  // ASL + ORA (Shift Left then OR with Accumulator)
    uint8_t RLA();  // ROL + AND (Rotate Left then AND with Accumulator)
    uint8_t SRE();  // LSR + EOR (Shift Right then XOR with Accumulator)
    uint8_t RRA();  // ROR + ADC (Rotate Right then Add with Carry)
    uint8_t SAX();  // Store A & X (no flags affected)
    uint8_t LAX();  // Load A and X with same value
    uint8_t DCP();  // DEC + CMP (Decrement then Compare)
    uint8_t ISC();  // INC + SBC (Increment then Subtract, also known as ISB)
    
    // Immediate mode operations
    uint8_t ANC();  // AND + set C to N (AND with immediate, copy N to C)
    uint8_t ALR();  // AND + LSR (AND with immediate, then shift right)
    uint8_t ARR();  // AND + ROR (AND with immediate, then rotate right)
    uint8_t XAA();  // TXA + AND (unstable, behavior varies)
    uint8_t AXS();  // (A & X) - immediate -> X (also known as SBX)
    
    // Unstable operations (behavior may vary between chips)
    uint8_t AHX();  // Store A & X & (high byte + 1) (also known as SHA)
    uint8_t SHY();  // Store Y & (high byte + 1)
    uint8_t SHX();  // Store X & (high byte + 1)
    uint8_t TAS();  // SP = A & X, then store A & X & (high byte + 1)
    uint8_t LAS();  // A, X, SP = memory & SP
    
    // Instruction table entry
    struct Instruction {
        const char* name;
        uint8_t (CPU::*operate)();
        uint8_t (CPU::*addrmode)();
        uint8_t cycles;
    };
    
    static const Instruction instruction_table[256];
    
    // Fetch data for current instruction
    uint8_t fetch();
};

#include "cpu.hpp"
#include "bus.hpp"
#include <cstring>

CPU::CPU(Bus* bus) : bus(bus), total_cycles(0), cycles_remaining(0) {
    reset();
}

void CPU::reset() {
    // ===== CPU RESET SEQUENCE =====
    // Triggered by: Power-on, Reset button
    // Takes 8 cycles to complete
    // 
    // RESET BEHAVIOR:
    // - Loads PC from reset vector at $FFFC-$FFFD
    // - Sets I flag (disables IRQ)
    // - Sets U flag (always 1)
    // - Stack pointer set to $FD (not $FF like some docs say)
    // - A, X, Y are NOT initialized (undefined state, but we zero them)
    // - Does NOT clear decimal mode (but NES doesn't use it anyway)
    
    // Load program counter from reset vector
    uint16_t lo = read(0xFFFC);
    uint16_t hi = read(0xFFFD);
    PC = (hi << 8) | lo;
    
    // Initialize registers to power-on state
    A = 0x00;   // Accumulator (technically undefined, but games expect 0)
    X = 0x00;   // X index register (technically undefined)
    Y = 0x00;   // Y index register (technically undefined)
    SP = 0xFD;  // Stack pointer (starts at $FD, not $FF)
    
    // Initialize status register
    // I flag set (interrupts disabled)
    // U flag set (always 1)
    // All other flags cleared
    P = 0x00 | U | I;
    
    // Clear internal state
    addr_abs = 0x0000;
    addr_rel = 0x0000;
    fetched = 0x00;
    
    // Reset sequence takes 8 cycles
    cycles_remaining = 8;
}

void CPU::clock() {
    // ===== 6502 CPU CYCLE EXECUTION =====
    // The 6502 is a multi-cycle processor - each instruction takes 2-7 cycles
    // This function represents ONE CPU cycle (not one instruction)
    // 
    // TIMING DETAILS:
    // - CPU runs at 1.789773 MHz (NTSC) = ~559 nanoseconds per cycle
    // - PPU runs 3x faster (3 PPU cycles per 1 CPU cycle)
    // - Instructions execute in phases: fetch, decode, address calculation, execution
    // 
    // CYCLE STEALING:
    // - DMA transfers steal CPU cycles (handled in Bus)
    // - DMC audio can steal cycles for sample fetching
    // - Interrupts add 7-8 cycles when triggered
    
    if (cycles_remaining == 0) {
        // ===== INSTRUCTION FETCH PHASE =====
        // Fetch the next opcode from memory at PC
        // This takes 1 cycle (the first cycle of the instruction)
        opcode = read(PC);
        
        // HARDWARE QUIRK: The unused flag (bit 5) is always set to 1
        // This is because the 6502's status register physically has bit 5 tied high
        // When you push P to stack or read it, bit 5 is always 1
        set_flag(U, true);
        
        // Increment program counter to point to next byte
        // PC now points to the first operand byte (if any)
        PC++;
        
        // Look up instruction details from the opcode table
        // This table contains all 256 possible opcodes (official + illegal)
        const Instruction& instr = instruction_table[opcode];
        
        // Set base cycle count for this instruction
        // This is the minimum cycles the instruction will take
        cycles_remaining = instr.cycles;
        
        // ===== ADDRESSING MODE PHASE =====
        // Execute the addressing mode to calculate the effective address
        // Addressing modes determine HOW to access the operand
        // Examples: immediate (#$10), absolute ($2000), indexed ($2000,X)
        // 
        // Returns 1 if a page boundary was crossed (may add extra cycle)
        // Page boundary = when high byte of address changes (e.g., $20FF -> $2100)
        uint8_t addr_mode_extra_cycle = (this->*instr.addrmode)();
        
        // ===== EXECUTION PHASE =====
        // Execute the actual operation (the instruction's behavior)
        // Examples: ADC adds to accumulator, STA stores accumulator, JMP jumps
        // 
        // Returns 1 if the instruction CAN take an extra cycle
        // Not all instructions that cross pages actually take the extra cycle
        // Only read instructions (LDA, ADC, etc.) take it, not writes (STA, etc.)
        uint8_t operation_extra_cycle = (this->*instr.operate)();
        
        // ===== CYCLE ADJUSTMENT =====
        // Some instructions take an extra cycle when:
        // 1. A page boundary is crossed during addressing (addr_mode_extra_cycle)
        // 2. The instruction itself requires it (operation_extra_cycle)
        // Both conditions must be true (AND operation)
        // 
        // Example: LDA $2000,X with X=$FF crosses page boundary -> +1 cycle
        // Example: STA $2000,X with X=$FF crosses page boundary -> NO extra cycle
        cycles_remaining += (addr_mode_extra_cycle & operation_extra_cycle);
        
        // Ensure unused flag remains set (hardware quirk)
        set_flag(U, true);
    }
    
    // Decrement remaining cycles for current instruction
    // When this reaches 0, the instruction is complete
    cycles_remaining--;
    
    // Increment total cycle counter (for timing/debugging)
    // This tracks the total number of CPU cycles since power-on
    // Used for: timing synchronization, performance profiling, debugging
    total_cycles++;
}

void CPU::irq() {
    // ===== IRQ (Interrupt Request) - Maskable Interrupt =====
    // Triggered by: Mapper IRQs (e.g., MMC3 scanline counter), APU frame counter
    // Can be disabled by setting the I (Interrupt Disable) flag
    // Takes 7 cycles to complete
    
    // Check if interrupts are enabled (I flag clear)
    if (get_flag(I) == 0) {
        // Save current program counter to stack (return address)
        push16(PC);
        
        // Push processor status to stack
        // B flag is cleared (distinguishes IRQ from BRK)
        // U flag is always set (hardware quirk)
        // I flag is set (disable further interrupts during handler)
        set_flag(B, false);
        set_flag(U, true);
        set_flag(I, true);
        push(P);
        
        // Load IRQ handler address from vector at $FFFE-$FFFF
        uint16_t lo = read(0xFFFE);
        uint16_t hi = read(0xFFFF);
        PC = (hi << 8) | lo;
        
        // IRQ sequence takes 7 cycles
        cycles_remaining = 7;
    }
}

void CPU::nmi() {
    // ===== NMI (Non-Maskable Interrupt) - Cannot be disabled =====
    // Triggered by: PPU VBlank (if enabled in PPU control register)
    // Cannot be disabled by I flag
    // Takes 8 cycles to complete
    // Used for: Frame timing, game logic updates during VBlank
    
    // Save current program counter to stack (return address)
    push16(PC);
    
    // Push processor status to stack
    // B flag is cleared (distinguishes NMI from BRK)
    // U flag is always set (hardware quirk)
    // I flag is set (disable IRQs during NMI handler)
    set_flag(B, false);
    set_flag(U, true);
    set_flag(I, true);
    push(P);
    
    // Load NMI handler address from vector at $FFFA-$FFFB
    uint16_t lo = read(0xFFFA);
    uint16_t hi = read(0xFFFB);
    PC = (hi << 8) | lo;
    
    // NMI sequence takes 8 cycles
    cycles_remaining = 8;
}

uint8_t CPU::read(uint16_t addr) {
    return bus->cpu_read(addr);
}

void CPU::write(uint16_t addr, uint8_t data) {
    bus->cpu_write(addr, data);
}

void CPU::push(uint8_t data) {
    write(0x0100 + SP, data);
    SP--;
}

uint8_t CPU::pop() {
    SP++;
    return read(0x0100 + SP);
}

void CPU::push16(uint16_t data) {
    push((data >> 8) & 0xFF);
    push(data & 0xFF);
}

uint16_t CPU::pop16() {
    uint16_t lo = pop();
    uint16_t hi = pop();
    return (hi << 8) | lo;
}

void CPU::set_flag(Flags flag, bool value) {
    if (value)
        P |= flag;
    else
        P &= ~flag;
}

bool CPU::get_flag(Flags flag) const {
    return (P & flag) != 0;
}

uint8_t CPU::fetch() {
    if (instruction_table[opcode].addrmode != &CPU::IMP)
        fetched = read(addr_abs);
    return fetched;
}

// ============================================================================
// ADDRESSING MODES - 12 modes total
// ============================================================================
// Each addressing mode calculates the effective address for the instruction
// Returns 1 if a page boundary was crossed (may add extra cycle)
// Returns 0 otherwise

uint8_t CPU::IMP() {
    // IMPLIED - No operand needed, instruction operates on accumulator or flags
    // Examples: TAX, INX, CLC, RTS
    // Cycles: 0 extra
    // For accumulator operations, we pre-fetch A into fetched
    fetched = A;
    return 0;
}

uint8_t CPU::IMM() {
    // IMMEDIATE - Operand is the next byte after opcode
    // Format: OP #$nn
    // Examples: LDA #$05, ADC #$10
    // Cycles: 0 extra
    // Address points directly to PC (the immediate value)
    addr_abs = PC++;
    return 0;
}

uint8_t CPU::ZP0() {
    // ZERO PAGE - Address is in zero page ($0000-$00FF)
    // Format: OP $nn
    // Examples: LDA $80, STA $10
    // Cycles: 0 extra
    // Faster than absolute addressing (only 1 byte address)
    // Commonly used for frequently accessed variables
    addr_abs = read(PC);
    PC++;
    addr_abs &= 0x00FF;  // Ensure it stays in zero page
    return 0;
}

uint8_t CPU::ZPX() {
    // Zero Page, X - address is in zero page, indexed by X
    addr_abs = (read(PC) + X);
    PC++;
    addr_abs &= 0x00FF;
    return 0;
}

uint8_t CPU::ZPY() {
    // Zero Page, Y - address is in zero page, indexed by Y
    addr_abs = (read(PC) + Y);
    PC++;
    addr_abs &= 0x00FF;
    return 0;
}

uint8_t CPU::REL() {
    // Relative - for branch instructions
    addr_rel = read(PC);
    PC++;
    if (addr_rel & 0x80)
        addr_rel |= 0xFF00;  // Sign extend
    return 0;
}

uint8_t CPU::ABS() {
    // Absolute - full 16-bit address
    uint16_t lo = read(PC);
    PC++;
    uint16_t hi = read(PC);
    PC++;
    addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU::ABX() {
    // Absolute, X - full 16-bit address indexed by X
    uint16_t lo = read(PC);
    PC++;
    uint16_t hi = read(PC);
    PC++;
    addr_abs = (hi << 8) | lo;
    addr_abs += X;
    
    // Extra cycle if page boundary crossed
    return ((addr_abs & 0xFF00) != (hi << 8)) ? 1 : 0;
}

uint8_t CPU::ABY() {
    // Absolute, Y - full 16-bit address indexed by Y
    uint16_t lo = read(PC);
    PC++;
    uint16_t hi = read(PC);
    PC++;
    addr_abs = (hi << 8) | lo;
    addr_abs += Y;
    
    // Extra cycle if page boundary crossed
    return ((addr_abs & 0xFF00) != (hi << 8)) ? 1 : 0;
}

uint8_t CPU::IND() {
    // Indirect - used only by JMP
    uint16_t ptr_lo = read(PC);
    PC++;
    uint16_t ptr_hi = read(PC);
    PC++;
    uint16_t ptr = (ptr_hi << 8) | ptr_lo;
    
    // Hardware bug: if low byte is 0xFF, high byte wraps within same page
    if (ptr_lo == 0x00FF) {
        addr_abs = (read(ptr & 0xFF00) << 8) | read(ptr);
    } else {
        addr_abs = (read(ptr + 1) << 8) | read(ptr);
    }
    
    return 0;
}

uint8_t CPU::IZX() {
    // Indexed Indirect - (zero page, X)
    uint16_t t = read(PC);
    PC++;
    
    uint16_t lo = read((t + X) & 0x00FF);
    uint16_t hi = read((t + X + 1) & 0x00FF);
    
    addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU::IZY() {
    // Indirect Indexed - (zero page), Y
    uint16_t t = read(PC);
    PC++;
    
    uint16_t lo = read(t & 0x00FF);
    uint16_t hi = read((t + 1) & 0x00FF);
    
    addr_abs = (hi << 8) | lo;
    addr_abs += Y;
    
    // Extra cycle if page boundary crossed
    return ((addr_abs & 0xFF00) != (hi << 8)) ? 1 : 0;
}

// ============================================================================
// OPCODES - OFFICIAL INSTRUCTIONS
// ============================================================================

uint8_t CPU::ADC() {
    // Add with Carry
    fetch();
    uint16_t temp = (uint16_t)A + (uint16_t)fetched + (uint16_t)get_flag(C);
    set_flag(C, temp > 255);
    set_flag(Z, (temp & 0x00FF) == 0);
    set_flag(V, (~((uint16_t)A ^ (uint16_t)fetched) & ((uint16_t)A ^ (uint16_t)temp)) & 0x0080);
    set_flag(N, temp & 0x80);
    A = temp & 0xFF;
    return 1;
}

uint8_t CPU::AND() {
    // Logical AND
    fetch();
    A = A & fetched;
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    return 1;
}

uint8_t CPU::ASL() {
    // Arithmetic Shift Left
    fetch();
    uint16_t temp = (uint16_t)fetched << 1;
    set_flag(C, (temp & 0xFF00) > 0);
    set_flag(Z, (temp & 0x00FF) == 0x00);
    set_flag(N, temp & 0x80);
    if (instruction_table[opcode].addrmode == &CPU::IMP)
        A = temp & 0xFF;
    else
        write(addr_abs, temp & 0xFF);
    return 0;
}

uint8_t CPU::BCC() {
    // Branch if Carry Clear
    if (get_flag(C) == 0) {
        cycles_remaining++;
        addr_abs = PC + addr_rel;
        
        if ((addr_abs & 0xFF00) != (PC & 0xFF00))
            cycles_remaining++;
        
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BCS() {
    // Branch if Carry Set
    if (get_flag(C) == 1) {
        cycles_remaining++;
        addr_abs = PC + addr_rel;
        
        if ((addr_abs & 0xFF00) != (PC & 0xFF00))
            cycles_remaining++;
        
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BEQ() {
    // Branch if Equal (Zero Set)
    if (get_flag(Z) == 1) {
        cycles_remaining++;
        addr_abs = PC + addr_rel;
        
        if ((addr_abs & 0xFF00) != (PC & 0xFF00))
            cycles_remaining++;
        
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BIT() {
    // Bit Test
    fetch();
    uint8_t temp = A & fetched;
    set_flag(Z, (temp & 0x00FF) == 0x00);
    set_flag(N, fetched & (1 << 7));
    set_flag(V, fetched & (1 << 6));
    return 0;
}

uint8_t CPU::BMI() {
    // Branch if Minus (Negative Set)
    if (get_flag(N) == 1) {
        cycles_remaining++;
        addr_abs = PC + addr_rel;
        
        if ((addr_abs & 0xFF00) != (PC & 0xFF00))
            cycles_remaining++;
        
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BNE() {
    // Branch if Not Equal (Zero Clear)
    if (get_flag(Z) == 0) {
        cycles_remaining++;
        addr_abs = PC + addr_rel;
        
        if ((addr_abs & 0xFF00) != (PC & 0xFF00))
            cycles_remaining++;
        
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BPL() {
    // Branch if Positive (Negative Clear)
    if (get_flag(N) == 0) {
        cycles_remaining++;
        addr_abs = PC + addr_rel;
        
        if ((addr_abs & 0xFF00) != (PC & 0xFF00))
            cycles_remaining++;
        
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BRK() {
    // Break
    PC++;
    
    set_flag(I, true);
    push16(PC);
    
    set_flag(B, true);
    push(P);
    set_flag(B, false);
    
    PC = (uint16_t)read(0xFFFE) | ((uint16_t)read(0xFFFF) << 8);
    return 0;
}

uint8_t CPU::BVC() {
    // Branch if Overflow Clear
    if (get_flag(V) == 0) {
        cycles_remaining++;
        addr_abs = PC + addr_rel;
        
        if ((addr_abs & 0xFF00) != (PC & 0xFF00))
            cycles_remaining++;
        
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::BVS() {
    // Branch if Overflow Set
    if (get_flag(V) == 1) {
        cycles_remaining++;
        addr_abs = PC + addr_rel;
        
        if ((addr_abs & 0xFF00) != (PC & 0xFF00))
            cycles_remaining++;
        
        PC = addr_abs;
    }
    return 0;
}

uint8_t CPU::CLC() { set_flag(C, false); return 0; }
uint8_t CPU::CLD() { set_flag(D, false); return 0; }
uint8_t CPU::CLI() { set_flag(I, false); return 0; }
uint8_t CPU::CLV() { set_flag(V, false); return 0; }

uint8_t CPU::CMP() {
    // Compare Accumulator
    fetch();
    uint16_t temp = (uint16_t)A - (uint16_t)fetched;
    set_flag(C, A >= fetched);
    set_flag(Z, (temp & 0x00FF) == 0x0000);
    set_flag(N, temp & 0x0080);
    return 1;
}

uint8_t CPU::CPX() {
    // Compare X Register
    fetch();
    uint16_t temp = (uint16_t)X - (uint16_t)fetched;
    set_flag(C, X >= fetched);
    set_flag(Z, (temp & 0x00FF) == 0x0000);
    set_flag(N, temp & 0x0080);
    return 0;
}

uint8_t CPU::CPY() {
    // Compare Y Register
    fetch();
    uint16_t temp = (uint16_t)Y - (uint16_t)fetched;
    set_flag(C, Y >= fetched);
    set_flag(Z, (temp & 0x00FF) == 0x0000);
    set_flag(N, temp & 0x0080);
    return 0;
}

uint8_t CPU::DEC() {
    // Decrement Memory
    fetch();
    uint16_t temp = fetched - 1;
    write(addr_abs, temp & 0x00FF);
    set_flag(Z, (temp & 0x00FF) == 0x0000);
    set_flag(N, temp & 0x0080);
    return 0;
}

uint8_t CPU::DEX() {
    // Decrement X Register
    X--;
    set_flag(Z, X == 0x00);
    set_flag(N, X & 0x80);
    return 0;
}

uint8_t CPU::DEY() {
    // Decrement Y Register
    Y--;
    set_flag(Z, Y == 0x00);
    set_flag(N, Y & 0x80);
    return 0;
}

uint8_t CPU::EOR() {
    // Exclusive OR
    fetch();
    A = A ^ fetched;
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    return 1;
}

uint8_t CPU::INC() {
    // Increment Memory
    fetch();
    uint16_t temp = fetched + 1;
    write(addr_abs, temp & 0x00FF);
    set_flag(Z, (temp & 0x00FF) == 0x0000);
    set_flag(N, temp & 0x0080);
    return 0;
}

uint8_t CPU::INX() {
    // Increment X Register
    X++;
    set_flag(Z, X == 0x00);
    set_flag(N, X & 0x80);
    return 0;
}

uint8_t CPU::INY() {
    // Increment Y Register
    Y++;
    set_flag(Z, Y == 0x00);
    set_flag(N, Y & 0x80);
    return 0;
}

uint8_t CPU::JMP() {
    // Jump
    PC = addr_abs;
    return 0;
}

uint8_t CPU::JSR() {
    // Jump to Subroutine
    PC--;
    push16(PC);
    PC = addr_abs;
    return 0;
}

uint8_t CPU::LDA() {
    // Load Accumulator
    fetch();
    A = fetched;
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    return 1;
}

uint8_t CPU::LDX() {
    // Load X Register
    fetch();
    X = fetched;
    set_flag(Z, X == 0x00);
    set_flag(N, X & 0x80);
    return 1;
}

uint8_t CPU::LDY() {
    // Load Y Register
    fetch();
    Y = fetched;
    set_flag(Z, Y == 0x00);
    set_flag(N, Y & 0x80);
    return 1;
}

uint8_t CPU::LSR() {
    // Logical Shift Right
    fetch();
    set_flag(C, fetched & 0x0001);
    uint16_t temp = fetched >> 1;
    set_flag(Z, (temp & 0x00FF) == 0x0000);
    set_flag(N, temp & 0x0080);
    if (instruction_table[opcode].addrmode == &CPU::IMP)
        A = temp & 0xFF;
    else
        write(addr_abs, temp & 0xFF);
    return 0;
}

uint8_t CPU::NOP() {
    // No Operation
    // Some illegal NOPs may require extra cycles
    switch (opcode) {
        case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
            return 1;
    }
    return 0;
}

uint8_t CPU::ORA() {
    // Logical OR
    fetch();
    A = A | fetched;
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    return 1;
}

uint8_t CPU::PHA() {
    // Push Accumulator
    push(A);
    return 0;
}

uint8_t CPU::PHP() {
    // Push Processor Status
    push(P | B | U);
    set_flag(B, false);
    set_flag(U, false);
    return 0;
}

uint8_t CPU::PLA() {
    // Pull Accumulator
    A = pop();
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    return 0;
}

uint8_t CPU::PLP() {
    // Pull Processor Status
    P = pop();
    set_flag(U, true);
    return 0;
}

uint8_t CPU::ROL() {
    // Rotate Left
    fetch();
    uint16_t temp = (uint16_t)(fetched << 1) | get_flag(C);
    set_flag(C, temp & 0xFF00);
    set_flag(Z, (temp & 0x00FF) == 0x0000);
    set_flag(N, temp & 0x0080);
    if (instruction_table[opcode].addrmode == &CPU::IMP)
        A = temp & 0xFF;
    else
        write(addr_abs, temp & 0xFF);
    return 0;
}

uint8_t CPU::ROR() {
    // Rotate Right
    fetch();
    uint16_t temp = (uint16_t)(get_flag(C) << 7) | (fetched >> 1);
    set_flag(C, fetched & 0x01);
    set_flag(Z, (temp & 0x00FF) == 0x00);
    set_flag(N, temp & 0x0080);
    if (instruction_table[opcode].addrmode == &CPU::IMP)
        A = temp & 0xFF;
    else
        write(addr_abs, temp & 0xFF);
    return 0;
}

uint8_t CPU::RTI() {
    // Return from Interrupt
    P = pop();
    set_flag(B, false);
    set_flag(U, false);
    PC = pop16();
    return 0;
}

uint8_t CPU::RTS() {
    // Return from Subroutine
    PC = pop16();
    PC++;
    return 0;
}

uint8_t CPU::SBC() {
    // Subtract with Carry
    fetch();
    uint16_t value = ((uint16_t)fetched) ^ 0x00FF;
    uint16_t temp = (uint16_t)A + value + (uint16_t)get_flag(C);
    set_flag(C, temp & 0xFF00);
    set_flag(Z, ((temp & 0x00FF) == 0));
    set_flag(V, (temp ^ (uint16_t)A) & (temp ^ value) & 0x0080);
    set_flag(N, temp & 0x0080);
    A = temp & 0x00FF;
    return 1;
}

uint8_t CPU::SEC() { set_flag(C, true); return 0; }
uint8_t CPU::SED() { set_flag(D, true); return 0; }
uint8_t CPU::SEI() { set_flag(I, true); return 0; }

uint8_t CPU::STA() {
    // Store Accumulator
    write(addr_abs, A);
    return 0;
}

uint8_t CPU::STX() {
    // Store X Register
    write(addr_abs, X);
    return 0;
}

uint8_t CPU::STY() {
    // Store Y Register
    write(addr_abs, Y);
    return 0;
}

uint8_t CPU::TAX() {
    // Transfer A to X
    X = A;
    set_flag(Z, X == 0x00);
    set_flag(N, X & 0x80);
    return 0;
}

uint8_t CPU::TAY() {
    // Transfer A to Y
    Y = A;
    set_flag(Z, Y == 0x00);
    set_flag(N, Y & 0x80);
    return 0;
}

uint8_t CPU::TSX() {
    // Transfer Stack Pointer to X
    X = SP;
    set_flag(Z, X == 0x00);
    set_flag(N, X & 0x80);
    return 0;
}

uint8_t CPU::TXA() {
    // Transfer X to A
    A = X;
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    return 0;
}

uint8_t CPU::TXS() {
    // Transfer X to Stack Pointer
    SP = X;
    return 0;
}

uint8_t CPU::TYA() {
    // Transfer Y to A
    A = Y;
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    return 0;
}

// ============================================================================
// ILLEGAL OPCODES - Properly implemented based on 6502 documentation
// ============================================================================
// These opcodes are not officially documented but exist in the 6502 hardware.
// Many games (especially unlicensed ones) use these for copy protection,
// optimization, or just because the programmers discovered them.
//
// CATEGORIES:
// 1. Combined operations: SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC
//    - Perform two operations in one instruction (faster than separate ops)
// 2. Immediate operations: ANC, ALR, ARR, XAA, AXS
//    - Various AND/shift combinations with immediate values
// 3. Unstable operations: AHX, SHY, SHX, TAS, LAS
//    - Behavior varies between CPU revisions, use with caution
// 4. JAM/KIL/HLT: Halt the CPU completely
//    - Used for copy protection or as a crash indicator
// 5. Various NOPs: Different cycle counts and addressing modes
//    - Used for timing or as padding

uint8_t CPU::XXX() {
    // Unknown/illegal opcode - NOP behavior
    // This is the fallback for any opcode not explicitly handled
    return 0;
}

uint8_t CPU::JAM() {
    // ===== JAM/KIL/HLT - Halt the CPU =====
    // Opcodes: $02, $12, $22, $32, $42, $52, $62, $72, $92, $B2, $D2, $F2
    // 
    // This instruction halts the CPU completely. The only way to recover
    // is to reset the system. On real hardware, the CPU enters an infinite
    // loop reading the same address over and over.
    //
    // USES:
    // - Copy protection (crash if tampered with)
    // - Debug breakpoints in development
    // - Indicating unrecoverable errors
    //
    // IMPLEMENTATION:
    // We decrement PC so the CPU keeps executing this same instruction,
    // effectively halting execution without crashing the emulator.
    PC--;
    return 0;
}

uint8_t CPU::SLO() {
    // ASL memory, then ORA with accumulator
    fetch();
    uint16_t temp = (uint16_t)fetched << 1;
    write(addr_abs, temp & 0xFF);
    set_flag(C, (temp & 0xFF00) > 0);
    A = A | (temp & 0xFF);
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    return 0;
}

uint8_t CPU::RLA() {
    // ROL memory, then AND with accumulator
    fetch();
    uint16_t temp = (uint16_t)(fetched << 1) | get_flag(C);
    write(addr_abs, temp & 0xFF);
    set_flag(C, temp & 0xFF00);
    A = A & (temp & 0xFF);
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    return 0;
}

uint8_t CPU::SRE() {
    // LSR memory, then EOR with accumulator
    fetch();
    set_flag(C, fetched & 0x01);
    uint8_t temp = fetched >> 1;
    write(addr_abs, temp);
    A = A ^ temp;
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    return 0;
}

uint8_t CPU::RRA() {
    // ROR memory, then ADC with accumulator
    fetch();
    uint8_t temp = (get_flag(C) << 7) | (fetched >> 1);
    set_flag(C, fetched & 0x01);
    write(addr_abs, temp);
    // Now ADC
    uint16_t sum = (uint16_t)A + (uint16_t)temp + (uint16_t)get_flag(C);
    set_flag(C, sum > 255);
    set_flag(Z, (sum & 0xFF) == 0);
    set_flag(V, (~((uint16_t)A ^ (uint16_t)temp) & ((uint16_t)A ^ sum)) & 0x0080);
    set_flag(N, sum & 0x80);
    A = sum & 0xFF;
    return 0;
}

uint8_t CPU::SAX() {
    // Store A & X to memory
    write(addr_abs, A & X);
    return 0;
}

uint8_t CPU::LAX() {
    // Load A and X with same value
    fetch();
    A = fetched;
    X = fetched;
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    return 1;
}

uint8_t CPU::DCP() {
    // DEC memory, then CMP with accumulator
    fetch();
    uint8_t temp = fetched - 1;
    write(addr_abs, temp);
    uint16_t cmp = (uint16_t)A - (uint16_t)temp;
    set_flag(C, A >= temp);
    set_flag(Z, (cmp & 0x00FF) == 0);
    set_flag(N, cmp & 0x0080);
    return 0;
}

uint8_t CPU::ISC() {
    // INC memory, then SBC with accumulator (also known as ISB)
    fetch();
    uint8_t temp = fetched + 1;
    write(addr_abs, temp);
    // Now SBC
    uint16_t value = ((uint16_t)temp) ^ 0x00FF;
    uint16_t sum = (uint16_t)A + value + (uint16_t)get_flag(C);
    set_flag(C, sum & 0xFF00);
    set_flag(Z, (sum & 0xFF) == 0);
    set_flag(V, (sum ^ (uint16_t)A) & (sum ^ value) & 0x0080);
    set_flag(N, sum & 0x0080);
    A = sum & 0xFF;
    return 0;
}

uint8_t CPU::ANC() {
    // AND with immediate, copy N to C
    fetch();
    A = A & fetched;
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    set_flag(C, A & 0x80);
    return 0;
}

uint8_t CPU::ALR() {
    // AND with immediate, then LSR A
    fetch();
    A = A & fetched;
    set_flag(C, A & 0x01);
    A = A >> 1;
    set_flag(Z, A == 0x00);
    set_flag(N, false);
    return 0;
}

uint8_t CPU::ARR() {
    // AND with immediate, then ROR A, weird flag behavior
    fetch();
    A = A & fetched;
    A = (get_flag(C) << 7) | (A >> 1);
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    set_flag(C, A & 0x40);
    set_flag(V, ((A & 0x40) ^ ((A & 0x20) << 1)) != 0);
    return 0;
}

uint8_t CPU::XAA() {
    // Highly unstable - TXA then AND immediate
    // Behavior varies between chips, this is approximate
    fetch();
    A = X & fetched;
    set_flag(Z, A == 0x00);
    set_flag(N, A & 0x80);
    return 0;
}

uint8_t CPU::AXS() {
    // (A & X) - immediate -> X, sets flags like CMP
    fetch();
    uint8_t temp = A & X;
    uint16_t result = (uint16_t)temp - (uint16_t)fetched;
    X = result & 0xFF;
    set_flag(C, temp >= fetched);
    set_flag(Z, X == 0);
    set_flag(N, X & 0x80);
    return 0;
}

uint8_t CPU::AHX() {
    // Store A & X & (high byte of addr + 1) - unstable
    uint8_t temp = A & X & ((addr_abs >> 8) + 1);
    write(addr_abs, temp);
    return 0;
}

uint8_t CPU::SHY() {
    // Store Y & (high byte of addr + 1) - unstable
    uint8_t temp = Y & ((addr_abs >> 8) + 1);
    write(addr_abs, temp);
    return 0;
}

uint8_t CPU::SHX() {
    // Store X & (high byte of addr + 1) - unstable
    uint8_t temp = X & ((addr_abs >> 8) + 1);
    write(addr_abs, temp);
    return 0;
}

uint8_t CPU::TAS() {
    // SP = A & X, then store A & X & (high byte + 1)
    SP = A & X;
    uint8_t temp = A & X & ((addr_abs >> 8) + 1);
    write(addr_abs, temp);
    return 0;
}

uint8_t CPU::LAS() {
    // A, X, SP = memory & SP
    fetch();
    uint8_t temp = fetched & SP;
    A = temp;
    X = temp;
    SP = temp;
    set_flag(Z, temp == 0x00);
    set_flag(N, temp & 0x80);
    return 1;
}

// ============================================================================
// INSTRUCTION TABLE
// ============================================================================

const CPU::Instruction CPU::instruction_table[256] = {
    {"BRK", &CPU::BRK, &CPU::IMM, 7},{"ORA", &CPU::ORA, &CPU::IZX, 6},{"JAM", &CPU::JAM, &CPU::IMP, 2},{"SLO", &CPU::SLO, &CPU::IZX, 8},{"NOP", &CPU::NOP, &CPU::ZP0, 3},{"ORA", &CPU::ORA, &CPU::ZP0, 3},{"ASL", &CPU::ASL, &CPU::ZP0, 5},{"SLO", &CPU::SLO, &CPU::ZP0, 5},{"PHP", &CPU::PHP, &CPU::IMP, 3},{"ORA", &CPU::ORA, &CPU::IMM, 2},{"ASL", &CPU::ASL, &CPU::IMP, 2},{"ANC", &CPU::ANC, &CPU::IMM, 2},{"NOP", &CPU::NOP, &CPU::ABS, 4},{"ORA", &CPU::ORA, &CPU::ABS, 4},{"ASL", &CPU::ASL, &CPU::ABS, 6},{"SLO", &CPU::SLO, &CPU::ABS, 6},
    {"BPL", &CPU::BPL, &CPU::REL, 2},{"ORA", &CPU::ORA, &CPU::IZY, 5},{"JAM", &CPU::JAM, &CPU::IMP, 2},{"SLO", &CPU::SLO, &CPU::IZY, 8},{"NOP", &CPU::NOP, &CPU::ZPX, 4},{"ORA", &CPU::ORA, &CPU::ZPX, 4},{"ASL", &CPU::ASL, &CPU::ZPX, 6},{"SLO", &CPU::SLO, &CPU::ZPX, 6},{"CLC", &CPU::CLC, &CPU::IMP, 2},{"ORA", &CPU::ORA, &CPU::ABY, 4},{"NOP", &CPU::NOP, &CPU::IMP, 2},{"SLO", &CPU::SLO, &CPU::ABY, 7},{"NOP", &CPU::NOP, &CPU::ABX, 4},{"ORA", &CPU::ORA, &CPU::ABX, 4},{"ASL", &CPU::ASL, &CPU::ABX, 7},{"SLO", &CPU::SLO, &CPU::ABX, 7},
    {"JSR", &CPU::JSR, &CPU::ABS, 6},{"AND", &CPU::AND, &CPU::IZX, 6},{"JAM", &CPU::JAM, &CPU::IMP, 2},{"RLA", &CPU::RLA, &CPU::IZX, 8},{"BIT", &CPU::BIT, &CPU::ZP0, 3},{"AND", &CPU::AND, &CPU::ZP0, 3},{"ROL", &CPU::ROL, &CPU::ZP0, 5},{"RLA", &CPU::RLA, &CPU::ZP0, 5},{"PLP", &CPU::PLP, &CPU::IMP, 4},{"AND", &CPU::AND, &CPU::IMM, 2},{"ROL", &CPU::ROL, &CPU::IMP, 2},{"ANC", &CPU::ANC, &CPU::IMM, 2},{"BIT", &CPU::BIT, &CPU::ABS, 4},{"AND", &CPU::AND, &CPU::ABS, 4},{"ROL", &CPU::ROL, &CPU::ABS, 6},{"RLA", &CPU::RLA, &CPU::ABS, 6},
    {"BMI", &CPU::BMI, &CPU::REL, 2},{"AND", &CPU::AND, &CPU::IZY, 5},{"JAM", &CPU::JAM, &CPU::IMP, 2},{"RLA", &CPU::RLA, &CPU::IZY, 8},{"NOP", &CPU::NOP, &CPU::ZPX, 4},{"AND", &CPU::AND, &CPU::ZPX, 4},{"ROL", &CPU::ROL, &CPU::ZPX, 6},{"RLA", &CPU::RLA, &CPU::ZPX, 6},{"SEC", &CPU::SEC, &CPU::IMP, 2},{"AND", &CPU::AND, &CPU::ABY, 4},{"NOP", &CPU::NOP, &CPU::IMP, 2},{"RLA", &CPU::RLA, &CPU::ABY, 7},{"NOP", &CPU::NOP, &CPU::ABX, 4},{"AND", &CPU::AND, &CPU::ABX, 4},{"ROL", &CPU::ROL, &CPU::ABX, 7},{"RLA", &CPU::RLA, &CPU::ABX, 7},
    {"RTI", &CPU::RTI, &CPU::IMP, 6},{"EOR", &CPU::EOR, &CPU::IZX, 6},{"JAM", &CPU::JAM, &CPU::IMP, 2},{"SRE", &CPU::SRE, &CPU::IZX, 8},{"NOP", &CPU::NOP, &CPU::ZP0, 3},{"EOR", &CPU::EOR, &CPU::ZP0, 3},{"LSR", &CPU::LSR, &CPU::ZP0, 5},{"SRE", &CPU::SRE, &CPU::ZP0, 5},{"PHA", &CPU::PHA, &CPU::IMP, 3},{"EOR", &CPU::EOR, &CPU::IMM, 2},{"LSR", &CPU::LSR, &CPU::IMP, 2},{"ALR", &CPU::ALR, &CPU::IMM, 2},{"JMP", &CPU::JMP, &CPU::ABS, 3},{"EOR", &CPU::EOR, &CPU::ABS, 4},{"LSR", &CPU::LSR, &CPU::ABS, 6},{"SRE", &CPU::SRE, &CPU::ABS, 6},
    {"BVC", &CPU::BVC, &CPU::REL, 2},{"EOR", &CPU::EOR, &CPU::IZY, 5},{"JAM", &CPU::JAM, &CPU::IMP, 2},{"SRE", &CPU::SRE, &CPU::IZY, 8},{"NOP", &CPU::NOP, &CPU::ZPX, 4},{"EOR", &CPU::EOR, &CPU::ZPX, 4},{"LSR", &CPU::LSR, &CPU::ZPX, 6},{"SRE", &CPU::SRE, &CPU::ZPX, 6},{"CLI", &CPU::CLI, &CPU::IMP, 2},{"EOR", &CPU::EOR, &CPU::ABY, 4},{"NOP", &CPU::NOP, &CPU::IMP, 2},{"SRE", &CPU::SRE, &CPU::ABY, 7},{"NOP", &CPU::NOP, &CPU::ABX, 4},{"EOR", &CPU::EOR, &CPU::ABX, 4},{"LSR", &CPU::LSR, &CPU::ABX, 7},{"SRE", &CPU::SRE, &CPU::ABX, 7},
    {"RTS", &CPU::RTS, &CPU::IMP, 6},{"ADC", &CPU::ADC, &CPU::IZX, 6},{"JAM", &CPU::JAM, &CPU::IMP, 2},{"RRA", &CPU::RRA, &CPU::IZX, 8},{"NOP", &CPU::NOP, &CPU::ZP0, 3},{"ADC", &CPU::ADC, &CPU::ZP0, 3},{"ROR", &CPU::ROR, &CPU::ZP0, 5},{"RRA", &CPU::RRA, &CPU::ZP0, 5},{"PLA", &CPU::PLA, &CPU::IMP, 4},{"ADC", &CPU::ADC, &CPU::IMM, 2},{"ROR", &CPU::ROR, &CPU::IMP, 2},{"ARR", &CPU::ARR, &CPU::IMM, 2},{"JMP", &CPU::JMP, &CPU::IND, 5},{"ADC", &CPU::ADC, &CPU::ABS, 4},{"ROR", &CPU::ROR, &CPU::ABS, 6},{"RRA", &CPU::RRA, &CPU::ABS, 6},
    {"BVS", &CPU::BVS, &CPU::REL, 2},{"ADC", &CPU::ADC, &CPU::IZY, 5},{"JAM", &CPU::JAM, &CPU::IMP, 2},{"RRA", &CPU::RRA, &CPU::IZY, 8},{"NOP", &CPU::NOP, &CPU::ZPX, 4},{"ADC", &CPU::ADC, &CPU::ZPX, 4},{"ROR", &CPU::ROR, &CPU::ZPX, 6},{"RRA", &CPU::RRA, &CPU::ZPX, 6},{"SEI", &CPU::SEI, &CPU::IMP, 2},{"ADC", &CPU::ADC, &CPU::ABY, 4},{"NOP", &CPU::NOP, &CPU::IMP, 2},{"RRA", &CPU::RRA, &CPU::ABY, 7},{"NOP", &CPU::NOP, &CPU::ABX, 4},{"ADC", &CPU::ADC, &CPU::ABX, 4},{"ROR", &CPU::ROR, &CPU::ABX, 7},{"RRA", &CPU::RRA, &CPU::ABX, 7},
    {"NOP", &CPU::NOP, &CPU::IMM, 2},{"STA", &CPU::STA, &CPU::IZX, 6},{"NOP", &CPU::NOP, &CPU::IMM, 2},{"SAX", &CPU::SAX, &CPU::IZX, 6},{"STY", &CPU::STY, &CPU::ZP0, 3},{"STA", &CPU::STA, &CPU::ZP0, 3},{"STX", &CPU::STX, &CPU::ZP0, 3},{"SAX", &CPU::SAX, &CPU::ZP0, 3},{"DEY", &CPU::DEY, &CPU::IMP, 2},{"NOP", &CPU::NOP, &CPU::IMM, 2},{"TXA", &CPU::TXA, &CPU::IMP, 2},{"XAA", &CPU::XAA, &CPU::IMM, 2},{"STY", &CPU::STY, &CPU::ABS, 4},{"STA", &CPU::STA, &CPU::ABS, 4},{"STX", &CPU::STX, &CPU::ABS, 4},{"SAX", &CPU::SAX, &CPU::ABS, 4},
    {"BCC", &CPU::BCC, &CPU::REL, 2},{"STA", &CPU::STA, &CPU::IZY, 6},{"JAM", &CPU::JAM, &CPU::IMP, 2},{"AHX", &CPU::AHX, &CPU::IZY, 6},{"STY", &CPU::STY, &CPU::ZPX, 4},{"STA", &CPU::STA, &CPU::ZPX, 4},{"STX", &CPU::STX, &CPU::ZPY, 4},{"SAX", &CPU::SAX, &CPU::ZPY, 4},{"TYA", &CPU::TYA, &CPU::IMP, 2},{"STA", &CPU::STA, &CPU::ABY, 5},{"TXS", &CPU::TXS, &CPU::IMP, 2},{"TAS", &CPU::TAS, &CPU::ABY, 5},{"SHY", &CPU::SHY, &CPU::ABX, 5},{"STA", &CPU::STA, &CPU::ABX, 5},{"SHX", &CPU::SHX, &CPU::ABY, 5},{"AHX", &CPU::AHX, &CPU::ABY, 5},
    {"LDY", &CPU::LDY, &CPU::IMM, 2},{"LDA", &CPU::LDA, &CPU::IZX, 6},{"LDX", &CPU::LDX, &CPU::IMM, 2},{"LAX", &CPU::LAX, &CPU::IZX, 6},{"LDY", &CPU::LDY, &CPU::ZP0, 3},{"LDA", &CPU::LDA, &CPU::ZP0, 3},{"LDX", &CPU::LDX, &CPU::ZP0, 3},{"LAX", &CPU::LAX, &CPU::ZP0, 3},{"TAY", &CPU::TAY, &CPU::IMP, 2},{"LDA", &CPU::LDA, &CPU::IMM, 2},{"TAX", &CPU::TAX, &CPU::IMP, 2},{"LAX", &CPU::LAX, &CPU::IMM, 2},{"LDY", &CPU::LDY, &CPU::ABS, 4},{"LDA", &CPU::LDA, &CPU::ABS, 4},{"LDX", &CPU::LDX, &CPU::ABS, 4},{"LAX", &CPU::LAX, &CPU::ABS, 4},
    {"BCS", &CPU::BCS, &CPU::REL, 2},{"LDA", &CPU::LDA, &CPU::IZY, 5},{"JAM", &CPU::JAM, &CPU::IMP, 2},{"LAX", &CPU::LAX, &CPU::IZY, 5},{"LDY", &CPU::LDY, &CPU::ZPX, 4},{"LDA", &CPU::LDA, &CPU::ZPX, 4},{"LDX", &CPU::LDX, &CPU::ZPY, 4},{"LAX", &CPU::LAX, &CPU::ZPY, 4},{"CLV", &CPU::CLV, &CPU::IMP, 2},{"LDA", &CPU::LDA, &CPU::ABY, 4},{"TSX", &CPU::TSX, &CPU::IMP, 2},{"LAS", &CPU::LAS, &CPU::ABY, 4},{"LDY", &CPU::LDY, &CPU::ABX, 4},{"LDA", &CPU::LDA, &CPU::ABX, 4},{"LDX", &CPU::LDX, &CPU::ABY, 4},{"LAX", &CPU::LAX, &CPU::ABY, 4},
    {"CPY", &CPU::CPY, &CPU::IMM, 2},{"CMP", &CPU::CMP, &CPU::IZX, 6},{"NOP", &CPU::NOP, &CPU::IMM, 2},{"DCP", &CPU::DCP, &CPU::IZX, 8},{"CPY", &CPU::CPY, &CPU::ZP0, 3},{"CMP", &CPU::CMP, &CPU::ZP0, 3},{"DEC", &CPU::DEC, &CPU::ZP0, 5},{"DCP", &CPU::DCP, &CPU::ZP0, 5},{"INY", &CPU::INY, &CPU::IMP, 2},{"CMP", &CPU::CMP, &CPU::IMM, 2},{"DEX", &CPU::DEX, &CPU::IMP, 2},{"AXS", &CPU::AXS, &CPU::IMM, 2},{"CPY", &CPU::CPY, &CPU::ABS, 4},{"CMP", &CPU::CMP, &CPU::ABS, 4},{"DEC", &CPU::DEC, &CPU::ABS, 6},{"DCP", &CPU::DCP, &CPU::ABS, 6},
    {"BNE", &CPU::BNE, &CPU::REL, 2},{"CMP", &CPU::CMP, &CPU::IZY, 5},{"JAM", &CPU::JAM, &CPU::IMP, 2},{"DCP", &CPU::DCP, &CPU::IZY, 8},{"NOP", &CPU::NOP, &CPU::ZPX, 4},{"CMP", &CPU::CMP, &CPU::ZPX, 4},{"DEC", &CPU::DEC, &CPU::ZPX, 6},{"DCP", &CPU::DCP, &CPU::ZPX, 6},{"CLD", &CPU::CLD, &CPU::IMP, 2},{"CMP", &CPU::CMP, &CPU::ABY, 4},{"NOP", &CPU::NOP, &CPU::IMP, 2},{"DCP", &CPU::DCP, &CPU::ABY, 7},{"NOP", &CPU::NOP, &CPU::ABX, 4},{"CMP", &CPU::CMP, &CPU::ABX, 4},{"DEC", &CPU::DEC, &CPU::ABX, 7},{"DCP", &CPU::DCP, &CPU::ABX, 7},
    {"CPX", &CPU::CPX, &CPU::IMM, 2},{"SBC", &CPU::SBC, &CPU::IZX, 6},{"NOP", &CPU::NOP, &CPU::IMM, 2},{"ISC", &CPU::ISC, &CPU::IZX, 8},{"CPX", &CPU::CPX, &CPU::ZP0, 3},{"SBC", &CPU::SBC, &CPU::ZP0, 3},{"INC", &CPU::INC, &CPU::ZP0, 5},{"ISC", &CPU::ISC, &CPU::ZP0, 5},{"INX", &CPU::INX, &CPU::IMP, 2},{"SBC", &CPU::SBC, &CPU::IMM, 2},{"NOP", &CPU::NOP, &CPU::IMP, 2},{"SBC", &CPU::SBC, &CPU::IMM, 2},{"CPX", &CPU::CPX, &CPU::ABS, 4},{"SBC", &CPU::SBC, &CPU::ABS, 4},{"INC", &CPU::INC, &CPU::ABS, 6},{"ISC", &CPU::ISC, &CPU::ABS, 6},
    {"BEQ", &CPU::BEQ, &CPU::REL, 2},{"SBC", &CPU::SBC, &CPU::IZY, 5},{"JAM", &CPU::JAM, &CPU::IMP, 2},{"ISC", &CPU::ISC, &CPU::IZY, 8},{"NOP", &CPU::NOP, &CPU::ZPX, 4},{"SBC", &CPU::SBC, &CPU::ZPX, 4},{"INC", &CPU::INC, &CPU::ZPX, 6},{"ISC", &CPU::ISC, &CPU::ZPX, 6},{"SED", &CPU::SED, &CPU::IMP, 2},{"SBC", &CPU::SBC, &CPU::ABY, 4},{"NOP", &CPU::NOP, &CPU::IMP, 2},{"ISC", &CPU::ISC, &CPU::ABY, 7},{"NOP", &CPU::NOP, &CPU::ABX, 4},{"SBC", &CPU::SBC, &CPU::ABX, 4},{"INC", &CPU::INC, &CPU::ABX, 7},{"ISC", &CPU::ISC, &CPU::ABX, 7},
};

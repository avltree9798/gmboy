# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

This is a Game Boy emulator (gmboy) written in C using SDL2 for graphics and input. The emulator implements the core components of the Game Boy hardware including CPU, memory bus, cartridge loading, and PPU (Picture Processing Unit).

## Build & Development Commands

### Building the Project
```bash
# Build the emulator
make

# Clean build artifacts
make clean

# Run the emulator with a ROM file
./build/gmboy <rom_file>
```

### Dependencies
The project requires SDL2 libraries installed via Homebrew (macOS):
- SDL2
- SDL2_image
- SDL2_ttf

The Makefile is configured for Homebrew installation paths on macOS.

## Architecture Overview

### Core Components

**CPU (`src/lib/cpu.c`, `src/include/cpu.h`)**
- Implements Game Boy's Sharp LR35902 CPU
- CPU context includes registers (A, F, B, C, D, E, H, L, PC, SP)
- Instruction fetch-decode-execute cycle with debugging output
- Supports halting and stepping modes

**Memory Bus (`src/lib/bus.c`, `src/include/bus.h`)**
- Handles 8-bit and 16-bit memory read/write operations
- Acts as the central communication hub between CPU, cartridge, and other components

**Cartridge (`src/lib/cart.c`, `src/include/cart.h`)**
- ROM loading and cartridge header parsing
- Handles cartridge memory mapping and banking
- Supports reading title, type, sizes, and checksums from ROM header

**Instructions (`src/lib/instructions.c`, `src/include/instructions.h`)**
- Comprehensive instruction set implementation
- Defines addressing modes, register types, and instruction types
- Includes CB-prefixed extended instructions
- Maps opcodes to instruction processors

**Emulator Core (`src/lib/emu.c`, `src/include/emu.h`)**
- Main emulation loop with pause/resume functionality
- SDL initialization and ROM loading
- Tick-based timing system
- Entry point that coordinates all subsystems

**PPU (Picture Processing Unit) (`src/include/ppu.h`)**
- Currently empty - graphics rendering component to be implemented

### Project Structure
```
src/
├── gmboy/          # Main entry point
│   └── main.c
├── include/        # Header files for all components
│   ├── bus.h
│   ├── cart.h
│   ├── common.h    # Common types and macros
│   ├── cpu.h
│   ├── emu.h
│   ├── instructions.h
│   ├── ppu.h
│   └── timer.h
└── lib/           # Implementation files
    ├── bus.c
    ├── cart.c
    ├── cpu.c
    ├── cpu_fetch.c
    ├── cpu_proc.c
    ├── cpu_util.c
    ├── emu.c
    └── instructions.c
```

### Key Development Notes

**Common Types (`src/include/common.h`)**
- Uses consistent integer types: u8, u16, u32, u64
- Provides bit manipulation macros: BIT(), BIT_SET(), BETWEEN()
- NO_IMPL macro for unimplemented functionality

**CPU Architecture**
- Game Boy CPU is based on Intel 8080/Zilog Z80
- 16-bit registers can be accessed as 8-bit pairs
- Flag register (F) contains Zero, Subtract, Half-carry, and Carry flags
- Program counter (PC) and stack pointer (SP) are 16-bit

**Memory Layout**
- Game Boy uses 16-bit addressing (0x0000-0xFFFF)
- ROM cartridge typically mapped to 0x0000-0x7FFF
- RAM and I/O regions in upper address space

**Debugging Features**
- CPU execution trace shows PC, instruction, opcode bytes, and register states
- Stepping mode available for single-instruction debugging
- Error handling for unknown opcodes and unimplemented features

### Testing
Currently no automated test suite - testing done by running ROM files and observing emulator behavior and debug output.

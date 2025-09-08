# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

This is a Game Boy emulator (gmboy) written in C using SDL2 for graphics and input. The emulator implements the core components of the Game Boy hardware including CPU, memory bus, cartridge loading, PPU (Picture Processing Unit), timer, interrupts, and DMA.

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

### Debug Output
The emulator provides debugging output including:
```bash
# Run with CPU trace enabled
DEBUG=1 ./build/gmboy <rom_file>

# Enable more verbose debugging
DEBUG=2 ./build/gmboy <rom_file>
```

## Architecture Overview

### Core Components

**CPU (`src/lib/cpu.c`, `src/include/cpu.h`)**
- Implements Game Boy's Sharp LR35902 CPU (similar to Z80/8080)
- CPU context includes registers (A, F, B, C, D, E, H, L, PC, SP)
- Instruction fetch-decode-execute cycle with debugging output
- Supports halting and stepping modes
- CPU implementation is split into multiple files:
  - `cpu_fetch.c`: Instruction fetch logic
  - `cpu_proc.c`: Instruction processing
  - `cpu_util.c`: CPU utility functions

**Memory Bus (`src/lib/bus.c`, `src/include/bus.h`)**
- Handles 8-bit and 16-bit memory read/write operations
- Acts as the central communication hub between CPU, cartridge, and other components
- Maps memory-mapped I/O to appropriate subsystems

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
- Runs CPU in a separate thread

**PPU (Picture Processing Unit)**
- Main PPU file (`src/lib/ppu.c`, `src/include/ppu.h`):
  - Handles Game Boy graphics rendering
  - Manages VRAM and OAM memory regions
  - Implementation of tile-based rendering system
- PPU State Machine (`src/lib/ppu_sm.c`, `src/include/ppu_sm.h`):
  - Implements the different PPU modes (OAM, Transfer, HBlank, VBlank)
  - Handles state transitions between modes
- PPU Pipeline (`src/lib/ppu_pipeline.c`):
  - Implements the pixel rendering pipeline
- LCD Controller (`src/lib/lcd.c`, `src/include/lcd.h`):
  - Handles the LCD control registers
  - Manages LCD modes, window and background settings

**Timer (`src/lib/timer.c`, `src/include/timer.h`)**
- Implements Game Boy timer system
- Handles DIV, TIMA, TMA, and TAC registers
- Generates timer interrupts

**Interrupts (`src/lib/interrupts.c`, `src/include/interrupts.h`)**
- Handles the Game Boy's interrupt system
- Supports VBlank, LCD STAT, Timer, Serial, and Joypad interrupts
- CPU interrupt handling and request management

**Joypad (`src/lib/joypad.c`, `src/include/joypad.h`)**
- Handles Game Boy joypad input
- Manages the joypad register and input state
- Supports directional pad and button inputs

**DMA (`src/lib/dma.c`, `src/include/dma.h`)**
- Implements Direct Memory Access functionality
- Manages OAM DMA transfers

**RAM (`src/lib/ram.c`, `src/include/ram.h`)**
- Work RAM (WRAM) implementation
- High RAM (HRAM) implementation

**Stack (`src/lib/stack.c`, `src/include/stack.h`)**
- Stack operations implementation
- Handles push/pop for 8-bit and 16-bit values

**UI (`src/lib/ui.c`, `src/include/ui.h`)**
- SDL-based user interface
- Window management for main and debug screens
- Event handling and rendering

**Debug (`src/lib/dbg.c`, `src/include/dbg.h`)**
- Debugging utilities and output formatting
- Trace logging of CPU operations

**I/O (`src/lib/io.c`, `src/include/io.h`)**
- Handles I/O port operations
- Manages I/O register memory mapping

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
│   ├── dbg.h
│   ├── dma.h
│   ├── emu.h
│   ├── instructions.h
│   ├── interrupts.h
│   ├── io.h
│   ├── joypad.h    # Joypad controller
│   ├── lcd.h       # LCD controller
│   ├── ppu.h       # Picture Processing Unit
│   ├── ppu_sm.h    # PPU state machine
│   ├── ram.h
│   ├── stack.h
│   ├── timer.h
│   └── ui.h
└── lib/           # Implementation files
    ├── bus.c
    ├── cart.c
    ├── cpu.c
    ├── cpu_fetch.c
    ├── cpu_proc.c
    ├── cpu_util.c
    ├── dbg.c
    ├── dma.c
    ├── emu.c
    ├── instructions.c
    ├── interrupts.c
    ├── io.c
    ├── joypad.c    # Joypad implementation
    ├── lcd.c       # LCD controller implementation
    ├── ppu.c       # Main PPU implementation
    ├── ppu_pipeline.c # PPU pixel pipeline
    ├── ppu_sm.c    # PPU state machine implementation
    ├── ram.c
    ├── stack.c
    ├── timer.c
    └── ui.c
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
- VRAM: 0x8000-0x9FFF
- External RAM (on cartridge): 0xA000-0xBFFF
- WRAM: 0xC000-0xDFFF
- Echo RAM (mirror of WRAM): 0xE000-0xFDFF
- OAM: 0xFE00-0xFE9F
- I/O Registers: 0xFF00-0xFF7F
- HRAM: 0xFF80-0xFFFE
- Interrupt Enable Register: 0xFFFF

**PPU Implementation Notes**
- Operates in modes: H-Blank, V-Blank, OAM Search, Pixel Transfer
- Tile-based rendering (8x8 pixel tiles)
- Manages OAM for sprite rendering
- Debug window shows tile data
- PPU implementation is split across multiple files:
  - Main PPU logic in `ppu.c`
  - State machine in `ppu_sm.c` 
  - Pixel pipeline in `ppu_pipeline.c`
  - LCD controller in `lcd.c`

**LCD Controller**
- Manages LCD control register (LCDC)
- Handles LCD status register (STAT)
- Controls window and background positioning
- Manages palette data for background and sprites

**Joypad Implementation**
- Handles button inputs (A, B, Start, Select)
- Handles directional inputs (Up, Down, Left, Right)
- Manages joypad register and interrupt generation

**Debugging Features**
- CPU execution trace shows PC, instruction, opcode bytes, and register states
- Stepping mode available for single-instruction debugging
- Tile viewer for graphics debugging
- Error handling for unknown opcodes and unimplemented features

**Multi-threading**
- CPU runs in a separate thread from UI
- UI thread handles rendering and user input
- Thread synchronization through emulator context

### Testing
Currently no automated test suite - testing done by running ROM files and observing emulator behavior and debug output. A good set of test ROMs can be found in the Blargg test suite or similar Game Boy test ROM collections.

### ROM Usage
Place ROM files in the `roms/` directory and run the emulator pointing to the ROM file:
```bash
./build/gmboy roms/your_game.gb
```

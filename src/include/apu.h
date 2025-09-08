#pragma once
#include <common.h>

// DMG APU clock: 4,194,304 Hz (same ticks you already step for PPU/TIMER)
#define APU_CLOCK_HZ 4194304

// Call once at startup. sample_rate example: 48000 or 44100
void apu_init(int sample_rate);

// Power-on / reset (also called from emu_run)
void apu_reset(void);

// Tick the APU by one T-cycle (you already call timer_tick()/ppu_tick() per dot)
void apu_tick(void);

// Map I/O
u8  apu_io_read(u16 addr);
void apu_io_write(u16 addr, u8 v);

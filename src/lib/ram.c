#include <ram.h>

typedef struct {
    u8 wram[0x2000]; // 8KB Work RAM
    u8 hram[0x80]; // 2KB High RAM
} ram_context;

static ram_context ctx;

u8 wram_read(u16 address) {
    address -= 0xC000;
    if (address >= 0x2000) {
        printf("Invalid WRAM read: %04X\n", address+0xC000);
        exit(-1);
    }
    return ctx.wram[address];
}

void wram_write(u16 address, u8 value) {
    address -= 0xC000;
    if (address >= 0x2000) {
        printf("Invalid WRAM write: %04X\n", address+0xC000);
        exit(-1);
    }
    ctx.wram[address] = value;
}

u8 hram_read(u16 address) {
    address -= 0xFF80;
    if (address >= 0x80) {
        printf("Invalid HRAM read: %04X\n", address+0xFF80);
        exit(-1);
    }
    return ctx.hram[address];
}

void hram_write(u16 address, u8 value) {
    address -= 0xFF80;
    if (address >= 0x80) {
        printf("Invalid HRAM write: %04X\n", address+0xFF80);
        exit(-1);
    }
    ctx.hram[address] = value;
}

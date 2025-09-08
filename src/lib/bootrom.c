#include <bootrom.h>
#include <string.h>

static bootrom_ctx g = {0};

bootrom_ctx* bootrom_get() { return &g; }

bool bootrom_load(const char* path) {
    memset(&g, 0, sizeof(g));
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(g.data, 1, sizeof(g.data), f);
    fclose(f);

    // Accept 256 (DMG) or 2048 (CGB) bytes; prefer DMG for now.
    if (n == 256) { g.size = 256; g.cgb = false; }
    else if (n == 2048) { g.size = 2048; g.cgb = true; }
    else return false;

    g.loaded = true;
    g.enabled = true; // mapped after reset
    return true;
}

void bootrom_reset() {
    if (g.loaded) g.enabled = true;
}

void bootrom_disable() {
    if (g.loaded) g.enabled = false; // one-way until next reset
}

bool bootrom_present()  { return g.loaded; }
bool bootrom_enabled()  { return g.loaded && g.enabled; }

bool bootrom_active_window(u16 addr) {
    if (!bootrom_enabled()) return false;

    // DMG maps 0x0000-0x00FF only. CGB also maps 0x0200-0x08FF.
    if (g.size == 256) {
        return addr < 0x0100;
    } else { // 2048
        if (addr < 0x0100) return true;
        if (addr >= 0x0200 && addr < 0x0900) return true;
        return false;
    }
}

u8 bootrom_read(u16 addr) {
    if (g.size == 256) {
        return (addr < 0x0100) ? g.data[addr] : 0xFF;
    } else {
        if (addr < 0x0100) return g.data[addr];
        if (addr >= 0x0200 && addr < 0x0900) return g.data[addr - 0x0200 + 0x0100];
        return 0xFF;
    }
}

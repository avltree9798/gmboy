#pragma once
#include <common.h>

typedef struct {
    bool loaded;     // a boot ROM file was loaded
    bool enabled;    // currently mapped at power-on until 0xFF50 disables it
    bool cgb;        // (optional) if you add CGB later
    u8   data[2048]; // enough for CGB; DMG only uses first 256 bytes
    u16  size;       // 256 (DMG) or 2048 (CGB)
} bootrom_ctx;

bootrom_ctx* bootrom_get();

bool bootrom_load(const char* path); // returns true if loaded
void bootrom_reset();                // enabled=true if loaded
void bootrom_disable();              // write to FF50
bool bootrom_active_window(u16 addr);// is addr currently covered by bootrom?
u8   bootrom_read(u16 addr);

// Helper to decide CPU reset PC / register seeding
bool bootrom_present();
bool bootrom_enabled();

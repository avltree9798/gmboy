#include <lcd.h>
#include <common.h>
#include <ppu.h>
#include <dma.h>


static lcd_context ctx = {0};

static unsigned long colours_default[4] = {0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000};

void lcd_init() {
    ctx.lcdc = 0x91;
    ctx.sc_x = 0;
    ctx.sc_y = 0;
    ctx.ly = 0;
    ctx.ly_compare = 0;
    ctx.bg_palette = 0xFC;
    ctx.obj_palette[0] = 0xFF;
    ctx.obj_palette[1] = 0xFF;
    ctx.win_x = 0;
    ctx.win_y = 0;
    int i;
    for (i=0;i<4;++i) {
        ctx.bg_colours[i] = colours_default[i];
        ctx.sp1_colours[i] = colours_default[i];
        ctx.sp2_colours[i] = colours_default[i];
    }
}

lcd_context* lcd_get_context() {
    return &ctx;
}

u8 lcd_read(u16 address) {
    u8 offset = (address - 0xFF40);
    u8* p = (u8*)&ctx;
    return p[offset];
}

void update_palette(u8 palette_data, u8 pal) {
    u32* p_colours = ctx.bg_colours;
    switch (pal) {
        case 1:
        p_colours = ctx.sp1_colours;
        break;
        case 2:
        p_colours = ctx.sp2_colours;
        break;
    }

    p_colours[0] = colours_default[palette_data & 0x11];
    p_colours[1] = colours_default[(palette_data >> 2) & 0b11];
    p_colours[2] = colours_default[(palette_data >> 4) & 0b11];
    p_colours[3] = colours_default[(palette_data >> 6) & 0b11];
}

void lcd_write(u16 address, u8 value) {
    u8 offset = (address - 0xFF40);
    u8* p = (u8*)&ctx;
    p[offset] = value;

    if (offset == 6) {
        //0xFF46 = DMA
        dma_start(value);
    }

    if (address == 0xFF47) {
        update_palette(value, 0);
    } else if (address == 0xFF48) {
        update_palette(value & 0b11111100, 1);
    } else if (address == 0xFF49) {
        update_palette(value & 0b11111100, 2);
    }
}


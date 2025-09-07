#include <joypad.h>
#include <string.h>

typedef struct {
    bool button_sel;
    bool dir_sel;
    joypad_state controller;
} joypad_context;

static joypad_context ctx = {0};

void joypad_init() {

}

bool joypad_button_sel() {
    return ctx.button_sel;
}

bool joypad_dir_sel() {
    return ctx.dir_sel;
}

void joypad_set_sel(u8 value) {
    ctx.button_sel = value & 0x20;
    ctx.dir_sel = value & 0x10;
}

joypad_state* joypad_get_state() {
    return &ctx.controller;
}

u8 joypad_get_output() {
    u8 output = 0xCF;
    if (!joypad_button_sel()) {
        if(joypad_get_state()->start) {
            output &= ~(1<<3);
        } else if(joypad_get_state()->select) {
            output &= ~(1<<2);
        } else if(joypad_get_state()->a) {
            output &= ~(1<<0);
        } else if(joypad_get_state()->b) {
            output &= ~(1<<1);
        }
    }
    if (!joypad_dir_sel()) {
        if(joypad_get_state()->left) {
            output &= ~(1<<1);
        } else if(joypad_get_state()->right) {
            output &= ~(1<<0);
        } else if(joypad_get_state()->up) {
            output &= ~(1<<2);
        } else if(joypad_get_state()->down) {
            output &= ~(1<<3);
        }
    }
    return output;
}
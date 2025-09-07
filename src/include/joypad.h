#pragma once

#include <common.h>

typedef struct {
    bool start;
    bool select;
    bool a;
    bool b;
    bool up;
    bool down;
    bool left;
    bool right;
} joypad_state;

void joypad_init();
bool joypad_button_sel();
bool joypad_dir_sel();
void joypad_set_sel(u8 value);

joypad_state* joypad_get_state();
u8 joypad_get_output();

#include <io.h>
#include <timer.h>
#include <cpu.h>
#include <dma.h>
#include <lcd.h>
#include <joypad.h>
#include <bootrom.h>
#include <apu.h>

static char serial_data[2];

u8 ly = 0;

u8 io_read(u16 address) {
    if (address >= 0xFF10 && address <= 0xFF3F) {
        return apu_io_read(address);
    }
    if (address == 0xFF24 || address == 0xFF25 || address == 0xFF26) {
        return apu_io_read(address);
    }
    if (address == 0xFF00) {
        return joypad_get_output();
    }
    if (address == 0xFF01) {
        return serial_data[0];
    }
    if (address == 0xFF02) {
        return serial_data[1];
    }
    if (BETWEEN(address, 0xFF04, 0xFF07)) {
        return timer_read(address);
    }
    if (address == 0xFF0F) {
        return cpu_get_int_flags();
    }
    if (BETWEEN(address, 0xFF40, 0xFF4B)) {
        return lcd_read(address);
    }
    if (address == 0xFF50) {
        // Common practice: 0x00 when mapped, 0x01 when unmapped.
        return bootrom_enabled() ? 0x00 : 0x01;
    }
    // printf("UNSUPPORTED io_read(%04X)\n", address);
    return 0x00;
}

void io_write(u16 address, u8 value) {
    if (address >= 0xFF10 && address <= 0xFF3F) {
        apu_io_write(address, value);
        return;
    }
    if (address == 0xFF24 || address == 0xFF25 || address == 0xFF26) {
        apu_io_write(address, value);
        return;
    }
    if (address == 0xFF00) {
        joypad_set_sel(value);
        return;
    }
    if (address == 0xFF01) {
        serial_data[0] = value;
    }
    if (address == 0xFF02) {
        serial_data[1] = value;
    }
    if (BETWEEN(address, 0xFF04, 0xFF07)) {
        timer_write(address, value);
        return;
    }
    if (address == 0xFF0F) {
        cpu_set_int_flags(value);
        return;
    }
    if (BETWEEN(address, 0xFF40, 0xFF4B)) {
        lcd_write(address, value);
        return;
    }
    if (address == 0xFF50) {
        // Any non-zero write permanently unmaps until reset.
        if (value & 0x01) bootrom_disable();
        return;
    }
    // printf("UNSUPPORTED io_write(%04X)\n", address);
}

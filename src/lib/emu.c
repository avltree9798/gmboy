#include <stdio.h>
#include <emu.h>
#include <cart.h>
#include <ui.h>
#include <cpu.h>
#include <pthread.h>
#include <unistd.h>
#include <timer.h>
#include <ui.h>
#include <dma.h>
#include <ppu.h>
#include <bootrom.h>
#include <apu.h>

static emu_context ctx;

emu_context* emu_get_context() {
    return &ctx;
}

void* cpu_run(void* p) {
    timer_init();
    cpu_init();
    ppu_init();
    ctx.running = true;
    ctx.paused = false;
    ctx.ticks = 0;
    while (ctx.running) {
        if (ctx.paused) {
            delay(10);
            continue;
        }
        if (!cpu_step()) {
            printf("CPU step failed\n");
            return (void *)-3;
        }
    }
    return NULL;
}

int emu_run(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <rom.gb> [bootrom.bin]\n", argv[0]);
        return -1;
    }
    // Optional 2nd arg: path to boot ROM
    if (argc >= 3 && bootrom_load(argv[2])) {
        printf("Loaded boot ROM: %s\n", argv[2]);
    }
    bootrom_reset();
    if (!cart_load(argv[1])) {
        printf("Failed to load ROM file: %s\n", argv[1]);
        return -2;
    }
    printf("Successfully loaded ROM file: %s\n", argv[1]);
    ui_init();
    pthread_t t1;
    if(pthread_create(&t1, NULL, cpu_run, NULL) != 0) {
        fprintf(stderr, "Failed to create CPU thread\n");
        return -3;
    }
    u32 prev_frame = 0;
    while(!ctx.die) {
        usleep(1000);
        ui_handle_events();
        if (prev_frame != ppu_get_context()->current_frame) {
            ui_update();
        }
        prev_frame = ppu_get_context()->current_frame;
    }
    return 0;
}

void emu_cycles(int cpu_cycles) {
    for (int i =0; i<cpu_cycles; ++i) {
        for (int n=0; n<4; ++n) {
            ctx.ticks ++;
            timer_tick();
            ppu_tick();
            apu_tick();
        }
        dma_tick();
    }
}

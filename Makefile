CC = gcc
CFLAGS = -I./src/include -I./include \
-I/opt/homebrew/Cellar/sdl2_image/2.8.8/include \
-I/opt/homebrew/Cellar/sdl2_image/2.8.8/include/SDL2 \
-I/opt/homebrew/Cellar/sdl2_ttf/2.24.0/include \
-I/opt/homebrew/Cellar/sdl2_ttf/2.24.0/include/SDL2 \
-I/opt/homebrew/Cellar/sdl2/2.32.8/include \
-I/opt/homebrew/Cellar/sdl2/2.32.8/include/SDL2
LDFLAGS = -L/opt/homebrew/Cellar/sdl2_image/2.8.8/lib -L/opt/homebrew/Cellar/sdl2_ttf/2.24.0/lib -L/opt/homebrew/Cellar/sdl2/2.32.8/lib -lSDL2_image -lSDL2_ttf -lSDL2

SRC = src/lib/apu.c src/lib/bootrom.c src/lib/joypad.c src/lib/ppu_pipeline.c src/lib/ppu_sm.c src/lib/lcd.c  src/lib/dma.c src/lib/ppu.c src/lib/dbg.c  src/lib/io.c src/lib/ui.c src/lib/interrupts.c src/lib/timer.c src/lib/stack.c src/lib/ram.c src/lib/cpu_fetch.c src/lib/cpu_proc.c src/lib/instructions.c src/lib/emu.c src/lib/cpu_util.c src/lib/bus.c src/lib/cpu.c src/lib/emu.c src/lib/cart.c src/gmboy/main.c
OBJ = $(SRC:%.c=build/%.o)
TARGET = build/gmboy

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build
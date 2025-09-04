#include <ui.h>
#include <common.h>
#include <emu.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>


SDL_Window* sdl_window;
SDL_Renderer* sdl_renderer;
SDL_Texture* sdl_texture;
SDL_Surface* sdl_surface;

void ui_init() {
    printf("SDL INIT\n");
    SDL_Init(SDL_INIT_VIDEO);
    printf("TTF INIT\n");
    TTF_Init();
    SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, 0, &sdl_window, &sdl_renderer);
}

void ui_handle_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e) > 0) {
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {
            emu_get_context()->die = true;
            SDL_DestroyWindow(sdl_window);
            SDL_Quit();
        }
    }
}

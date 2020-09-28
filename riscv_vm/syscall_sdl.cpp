#if RISCV_VM_USE_SDL

#include <cstdint>
#include <cstdio>
#include <ctime>

#include "../riscv_core/riscv.h"
#include "state.h"

#include <SDL.h>


SDL_Surface *g_video;


static bool check_sdl(uint32_t width, uint32_t height) {
  // check if video has been setup
  if (!g_video) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      fprintf(stderr, "Failed to call SDL_Init()\n");
      exit(1);
    }
    g_video = SDL_SetVideoMode(width, height, 32, 0);
    if (!g_video) {
      fprintf(stderr, "Failed to call SDL_SetVideoMode()\n");
      exit(1);
    }
    SDL_WM_SetCaption("riscv-vm", nullptr);
  }
  // run a simple event handler
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      exit(0);
    }
  }
  // success
  return true;
}

void syscall_draw_frame(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // draw(screen, width, height);
  const uint32_t screen = rv_get_reg(rv, rv_reg_a0);
  const uint32_t width  = rv_get_reg(rv, rv_reg_a1);
  const uint32_t height = rv_get_reg(rv, rv_reg_a2);
  // check if we need to setup SDL
  if (!check_sdl(width, height)) {
    return;
  }
  // read directly into video memory
  if (g_video) {
    s->mem.read((uint8_t*)g_video->pixels, screen, width * height * 4);
    SDL_Flip(g_video);
  }
}

#endif  // RISCV_VM_USE_SDL

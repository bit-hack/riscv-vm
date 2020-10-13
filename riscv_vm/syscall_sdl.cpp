#if RISCV_VM_USE_SDL

#include <cstdint>
#include <cstdio>
#include <ctime>

#include <SDL.h>

#include "../riscv_core/riscv.h"
#include "state.h"

extern bool g_fullscreen;

static SDL_Surface *g_video;


static bool check_sdl(struct riscv_t *rv, uint32_t width, uint32_t height) {

  state_t *s = (struct state_t *)rv_userdata(rv);

  // check if video has been setup
  if (!g_video) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      fprintf(stderr, "Failed to call SDL_Init()\n");
      exit(1);
    }

    int flags = 0;
    if (g_fullscreen) {
      flags |= SDL_FULLSCREEN;
    }

    g_video = SDL_SetVideoMode(width, height, 32, flags);
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
      rv_halt(rv);
      break;
    case SDL_KEYDOWN:
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        rv_halt(rv);
        break;
      }
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
  if (!check_sdl(rv, width, height)) {
    return;
  }
  // read directly into video memory
  if (g_video) {
    s->mem.read((uint8_t*)g_video->pixels, screen, width * height * 4);
    SDL_Flip(g_video);
  }
#if 1
  SDL_Delay(1);
#endif
}

void syscall_draw_frame_pal(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // draw(screen, width, height);
  const uint32_t buf = rv_get_reg(rv, rv_reg_a0);
  const uint32_t pal = rv_get_reg(rv, rv_reg_a1);
  const uint32_t width = rv_get_reg(rv, rv_reg_a2);
  const uint32_t height = rv_get_reg(rv, rv_reg_a3);
  // check if we need to setup SDL
  if (!check_sdl(rv, width, height)) {
    return;
  }
  // read directly into video memory
  if (g_video) {

    uint8_t *i = (uint8_t*)alloca(width * height);
    uint8_t *j = (uint8_t*)alloca(256 * 3);

    s->mem.read(i, buf, width * height);
    s->mem.read(j, pal, 256 * 3);

    uint32_t *d = (uint32_t*)g_video->pixels;
    const uint8_t *p = i;
    for (int y = 0; y < g_video->h; ++y) {
      for (int x = 0; x < g_video->w; ++x) {
        const uint8_t c = p[x];
        const uint8_t *lut = j + (c * 3);
        d[x] = (lut[0] << 16) | (lut[1] << 8) | lut[2];
      }
      p += g_video->w;
      d += g_video->w;
    }

    SDL_Flip(g_video);
  }
#if 1
  SDL_Delay(1);
#endif
}

#endif  // RISCV_VM_USE_SDL

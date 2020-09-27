#include <cstdint>
#include <cstdio>
#include <ctime>

#include "../riscv_core/riscv.h"
#include "state.h"

#include <SDL.h>


SDL_Surface *g_video;


void syscall_DG_DrawFrame(struct riscv_t *rv) {
  // access userdata
  state_t *s = (state_t*)rv_userdata(rv);
  // draw(name, flags, mode);
  const uint32_t screen = rv_get_reg(rv, rv_reg_a0);
  const uint32_t width  = rv_get_reg(rv, rv_reg_a1);
  const uint32_t height = rv_get_reg(rv, rv_reg_a2);
  // check if we need to setup SDL
  if (!g_video) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      exit(1);
    }
    g_video = SDL_SetVideoMode(width, height, 32, 0);
  }
  // read directly into video memory
  s->mem.read((uint8_t*)g_video->pixels, screen, width * height * 4);
  SDL_Flip(g_video);
  // throw in an event handler
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      exit(0);
    }
  }
}

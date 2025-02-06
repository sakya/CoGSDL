#ifndef COGSDL_H__
#define COGSDL_H__

#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_image.h"

struct CoGSDL {
  int quit;
  int32_t DISPLAY_WIDTH;
  int32_t DISPLAY_HEIGHT;
  SDL_Window *window;
  SDL_Renderer *renderer;
} typedef CoGSDL;

extern CoGSDL _Globals;

int init();
#endif
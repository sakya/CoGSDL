#ifndef COGSDL_H__
#define COGSDL_H__

#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_image.h"

#include "KeyMap.h"

struct CoGSDL {
  int quit;
  int32_t DISPLAY_WIDTH;
  int32_t DISPLAY_HEIGHT;
  SDL_Window *window;
  SDL_Renderer *renderer;

  TTF_Font *defaultFont;
  int defaultFontSize;
} typedef CoGSDL;

extern CoGSDL _Globals;

// init.c
int init();

// draw.c
int draw();

// events.c
int manageEvents();

#endif
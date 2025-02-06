#ifndef COG_SDL_H__
#define COG_SDL_H__

#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "signal.h"

#include "KeyMap.h"

struct CoGSDL {
  int32_t DISPLAY_WIDTH;
  int32_t DISPLAY_HEIGHT;
  SDL_Window *window;
  SDL_Renderer *renderer;

  TTF_Font *defaultFont;
  int defaultFontSize;

  int quit;
} typedef CoGSDL;

extern CoGSDL _G;

int init();
int init_display_Resolution();
int draw_loop();
void pollevent();
int handle_key();
void dispose();

#endif
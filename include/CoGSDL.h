#ifndef COGSDL_H__
#define COGSDL_H__

#include <locale.h>
#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_image.h"

#include "scene.h"
#include "KeyMap.h"

struct CoGSDL {
  int quit;
  int32_t DISPLAY_WIDTH;
  int32_t DISPLAY_HEIGHT;
  int32_t WINDOW_WIDTH;
  int32_t WINDOW_HEIGHT;
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

// scene.c
int loadScene(char* path, Scene* scene);

// utils.c
int indexOf(const char *string, char separator);
int startsWith(const char *pre, const char *str);
void subString(const char *source, char *target, int pos, int length);

#endif
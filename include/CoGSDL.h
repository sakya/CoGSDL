#ifndef COGSDL_H__
#define COGSDL_H__

#include "locale.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_image.h"
#include <sys/stat.h>

#include "game.h"
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
// Loads a game from a folder (read the file scenes/startup.txt)
int loadGame(char* path, Game* scene);

// utils.c
int indexOf(const char *string, char separator, int startFrom);
int startsWith(const char *prefix, const char *string);
void subString(const char *source, char *target, int pos, int length);
void trim(char *string);

// Return 1 if the given file exists
int fileExists(char *filename);

#endif
#include "CoGSDL.h"

int init()
{
  printf("##Start initialization\n");

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("\tSDL_Init Fail: %s\n", SDL_GetError());
    return -1;
  }

  if (TTF_Init() < 0) {
    printf("\tTTF_Init Fail: %s\n", TTF_GetError());
    return -1;
  }

  SDL_DisplayMode mode;
  if (SDL_GetDesktopDisplayMode(0, &mode)) {
    printf("SDL_GetDesktopDisplayMode Fail: %s\n", SDL_GetError());
    return -1;
  }
  _Globals.DISPLAY_WIDTH = mode.w;
  _Globals.DISPLAY_HEIGHT = mode.h;
  printf("\tDisplay Resolution: %dx%d\n", _Globals.DISPLAY_WIDTH, _Globals.DISPLAY_HEIGHT);

  const char *FONT_LOCATION = "./fonts/Roboto-Regular.ttf";
   _Globals.defaultFont = TTF_OpenFont(FONT_LOCATION, _Globals.defaultFontSize);

  _Globals.window = SDL_CreateWindow("Choice of games interpreter", SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED,
                               _Globals.DISPLAY_WIDTH, _Globals.DISPLAY_HEIGHT,
                               SDL_WINDOW_SHOWN);
  if (_Globals.window == NULL) {
    printf("\tSDL_CreateWindow Fail: %s\n", SDL_GetError());
    return -1;
  }

  _Globals.renderer = SDL_CreateRenderer(_Globals.window, -1, 0);
  if (_Globals.renderer == NULL) {
    printf("\tSDL_CreateRenderer Fail: %s\n", SDL_GetError());
    return -1;
  }

  return 0;
}
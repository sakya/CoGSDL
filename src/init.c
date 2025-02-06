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

  return 0;
}
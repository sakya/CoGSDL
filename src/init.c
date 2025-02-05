#include "CoGSDL.h.h"

int init() {
  printf("##Start initialization\n");

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("\tSDL_Init Fail: %s\n", SDL_GetError());
    return -1;
  }

  if (TTF_Init() < 0) {
    printf("\tTTF_Init Fail: %s\n", TTF_GetError());
    return -1;
  }

  if (init_display_Resolution() < 0) {
    return -1;
  }
  printf("\tDisplay Resolution: %dx%d\n", _G.DISPLAY_WIDTH, _G.DISPLAY_HEIGHT);

  const char *FONT_LOCATION = "./fonts/Oswald-Bold.ttf";
  _G.defaultFont = TTF_OpenFont(FONT_LOCATION, _G.TIME_SIZE);

  _G.window = SDL_CreateWindow("Choice of Games Interpreter", SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, _G.DISPLAY_WIDTH,
                               _G.DISPLAY_HEIGHT, SDL_WINDOW_SHOWN);

  if (_G.window == NULL) {
    printf("\tSDL_SetVideoMode Fail: %s\n", SDL_GetError());
    return -1;
  }

  _G.renderer = SDL_CreateRenderer(_G.window, -1, 0);

  if (_G.renderer == NULL) {
    printf("\tSDL_CreateRGBSurface Fail: %s\n", SDL_GetError());
    return -1;
  }

  printf("##Finish initialization\n\n");
  return 0;
}

int init_display_Resolution() {
  SDL_DisplayMode mode;
  if (SDL_GetDesktopDisplayMode(0, &mode)) {
    printf("SDL_GetDesktopDisplayMode Fail: %s\n", SDL_GetError());
    return -1;
  }
  _G.DISPLAY_WIDTH = mode.w;
  _G.DISPLAY_HEIGHT = mode.h;

  return 0;
}

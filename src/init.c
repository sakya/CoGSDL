#include "CoGSDL.h"

int init()
{
  logInit("CoGSDL.log");
  logInfo("Initializing\n");

  char *locale;
  locale = setlocale(LC_ALL, "");
  logInfo("\tLocale: %s\n", locale);

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    logInfo("\tSDL_Init Fail: %s\n", SDL_GetError());
    return -1;
  }

  if (TTF_Init() < 0) {
    logInfo("\tTTF_Init Fail: %s\n", TTF_GetError());
    return -1;
  }

  SDL_DisplayMode mode;
  if (SDL_GetDesktopDisplayMode(0, &mode)) {
    logInfo("SDL_GetDesktopDisplayMode Fail: %s\n", SDL_GetError());
    return -1;
  }
  _Globals.DISPLAY_WIDTH = mode.w;
  _Globals.DISPLAY_HEIGHT = mode.h;
  _Globals.WINDOW_WIDTH = 640;
  _Globals.WINDOW_HEIGHT = 480;

  logInfo("\tDisplay Resolution: %dx%d\n", _Globals.DISPLAY_WIDTH, _Globals.DISPLAY_HEIGHT);

  const char *FONT_LOCATION = "./fonts/Roboto-Regular.ttf";
   _Globals.defaultFont = TTF_OpenFont(FONT_LOCATION, _Globals.defaultFontSize);

  _Globals.window = SDL_CreateWindow("Choice of games interpreter",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               _Globals.WINDOW_WIDTH > 0 ? _Globals.WINDOW_WIDTH : _Globals.DISPLAY_WIDTH,
                               _Globals.WINDOW_HEIGHT > 0 ? _Globals.WINDOW_HEIGHT : _Globals.DISPLAY_HEIGHT,
                               SDL_WINDOW_SHOWN);
  if (_Globals.window == NULL) {
    logInfo("\tSDL_CreateWindow Fail: %s\n", SDL_GetError());
    return -1;
  }

  _Globals.renderer = SDL_CreateRenderer(_Globals.window, -1, SDL_RENDERER_SOFTWARE);
  if (_Globals.renderer == NULL) {
    logInfo("\tSDL_CreateRenderer Fail: %s\n", SDL_GetError());
    return -1;
  }

  return 0;
}
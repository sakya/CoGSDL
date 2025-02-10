#include "CoGSDL.h"

int init()
{
  logInit("CoGSDL.log");
  logInfo("Initializing\n");

  char *locale;
  locale = setlocale(LC_ALL, "");
  logInfo("\tLocale: %s\n", locale);

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    logError("\tSDL_Init Fail: %s\n", SDL_GetError());
    return -1;
  }

  if (TTF_Init() < 0) {
    logError("\tTTF_Init Fail: %s\n", TTF_GetError());
    return -1;
  }

  if (IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) < 0) {
    logError("\tIMG_Init Fail: %s\n", TTF_GetError());
    return -1;
  }

  SDL_DisplayMode mode;
  if (SDL_GetDesktopDisplayMode(0, &mode)) {
    logError("SDL_GetDesktopDisplayMode Fail: %s\n", SDL_GetError());
    return -1;
  }
  _Globals.DISPLAY_WIDTH = mode.w;
  _Globals.DISPLAY_HEIGHT = mode.h;
  _Globals.WINDOW_WIDTH = 640;
  _Globals.WINDOW_HEIGHT = 480;

  logInfo("\tDisplay Resolution: %dx%d\n", _Globals.DISPLAY_WIDTH, _Globals.DISPLAY_HEIGHT);

  _Globals.defaultFontSize = 18;
  _Globals.defaultFont = TTF_OpenFont("./fonts/Roboto-Regular.ttf", _Globals.defaultFontSize);

  _Globals.defaultSmallFontSize = 14;
  _Globals.defaultSmallFont = TTF_OpenFont("./fonts/Roboto-Light.ttf", _Globals.defaultSmallFontSize);

  button_x = IMG_Load("images/button_x.png");
  button_y = IMG_Load("images/button_y.png");
  button_a = IMG_Load("images/button_a.png");
  button_b = IMG_Load("images/button_b.png");
  dpad = IMG_Load("images/dpad.png");
  dpad_up_down = IMG_Load("images/dpad_up_down.png");

  _Globals.window = SDL_CreateWindow("Choice of games interpreter",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               _Globals.WINDOW_WIDTH > 0 ? _Globals.WINDOW_WIDTH : _Globals.DISPLAY_WIDTH,
                               _Globals.WINDOW_HEIGHT > 0 ? _Globals.WINDOW_HEIGHT : _Globals.DISPLAY_HEIGHT,
                               SDL_WINDOW_SHOWN);
  if (_Globals.window == NULL) {
    logError("\tSDL_CreateWindow Fail: %s\n", SDL_GetError());
    return -1;
  }

  _Globals.renderer = SDL_CreateRenderer(_Globals.window, -1, SDL_RENDERER_SOFTWARE);
  if (_Globals.renderer == NULL) {
    logError("\tSDL_CreateRenderer Fail: %s\n", SDL_GetError());
    return -1;
  }

  SDL_SetRenderDrawBlendMode(_Globals.renderer, SDL_BLENDMODE_BLEND);

  _Globals.logLevel = Debug;
  _Globals.status = GameSelect;
  return 0;
}
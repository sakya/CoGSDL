#include "CoGSDL.h"

void load_images();
void load_sounds();

int init()
{
  _Globals.logLevel = Debug;
  _Globals.scroll = scroll_page;
  _Globals.justify_text = 1;
  _Globals.quit = 0;

  logInit("CoGSDL.log");
  logInfo("Initializing");

  char *locale;
  locale = setlocale(LC_ALL, "");
  logInfo("\tLocale: %s", locale);

  settingsLoad();

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    logError("\tSDL_Init Fail: %s", SDL_GetError());
    return -1;
  }

  if (TTF_Init() < 0) {
    logError("\tTTF_Init Fail: %s", TTF_GetError());
    return -1;
  }

  if (IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) < 0) {
    logError("\tIMG_Init Fail: %s", TTF_GetError());
    return -1;
  }

  if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
    logError("\tMix_OpenAudio Fail: %s", Mix_GetError());
    return -1;
  }

  SDL_DisplayMode mode;
  if (SDL_GetDesktopDisplayMode(0, &mode)) {
    logError("SDL_GetDesktopDisplayMode Fail: %s", SDL_GetError());
    return -1;
  }
  _Globals.displayWidth = mode.w;
  _Globals.displayHeight = mode.h;
  _Globals.windowWidth = 640;
  _Globals.windowHeight = 480;

  logInfo("\tDisplay Resolution: %dx%d", _Globals.displayWidth, _Globals.displayHeight);

  _Globals.defaultFontSize = 21;
  _Globals.textFontSize = 21;
  _Globals.defaultFont = TTF_OpenFont("./fonts/Roboto-Regular.ttf", _Globals.defaultFontSize);

  _Globals.textFont = TTF_OpenFont("./fonts/Cardo-Regular.ttf", _Globals.textFontSize);
  _Globals.textItalicFont = TTF_OpenFont("./fonts/Cardo-Italic.ttf", _Globals.textFontSize);
  _Globals.textBoldFont = TTF_OpenFont("./fonts/Cardo-Bold.ttf", _Globals.textFontSize);

  _Globals.defaultSmallFontSize = 16;
  _Globals.defaultSmallFont = TTF_OpenFont("./fonts/Roboto-Light.ttf", _Globals.defaultSmallFontSize);

  load_images();
  load_sounds();

  _Globals.window = SDL_CreateWindow("CoGSDL",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               _Globals.windowWidth > 0 ? _Globals.windowWidth : _Globals.displayWidth,
                               _Globals.windowHeight > 0 ? _Globals.windowHeight : _Globals.displayHeight,
                               SDL_WINDOW_SHOWN);
  if (_Globals.window == NULL) {
    logError("\tSDL_CreateWindow Fail: %s", SDL_GetError());
    return -1;
  }

  int w, h = 0;
  SDL_GetWindowSize(_Globals.window, &w, &h);
  logInfo("\tWindow Resolution: %dx%d", w, h);

  _Globals.renderer = SDL_CreateRenderer(_Globals.window, -1, SDL_RENDERER_SOFTWARE);
  if (_Globals.renderer == NULL) {
    logError("\tSDL_CreateRenderer Fail: %s", SDL_GetError());
    return -1;
  }

  SDL_SetRenderDrawBlendMode(_Globals.renderer, SDL_BLENDMODE_BLEND);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

  _Globals.logLevel = Debug;
  _Globals.scroll = scroll_page;
  _Globals.status = MainMenu;
  _Globals.gamesCount = listGames("./games/", _Globals.games);

  return 0;
}

void load_images()
{
  app_icon = IMG_Load("images/icon.png");
  button_x = IMG_Load("images/button_x.png");
  button_y = IMG_Load("images/button_y.png");
  button_a = IMG_Load("images/button_a.png");
  button_b = IMG_Load("images/button_b.png");
  dpad = IMG_Load("images/dpad.png");
  dpad_up_down = IMG_Load("images/dpad_up_down.png");
}

void free_fonts()
{
  TTF_CloseFont(_Globals.defaultFont);
  _Globals.defaultFont = NULL;

  TTF_CloseFont(_Globals.textFont);
  _Globals.textFont = NULL;

  TTF_CloseFont(_Globals.textBoldFont);
  _Globals.textBoldFont = NULL;

  TTF_CloseFont(_Globals.textItalicFont);
  _Globals.textItalicFont = NULL;
}

void free_images()
{
  SDL_FreeSurface(app_icon);
  SDL_FreeSurface(button_x);
  SDL_FreeSurface(button_y);
  SDL_FreeSurface(button_a);
  SDL_FreeSurface(button_b);
  SDL_FreeSurface(dpad);
  SDL_FreeSurface(dpad_up_down);
}

void load_sounds()
{
  sound_menu_change = Mix_LoadWAV("sounds/menu_change.wav");
}

void free_sounds()
{
  Mix_FreeChunk(sound_menu_change);
  sound_menu_change = NULL;
}
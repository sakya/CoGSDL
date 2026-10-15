#include "CoGSDL.h"

void load_images();
void load_sounds();

const text_font_family textFontFamilies[] = {
  { "Cardo", "Cardo" },
  { "Charis SIL", "CharisSIL" },
  { "Literata", "Literata" }
};
const int textFontFamiliesCount = sizeof(textFontFamilies) / sizeof(textFontFamilies[0]);

int init()
{
  // Default, settings.cfg can change it (logLevel=Debug|Info|Warning|Error)
  _Globals.logLevel = Info;
  _Globals.scroll = scroll_page;
  _Globals.justify_text = 1;
  _Globals.show_fps = 0;
  _Globals.quit = 0;
  _Globals.textFontFamily = 0; // Cardo
  #ifdef MIYOO
    _Globals.textFontSize = 32;
  #else
    _Globals.textFontSize = 21;
  #endif

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
    logWarning("\tMix_OpenAudio Fail: %s (audio disabled)", Mix_GetError());
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

  #ifdef MIYOO
    _Globals.titleFontSize = 28;
    _Globals.defaultSmallFontSize = 21;
    _Globals.defaultFontSize = 26;
  #else
    _Globals.titleFontSize = 24;
    _Globals.defaultSmallFontSize = 16;
    _Globals.defaultFontSize = 21;
  #endif

  _Globals.defaultFont = TTF_OpenFont("./fonts/Roboto-Regular.ttf", _Globals.defaultFontSize);
  _Globals.defaultFontHeight = TTF_FontHeight(_Globals.defaultFont);
  _Globals.titleFont = TTF_OpenFont("./fonts/Roboto-Regular.ttf", _Globals.titleFontSize);

  loadTextFonts();

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

  #ifdef MIYOO
    // The renderer of the Miyoo SDL2 port (MI_GFX) can only copy textures to the screen:
    // no clear, no rectangles, no blending, no rotation. Draw everything with the SDL
    // software renderer on a surface and copy the whole frame to the screen (presentFrame)
    _Globals.screenRenderer = SDL_CreateRenderer(_Globals.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (_Globals.screenRenderer == NULL) {
      logError("\tSDL_CreateRenderer Fail: %s", SDL_GetError());
      return -1;
    }
    _Globals.screenSurface = SDL_CreateRGBSurfaceWithFormat(0, _Globals.windowWidth, _Globals.windowHeight, 32, SDL_PIXELFORMAT_ARGB8888);
    _Globals.screenTexture = SDL_CreateTexture(_Globals.screenRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                               _Globals.windowWidth, _Globals.windowHeight);
    if (_Globals.screenSurface == NULL || _Globals.screenTexture == NULL) {
      logError("\tScreen buffer Fail: %s", SDL_GetError());
      return -1;
    }
    _Globals.renderer = SDL_CreateSoftwareRenderer(_Globals.screenSurface);
  #else
    _Globals.renderer = SDL_CreateRenderer(_Globals.window, -1, SDL_RENDERER_SOFTWARE);
  #endif

  if (_Globals.renderer == NULL) {
    logError("\tSDL_CreateRenderer Fail: %s", SDL_GetError());
    return -1;
  }

  // All the rectangles are opaque: without blending the software renderer fills them much faster
  SDL_SetRenderDrawBlendMode(_Globals.renderer, SDL_BLENDMODE_NONE);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

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

/// @brief (Re)open the game text fonts using _Globals.textFontSize
/// @return 0 on success. On failure the current fonts are kept
int loadTextFonts()
{
  if (_Globals.textFontFamily < 0 || _Globals.textFontFamily >= textFontFamiliesCount)
    _Globals.textFontFamily = 0;
  const char *file = textFontFamilies[_Globals.textFontFamily].file;
  char path[256];

  snprintf(path, sizeof(path), "./fonts/%s-Regular.ttf", file);
  TTF_Font *regular = TTF_OpenFont(path, _Globals.textFontSize);
  snprintf(path, sizeof(path), "./fonts/%s-Italic.ttf", file);
  TTF_Font *italic = TTF_OpenFont(path, _Globals.textFontSize);
  snprintf(path, sizeof(path), "./fonts/%s-Bold.ttf", file);
  TTF_Font *bold = TTF_OpenFont(path, _Globals.textFontSize);
  if (regular == NULL || italic == NULL || bold == NULL) {
    logError("\tCannot open the text fonts %s (size %d): %s", textFontFamilies[_Globals.textFontFamily].name, _Globals.textFontSize, TTF_GetError());
    if (regular)
      TTF_CloseFont(regular);
    if (italic)
      TTF_CloseFont(italic);
    if (bold)
      TTF_CloseFont(bold);
    return -1;
  }

  if (_Globals.textFont)
    TTF_CloseFont(_Globals.textFont);
  if (_Globals.textItalicFont)
    TTF_CloseFont(_Globals.textItalicFont);
  if (_Globals.textBoldFont)
    TTF_CloseFont(_Globals.textBoldFont);
  _Globals.textFont = regular;
  _Globals.textItalicFont = italic;
  _Globals.textBoldFont = bold;

  // The text cache is keyed by font pointer: a new font could reuse the
  // address of a closed one
  sdlCacheClear();
  return 0;
}

void free_fonts()
{
  TTF_CloseFont(_Globals.defaultFont);
  _Globals.defaultFont = NULL;

  TTF_CloseFont(_Globals.titleFont);
  _Globals.titleFont = NULL;

  TTF_CloseFont(_Globals.textFont);
  _Globals.textFont = NULL;

  TTF_CloseFont(_Globals.textBoldFont);
  _Globals.textBoldFont = NULL;

  TTF_CloseFont(_Globals.textItalicFont);
  _Globals.textItalicFont = NULL;

  TTF_CloseFont(_Globals.defaultSmallFont);
  _Globals.defaultSmallFont = NULL;
}

void free_images()
{
  sdlFreeSurface(app_icon);
  sdlFreeSurface(button_x);
  sdlFreeSurface(button_y);
  sdlFreeSurface(button_a);
  sdlFreeSurface(button_b);
  sdlFreeSurface(dpad);
  sdlFreeSurface(dpad_up_down);
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
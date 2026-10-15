#include "CoGSDL.h"

void drawHeader();
void drawFooter();
void drawAppIcon();

#define TARGET_FPS 60

static int fps = 0;
static Uint64 fpsStart = 0;
static int fpsCount = 0;
static double header_angle = 0;

int draw()
{
  const Uint64 freq = SDL_GetPerformanceFrequency();
  const Uint64 frameTicks = freq / TARGET_FPS;
  // Absolute time of the next frame: a frame that waits too long is
  // compensated by the following one, so the average stays at TARGET_FPS
  Uint64 nextFrame = SDL_GetPerformanceCounter();
  fpsStart = nextFrame;
  fpsCount = 0;

  while (_Globals.quit == 0) {

    SDL_SetRenderDrawColor(_Globals.renderer, _theme->backgroundColor.r, _theme->backgroundColor.g, _theme->backgroundColor.b, 255);
    SDL_RenderClear(_Globals.renderer);

    drawHeader();
    drawFooter();
    switch (_Globals.status)
    {
      case MainMenu:
        drawMainMenu();
        break;
      case About:
        drawAbout();
        break;
      case GameSelect:
        drawGameSelectMenu();
        break;
      case Game:
        drawGame();
        break;
      case GameChoice:
        drawGameChoice();
        break;
      case GameMenu:
        drawGameMenu();
        break;
      case GameStats:
        drawGameStats();
        break;
      case Settings:
        drawSettings();
        break;
      case OnScreenKeyboard:
        drawOsk();
        break;
    }
    manageEvents();
    presentFrame();

    // Cap to TARGET_FPS
    nextFrame += frameTicks;
    Uint64 now = SDL_GetPerformanceCounter();
    if (now < nextFrame) {
      Uint32 toWait = (Uint32)((nextFrame - now) * 1000 / freq);
      if (toWait > 0)
        SDL_Delay(toWait);
    } else if (now - nextFrame > frameTicks) {
      // More than a frame late (e.g. loading a scene): don't try to catch up
      nextFrame = now;
    }

    // Fps: frames counted over (at least) one second, rounded
    fpsCount++;
    now = SDL_GetPerformanceCounter();
    if (now - fpsStart >= freq) {
      fps = (int)((fpsCount * freq + (now - fpsStart) / 2) / (now - fpsStart));
      fpsStart = now;
      fpsCount = 0;
    }
  }

  return 0;
}

void presentFrame()
{
  SDL_RenderPresent(_Globals.renderer);
  sdlCacheEndFrame();
#ifdef MIYOO
  // Copy the whole frame to the screen
  SDL_UpdateTexture(_Globals.screenTexture, NULL, _Globals.screenSurface->pixels, _Globals.screenSurface->pitch);
  SDL_RenderCopy(_Globals.screenRenderer, _Globals.screenTexture, NULL, NULL);
  SDL_RenderPresent(_Globals.screenRenderer);
#endif
}

void drawHeader()
{
  SDL_SetRenderDrawColor(_Globals.renderer, _theme->headerBackgroundColor.r, _theme->headerBackgroundColor.g, _theme->headerBackgroundColor.b, 255);

  SDL_Rect rect = { 0, 0, _Globals.windowWidth, HEADER_HEIGHT };
  SDL_RenderFillRect(_Globals.renderer, &rect);

  if (runningGame != NULL && (_Globals.status == Game || _Globals.status == GameMenu || _Globals.status == GameStats || _Globals.status == GameChoice || _Globals.status == OnScreenKeyboard)) {
    if (runningGameIcon != NULL) {
      sdlBlitImage(runningGameIcon, 0, 0, HEADER_HEIGHT, HEADER_HEIGHT);
    } else {
      drawAppIcon();
    }
    sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, runningGame->title, 35, 2, 0, 0);
  } else if (_Globals.status != MainMenu && _Globals.status != About) {
    drawAppIcon();
    sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, "CoGSDL v" VERSION, 35, 2, 0, 0);
  }

  if (_Globals.show_fps && fps > 0) {
    char fpsStr[20];
    int width;
    snprintf(fpsStr, sizeof(fpsStr), "Fps: %i", fps);
    TTF_MeasureUTF8(_Globals.defaultFont, fpsStr, _Globals.windowWidth, &width, NULL);

    sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, fpsStr, _Globals.windowWidth - width - 2, 2, 0, 0);
  }
}

void drawFooter()
{
  SDL_SetRenderDrawColor(_Globals.renderer, _theme->headerBackgroundColor.r, _theme->headerBackgroundColor.g, _theme->headerBackgroundColor.b, 255);

  SDL_Rect rect = { 0, _Globals.windowHeight - HEADER_HEIGHT, _Globals.windowWidth, HEADER_HEIGHT };
  SDL_RenderFillRect(_Globals.renderer, &rect);
}

void drawAppIcon()
{
  SDL_Texture* texture = sdlGetImageTexture(app_icon, 26, 26);
  SDL_Rect rect = {2, 2, 26, 26};
  SDL_Point center = { 13, 13};
  SDL_RendererFlip flip = SDL_FLIP_NONE;
  SDL_RenderCopyEx(_Globals.renderer, texture, NULL, &rect, header_angle, &center, flip);

  header_angle += 0.5;
  if (header_angle >= 360)
    header_angle = 0;
}
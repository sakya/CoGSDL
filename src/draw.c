#include "CoGSDL.h"

void drawHeader();
void drawFooter();
void drawAppIcon();

static int fps = 0;
static Uint64 fpsStart = 0;
static int fpsCount = 0;
static double header_angle = 0;

int draw()
{
  while (_Globals.quit == 0) {
    Uint64 start = SDL_GetPerformanceCounter();
    if (fpsStart == 0) {
      fpsStart = start;
    }

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

    Uint64 end = SDL_GetPerformanceCounter();
    float elapsedMs = (end - start) / (float)SDL_GetPerformanceFrequency() * 1000.0f;
    // Cap to 60 FPS
    int toWait = floor(16.666f - elapsedMs);
    if (toWait > 0) {
      SDL_Delay(toWait);
    }

    // Fps
    if (++fpsCount == 50) {
      end = SDL_GetPerformanceCounter();
      elapsedMs = (end - fpsStart) / (float)SDL_GetPerformanceFrequency() * 1000.0f;
      fps = floor(50000.0f / elapsedMs);

      fpsStart = 0;
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
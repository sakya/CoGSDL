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
    }
    manageEvents();
    SDL_RenderPresent(_Globals.renderer);

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

void drawHeader()
{
  SDL_SetRenderDrawColor(_Globals.renderer, _theme->headerBackgroundColor.r, _theme->headerBackgroundColor.g, _theme->headerBackgroundColor.b, 255);

  SDL_Rect rect = { 0, 0, _Globals.windowWidth, HEADER_HEIGHT };
  SDL_RenderFillRect(_Globals.renderer, &rect);

  if (runningGame != NULL && (_Globals.status == Game || _Globals.status == GameMenu || _Globals.status == GameStats || _Globals.status == GameChoice)) {
    if (runningGameIcon != NULL) {
      sdlBlitImage(runningGameIcon, 0, 0, 30, 30);
    } else {
      drawAppIcon();
    }
    sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, runningGame->title, 35, 2, 0, 0);
  } else if (_Globals.status != MainMenu && _Globals.status != About) {
    drawAppIcon();
    sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, "CoGSDL v1.0.0", 35, 2, 0, 0);
  }

  if (fps > 0) {
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
  SDL_Texture* texture = SDL_CreateTextureFromSurface(_Globals.renderer, app_icon);
  SDL_Rect rect = {2, 2, 26, 26};
  SDL_Rect crop = {0, 0, app_icon->w, app_icon->h};
  SDL_Point center = { 13, 13};
  SDL_RendererFlip flip = SDL_FLIP_NONE;
  SDL_RenderCopyEx(_Globals.renderer, texture, &crop , &rect, header_angle, &center, flip);
  SDL_DestroyTexture(texture);

  header_angle += 0.5;
  if (header_angle >= 360)
    header_angle = 0;
}
#include "CoGSDL.h"

void drawHeader();
void drawFooter();
void drawAppIcon();

double header_angle = 0;

int draw()
{
  while (_Globals.quit == 0) {
    int start = SDL_GetPerformanceCounter();

    SDL_SetRenderDrawColor(_Globals.renderer, colorBackground.r, colorBackground.g, colorBackground.b, 255);
    SDL_RenderClear(_Globals.renderer);

    drawHeader();
    drawFooter();
    switch (_Globals.status)
    {
      case MainMenu:
        drawMainMenu();
        break;
      case GameSelect:
        drawGameSelectMenu();
        break;
      case Game:
        drawGame();
        break;
    }
    manageEvents();
    SDL_RenderPresent(_Globals.renderer);

    int end = SDL_GetPerformanceCounter();
    float elapsedMs = (end - start) / (float)SDL_GetPerformanceFrequency() * 1000.0f;
    // Cap to 60 FPS
    int toWait = floor(16.666f - elapsedMs);
    if (toWait > 0) {
      SDL_Delay(toWait);
    }
  }

  return 0;
}

void drawHeader()
{
  SDL_SetRenderDrawColor(_Globals.renderer, colorHeaderBackground.r, colorHeaderBackground.g, colorHeaderBackground.b, 255);

  SDL_Rect rect;
  rect.x = 0;
  rect.y = 0;
  rect.w = _Globals.WINDOW_WIDTH;
  rect.h = HEADER_HEIGHT;

  SDL_RenderFillRect(_Globals.renderer, &rect);

  if (_Globals.status == Game && runningGame != NULL) {
    if (runningGameIcon != NULL) {
      sdlBlitImage(runningGameIcon, 0, 0, 30, 30);
    } else {
      drawAppIcon();
    }
    sdlWriteText(_Globals.defaultFont, colorTitleText, runningGame->title, 35, 5, 0, 0);
  } else if (_Globals.status != MainMenu) {
    drawAppIcon();
    sdlWriteText(_Globals.defaultFont, colorTitleText, "CoGSDL v.1.0", 35, 5, 0, 0);
  }
}

void drawFooter()
{
  SDL_SetRenderDrawColor(_Globals.renderer, colorHeaderBackground.r, colorHeaderBackground.g, colorHeaderBackground.b, 255);

  SDL_Rect rect;
  rect.x = 0;
  rect.y = _Globals.WINDOW_HEIGHT - HEADER_HEIGHT;
  rect.w = _Globals.WINDOW_WIDTH;
  rect.h = HEADER_HEIGHT;

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
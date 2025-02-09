#include "CoGSDL.h"

void drawHeader();
void drawFooter();

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
        break;
      case GameSelect:
        drawGameSelectMenu();
        break;
      case Game:
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

  sdlWriteText(_Globals.defaultFont, colorTitleText, "CoGSDL v.1.0", 5, 5, 0, 0);
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
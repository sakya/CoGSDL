#include "CoGSDL.h"

int draw()
{
  while (_Globals.quit == 0) {
    int start = SDL_GetPerformanceCounter();

    SDL_SetRenderDrawColor(_Globals.renderer, 30, 30, 30, 255);
    SDL_RenderClear(_Globals.renderer);
    switch (_Globals.status)
    {
      case MainMenu:
        drawMainMenu();
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
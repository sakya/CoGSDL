#include "CoGSDL.h"

int draw()
{
  while (_Globals.quit == 0) {
    manageEvents();
    SDL_RenderPresent(_Globals.renderer);
  }

  return 0;
}
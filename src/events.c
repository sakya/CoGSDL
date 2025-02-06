#include "CoGSDL.h"

int manageEvents()
{
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT)  {
      _Globals.quit = 1;
      break;
    }
  }

  return 0;
}
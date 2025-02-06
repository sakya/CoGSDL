#include "CoGSDL.h"

int manageEvents()
{
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type)
    {
      case SDL_QUIT:
        _Globals.quit = 1;
        return 0;
        break;

      default:
        break;
    }
  }

  return 0;
}
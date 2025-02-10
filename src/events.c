#include "CoGSDL.h"

void manageEvents()
{
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type)
    {
      case SDL_QUIT:
        _Globals.quit = 1;
        return;
        break;

      case SDL_KEYDOWN:
      if (!event.key.repeat)
          _Globals.input[event.key.keysym.scancode] = 1;
        break;
      case SDL_KEYUP:
        _Globals.input[event.key.keysym.scancode] = 0;
        break;

      default:
        break;
    }
  }
}
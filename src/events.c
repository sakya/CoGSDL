#include "CoGSDL.h"

static Uint64 lastRepeat[256];
static const int repeatMs = 100;

void manageEvents()
{
  SDL_Event event;
  Uint64 eventTime = SDL_GetPerformanceCounter();
  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
      case SDL_QUIT:
        _Globals.quit = 1;
        return;
        break;

      case SDL_KEYDOWN:
        if (!event.key.repeat || (eventTime - lastRepeat[event.key.keysym.scancode]) / (float)SDL_GetPerformanceFrequency() * 1000.0f >= repeatMs) {
          _Globals.input[event.key.keysym.scancode] = 1;
          lastRepeat[event.key.keysym.scancode] = eventTime;
        }
        break;
      case SDL_KEYUP:
        _Globals.input[event.key.keysym.scancode] = 0;
        break;

      default:
        break;
    }
  }
}
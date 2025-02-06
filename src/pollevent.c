#include "CoGSDL.h"

void handle_keydown(SDLKey sym);
void handle_keyup(SDLKey sym);

void pollevent() {
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      _G.quit = 1;
      break;
    case SDL_KEYDOWN:
      handle_keydown(event.key.keysym.sym);
      break;
    case SDL_KEYUP:
      handle_keyup(event.key.keysym.sym);
      break;
    default:
      break;
    }
  }
}

void handle_keydown(SDLKey sym) {

}

void handle_keyup(SDLKey sym) {

}
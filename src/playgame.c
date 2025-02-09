#include "CoGSDL.h"

void drawGame()
{
  if (_Globals.input[_Keys.KeyStart] == 1) {
    _Globals.input[_Keys.KeyStart] = 0;

    freeGame(runningGame);
    SDL_FreeSurface(runningGameIcon);
    _Globals.status = GameSelect;
  }
}
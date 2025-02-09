#include "CoGSDL.h"

void drawGame()
{
  if (_Globals.input[_Keys.KeyStart] == 1) {
    _Globals.input[_Keys.KeyStart] = 0;

    freeGame(runningGame);
    runningGame = NULL;
    SDL_FreeSurface(runningGameIcon);
    runningGameIcon = NULL;
    _Globals.status = GameSelect;
  }
}
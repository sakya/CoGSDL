#include "CoGSDL.h"

void drawMainMenu()
{
  int i = 0;
  int y = 10;
  for (i = 0; i < _Globals.gamesCount; i++) {
    game* g = &_Globals.games[i];
    if (g != NULL) {
      sdlWriteText(g->title, 10, y, 0, 0);

      y += 30;
    }
  }
}
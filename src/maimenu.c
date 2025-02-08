#include "CoGSDL.h"

int firstIdx = 0;
int selectedIdx = 0;

void drawMainMenu()
{
  int i = 0;
  int y = 10;

  int items = _Globals.WINDOW_HEIGHT / 50;
  SDL_Color white = {255, 255, 255};
  SDL_Color selected = {95, 195, 248};

  if (_Globals.input[SDL_SCANCODE_DOWN] == 1 && selectedIdx + 1 < _Globals.gamesCount) {
    selectedIdx++;
    if (selectedIdx - firstIdx > items)
      firstIdx++;
    _Globals.input[SDL_SCANCODE_DOWN] = 0;
  }
  if (_Globals.input[SDL_SCANCODE_UP] == 1 && selectedIdx > 0) {
    selectedIdx--;
    if (selectedIdx < firstIdx)
      firstIdx = selectedIdx;
    _Globals.input[SDL_SCANCODE_UP] = 0;
  }


  for (i = firstIdx; i < _Globals.gamesCount; i++) {
    game* g = &_Globals.games[i];
    SDL_Color color = i == selectedIdx ? selected : white;
    if (g != NULL) {
      sdlWriteText(_Globals.defaultFont, color, g->title, 10, y, 0, 0);
      y += 20;
      sdlWriteText(_Globals.defaultSmallFont, color, g->author, 10, y, 0, 0);

      y += 25;
      if (y + 30 > _Globals.WINDOW_HEIGHT)
        break;
    }
  }

  game* selGame = &_Globals.games[selectedIdx];
  if (strlen(selGame->icon) > 0) {
    sdlBlitImage(selGame->icon, 440, 10, 200, 300);
  }
}
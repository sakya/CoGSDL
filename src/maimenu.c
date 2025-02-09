#include "CoGSDL.h"

int firstIdx = 0;
int selectedIdx = 0;
SDL_Surface *iconSurface = NULL;

void drawMainMenu()
{
  int i = 0;
  int y = 40;

  const int items = (_Globals.WINDOW_HEIGHT - HEADER_HEIGHT) / 50;
  if (_Globals.input[SDL_SCANCODE_DOWN] == 1 && selectedIdx + 1 < _Globals.gamesCount) {
    selectedIdx++;
    if (iconSurface != NULL) {
      SDL_FreeSurface(iconSurface);
      iconSurface = NULL;
    }
    if (selectedIdx - firstIdx + 1 > items)
      firstIdx++;
    _Globals.input[SDL_SCANCODE_DOWN] = 0;
  }
  if (_Globals.input[SDL_SCANCODE_UP] == 1 && selectedIdx > 0) {
    selectedIdx--;
    if (iconSurface != NULL) {
      SDL_FreeSurface(iconSurface);
      iconSurface = NULL;
    }
    if (selectedIdx < firstIdx)
      firstIdx = selectedIdx;
    _Globals.input[SDL_SCANCODE_UP] = 0;
  }

  game* selGame = &_Globals.games[selectedIdx];
  for (i = firstIdx; i < _Globals.gamesCount; i++) {
    game* g = &_Globals.games[i];
    SDL_Color color = i == selectedIdx ? colorSelectedText : colorText;
    if (g != NULL) {
      sdlWriteText(_Globals.defaultFont, color, g->title, 10, y, 0, 0);
      y += 20;
      sdlWriteText(_Globals.defaultSmallFont, color, g->author, 10, y, 0, 0);

      y += 25;
      if (i - firstIdx + 1 >= items)
        break;
    }
  }

  if (strlen(selGame->icon) > 0) {
    if (iconSurface == NULL) {
      iconSurface = IMG_Load(selGame->icon);
    }
    sdlBlitImage(iconSurface, 440, 10 + HEADER_HEIGHT, 200, 300);
  }
}
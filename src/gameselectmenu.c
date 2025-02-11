#include "CoGSDL.h"

int firstIdx = 0;
int selectedIdx = 0;
SDL_Surface *iconSurface = NULL;

void drawGameSelectMenu()
{
  if (_Globals.gamesCount == 0)
    return;

  int i = 0;
  int y = 40;

  const int items = (_Globals.WINDOW_HEIGHT - HEADER_HEIGHT) / 50;
  if (_Globals.input[_Keys.KeyDown] == 1) {
    selectedIdx++;
    Mix_PlayChannel(-1, sound_menu_change, 0);
    if (selectedIdx == _Globals.gamesCount) {
      selectedIdx = 0;
      firstIdx = 0;
    }
    if (iconSurface != NULL) {
      SDL_FreeSurface(iconSurface);
      iconSurface = NULL;
    }
    if (selectedIdx - firstIdx + 1 > items)
      firstIdx++;
    _Globals.input[_Keys.KeyDown] = 0;
  } else if (_Globals.input[_Keys.KeyUp] == 1) {
    selectedIdx--;
    Mix_PlayChannel(-1, sound_menu_change, 0);
    if (selectedIdx < 0) {
      selectedIdx = _Globals.gamesCount - 1;
      firstIdx = _Globals.gamesCount - items;
      if (firstIdx < 0)
        firstIdx = 0;
    }
    if (iconSurface != NULL) {
      SDL_FreeSurface(iconSurface);
      iconSurface = NULL;
    }
    if (selectedIdx < firstIdx)
      firstIdx = selectedIdx;
    _Globals.input[_Keys.KeyUp] = 0;
  } else if (_Globals.input[_Keys.KeyA] == 1) {
    runningGame = malloc(sizeof(game));
    loadGame(_Globals.games[selectedIdx].path, runningGame, 0);
    if (strlen(runningGame->icon) > 0) {
      runningGameIcon = IMG_Load(runningGame->icon);
    }

    _Globals.status = Game;
    return;
  } else if (_Globals.input[_Keys.KeyB] == 1) {
    if (iconSurface != NULL) {
      SDL_FreeSurface(iconSurface);
      iconSurface = NULL;
    }

    _Globals.status = MainMenu;
    return;
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

  sdlBlitImage(button_a, _Globals.WINDOW_WIDTH - 35, _Globals.WINDOW_HEIGHT - 30, 30, 30);
  sdlBlitImage(button_b, _Globals.WINDOW_WIDTH - 70, _Globals.WINDOW_HEIGHT - 30, 30, 30);
  sdlBlitImage(dpad_up_down, _Globals.WINDOW_WIDTH - 105, _Globals.WINDOW_HEIGHT - 30, 30, 30);
}
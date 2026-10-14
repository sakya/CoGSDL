#include "CoGSDL.h"

static int firstIdx = 0;
static int selectedIdx = -1;
static SDL_Surface *iconSurface = NULL;

void drawGameSelectMenu()
{
  if (selectedIdx < 0 && _Globals.gamesCount > 0) {
    selectedIdx = 0;
  }

  const int old_selected_idx = selectedIdx;
  const int items = (_Globals.windowHeight - HEADER_HEIGHT) / 52;
  if (_Globals.input[_Keys.KeyDown] == 1 && selectedIdx >= 0) {
    _Globals.input[_Keys.KeyDown] = 0;
    selectedIdx++;
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
  } else if (_Globals.input[_Keys.KeyUp] == 1 && selectedIdx >= 0) {
    _Globals.input[_Keys.KeyUp] = 0;
    selectedIdx--;
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
  } else if (_Globals.input[_Keys.KeyA] == 1 && selectedIdx >= 0) {
    _Globals.input[_Keys.KeyA] = 0;
    runningGame = malloc(sizeof(game));
    loadGame(_Globals.games[selectedIdx].path, runningGame, 0);
    if (strlen(runningGame->icon) > 0) {
      runningGameIcon = IMG_Load(runningGame->icon);
    }

    _Globals.status = Game;
    return;
  } else if (_Globals.input[_Keys.KeyB] == 1) {
    _Globals.input[_Keys.KeyB] = 0;
    if (iconSurface != NULL) {
      SDL_FreeSurface(iconSurface);
      iconSurface = NULL;
    }

    _Globals.status = MainMenu;
    return;
  }

  if (old_selected_idx != selectedIdx) {
    Mix_PlayChannel(-1, sound_menu_change, 0);
  }

  if (_Globals.gamesCount > 0) {
    int i = 0;
    int y = 40;
    game* selGame = &_Globals.games[selectedIdx];
    for (i = firstIdx; i < _Globals.gamesCount; i++) {
      game* g = &_Globals.games[i];
      SDL_Color color = i == selectedIdx ? _theme->titleTextColor : _theme->textColor;
      if (i == selectedIdx)
        sdlDrawSelection(y - 2, 50);
      if (g != NULL) {
        sdlWriteText(_Globals.defaultFont, color, g->title, 10, y, 0, 0);
        y += 25;
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
  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(button_b, _Globals.windowWidth - 70, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}
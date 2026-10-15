#include "CoGSDL.h"
#include "gui_playgame.h"

static int firstIdx = 0;
static int selectedIdx = -1;
static SDL_Surface *iconSurface = NULL;

/// @brief Set to 1 when the saves of the selected game are shown
static int slotsPage = 0;
/// @brief Selected item of the saves page: 0 = new game, 1..SAVE_SLOTS = slot
static int slotsItemIdx = 0;
static save_info slots[SAVE_SLOTS];
static char message[128];
static Uint32 messageTime = 0;

/// @brief Load the selected game (runningGame) and its icon
static void gs_openGame()
{
  runningGame = malloc(sizeof(game));
  loadGame(_Globals.games[selectedIdx].path, runningGame, 0);
  if (strlen(runningGame->icon) > 0) {
    runningGameIcon = IMG_Load(runningGame->icon);
  }
}

/// @brief Release the game opened by gs_openGame (not started)
static void gs_closeGame()
{
  // freeGame also frees the struct
  freeGame(runningGame);
  runningGame = NULL;
  sdlFreeSurface(runningGameIcon);
  runningGameIcon = NULL;
}

static int gs_isItemEnabled(int item)
{
  return item == 0 || slots[item - 1].used;
}

static void gs_moveItem(int delta)
{
  int idx = slotsItemIdx;
  for (int n = 0; n <= SAVE_SLOTS; n++) {
    idx = (idx + delta + SAVE_SLOTS + 1) % (SAVE_SLOTS + 1);
    if (gs_isItemEnabled(idx)) {
      slotsItemIdx = idx;
      playMenuSound();
      return;
    }
  }
}

/// @brief The page with "New game" and the saves of the selected game
static void gs_drawSlotsPage()
{
  if (_Globals.input[_Keys.KeyDown] == 1) {
    _Globals.input[_Keys.KeyDown] = 0;
    gs_moveItem(1);
  } else if (_Globals.input[_Keys.KeyUp] == 1) {
    _Globals.input[_Keys.KeyUp] = 0;
    gs_moveItem(-1);
  } else if (_Globals.input[_Keys.KeyA] == 1) {
    _Globals.input[_Keys.KeyA] = 0;
    if (slotsItemIdx == 0) {
      // New game: drawGame starts it
      slotsPage = 0;
      _Globals.status = Game;
      return;
    }

    pg_init();
    if (pg_loadGame(slotsItemIdx - 1)) {
      slotsPage = 0;
      _Globals.status = Game;
      return;
    }
    // The state of the game is not changed by a failed load, but the game must be opened again
    pg_dispose();
    gs_openGame();
    snprintf(message, sizeof(message), "Load failed: the save is not valid");
    messageTime = SDL_GetTicks();
  } else if (_Globals.input[_Keys.KeyB] == 1) {
    _Globals.input[_Keys.KeyB] = 0;
    gs_closeGame();
    slotsPage = 0;
    return;
  }

  const int x = 30;
  int y = HEADER_HEIGHT + 8;
  // Game image and title (as in the header during the game)
  int titleX = x;
  if (runningGameIcon != NULL) {
    sdlBlitImage(runningGameIcon, x, y - 2, HEADER_HEIGHT, HEADER_HEIGHT);
    titleX += HEADER_HEIGHT + 6;
  }
  sdlWriteText(_Globals.defaultFont, _theme->textColor, runningGame->title, titleX, y, _Globals.windowWidth - titleX - x, 0);

  y = HEADER_HEIGHT + 45;
  if (slotsItemIdx == 0)
    sdlDrawSelection(y - 2, sdlSelectionHeight());
  sdlWriteText(_Globals.defaultFont, slotsItemIdx == 0 ? _theme->titleTextColor : _theme->textColor, "New game", x, y, 0, 0);

  y = HEADER_HEIGHT + 85;
  sdlWriteText(_Globals.titleFont, _theme->textColor, "Load game", x, y, 0, 0);

  y = HEADER_HEIGHT + 120;
  for (int i = 0; i < SAVE_SLOTS; i++) {
    int selected = slotsItemIdx == i + 1;
    SDL_Color color = selected ? _theme->titleTextColor : _theme->textColor;
    if (!slots[i].used)
      color = _theme->disabledTextColor;
    if (selected)
      sdlDrawSelection(y - 2, 50);

    char line[320];
    if (slots[i].used)
      snprintf(line, sizeof(line), "%i. %s", i + 1, slots[i].name);
    else
      snprintf(line, sizeof(line), "%i. Empty", i + 1);
    sdlWriteText(_Globals.defaultFont, color, line, x, y, _Globals.windowWidth - x - 10, 0);
    if (slots[i].used) {
      char date[64];
      strftime(date, sizeof(date), "%Y-%m-%d %H:%M", localtime(&slots[i].time));
      sdlWriteText(_Globals.defaultSmallFont, color, date, x + 20, y + 25, 0, 0);
    }
    y += 52;
  }

  if (messageTime > 0 && SDL_GetTicks() - messageTime < 2500)
    sdlWriteText(_Globals.defaultSmallFont, _theme->selectedTextColor, message, x, _Globals.windowHeight - HEADER_HEIGHT - 22, 0, 0);

  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(button_b, _Globals.windowWidth - 70, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}

void gameSelectDispose()
{
  sdlFreeSurface(iconSurface);
  iconSurface = NULL;
}

void drawGameSelectMenu()
{
  if (slotsPage) {
    gs_drawSlotsPage();
    return;
  }

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
      sdlFreeSurface(iconSurface);
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
      sdlFreeSurface(iconSurface);
      iconSurface = NULL;
    }
    if (selectedIdx < firstIdx)
      firstIdx = selectedIdx;
  } else if (_Globals.input[_Keys.KeyA] == 1 && selectedIdx >= 0) {
    _Globals.input[_Keys.KeyA] = 0;
    gs_openGame();

    // If the game has saves, choose between a new game and a save
    int usedSlots = 0;
    for (int i = 0; i < SAVE_SLOTS; i++)
      usedSlots += pg_getSaveInfo(i, &slots[i]);
    if (usedSlots > 0) {
      slotsPage = 1;
      slotsItemIdx = 0;
      messageTime = 0;
      return;
    }

    _Globals.status = Game;
    return;
  } else if (_Globals.input[_Keys.KeyB] == 1) {
    _Globals.input[_Keys.KeyB] = 0;
    if (iconSurface != NULL) {
      sdlFreeSurface(iconSurface);
      iconSurface = NULL;
    }

    _Globals.status = MainMenu;
    return;
  }

  if (old_selected_idx != selectedIdx) {
    playMenuSound();
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
      // Right aligned, 5 pixels from the right edge of the screen
      int iconWidth;
      sdlImageSize(iconSurface, 200, 300, &iconWidth, NULL);
      sdlBlitImage(iconSurface, _Globals.windowWidth - iconWidth - 5, 10 + HEADER_HEIGHT, 200, 300);
    }
  }
  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(button_b, _Globals.windowWidth - 70, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}

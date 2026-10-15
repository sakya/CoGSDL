#include "CoGSDL.h"
#include "gui_playgame.h"

enum gm_page
{
  gm_main = 0,
  gm_save = 1,
  gm_load = 2,
  gm_confirm_overwrite = 3,
  gm_confirm_quit = 4,
  gm_message = 5
};

static enum gm_page page = gm_main;
static int mainIdx = 0;
static int slotIdx = 0;
static save_info slots[SAVE_SLOTS];
static char message[256];
static Uint32 messageTime = 0;
/// @brief The page shown after the message
static enum gm_page messageNextPage = gm_main;

static const char *mainItems[] = { "Save", "Load", "Quit" };
static const int mainItemsCount = sizeof(mainItems) / sizeof(mainItems[0]);

static void gm_refreshSlots()
{
  for (int i = 0; i < SAVE_SLOTS; i++)
    pg_getSaveInfo(i, &slots[i]);
}

static int gm_usedSlots()
{
  int count = 0;
  for (int i = 0; i < SAVE_SLOTS; i++)
    count += slots[i].used;
  return count;
}

static void gm_showMessage(const char *text, enum gm_page nextPage)
{
  snprintf(message, sizeof(message), "%s", text);
  messageTime = SDL_GetTicks();
  messageNextPage = nextPage;
  page = gm_message;
}

/// @brief Move the selection in the slot list (in the load page only the used slots can be selected)
static void gm_moveSlot(int delta)
{
  int idx = slotIdx;
  for (int n = 0; n < SAVE_SLOTS; n++) {
    idx = (idx + delta + SAVE_SLOTS) % SAVE_SLOTS;
    if (page != gm_load || slots[idx].used) {
      slotIdx = idx;
      Mix_PlayChannel(-1, sound_menu_change, 0);
      return;
    }
  }
}

static void gm_enterSlots(enum gm_page slotsPage)
{
  gm_refreshSlots();
  page = slotsPage;
  slotIdx = 0;
  if (slotsPage == gm_load) {
    if (gm_usedSlots() == 0) {
      gm_showMessage("No saved games", gm_main);
      return;
    }
    // First used slot
    while (!slots[slotIdx].used)
      slotIdx++;
  }
}

static void gm_doSave()
{
  char text[64];
  if (pg_saveGame(slotIdx)) {
    snprintf(text, sizeof(text), "Game saved in slot %i", slotIdx + 1);
    gm_showMessage(text, gm_main);
  } else {
    gm_showMessage("Save failed", gm_main);
  }
}

static void gm_drawFooter(int showA, int showB)
{
  if (showA)
    sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  if (showB)
    sdlBlitImage(button_b, _Globals.windowWidth - 70, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}

static void gm_drawSlots(const char *title)
{
  sdlWriteText(_Globals.titleFont, _theme->textColor, (char *)title, 30, HEADER_HEIGHT + 10, 0, 0);

  int y = HEADER_HEIGHT + 50;
  for (int i = 0; i < SAVE_SLOTS; i++) {
    int selectable = page != gm_load || slots[i].used;
    SDL_Color color = i == slotIdx ? _theme->titleTextColor : _theme->textColor;
    if (!selectable)
      color = _theme->disabledTextColor;
    if (i == slotIdx && selectable)
      sdlDrawSelection(y - 2, 50);

    char line[320];
    if (slots[i].used)
      snprintf(line, sizeof(line), "%i. %s", i + 1, slots[i].name);
    else
      snprintf(line, sizeof(line), "%i. Empty", i + 1);
    sdlWriteText(_Globals.defaultFont, color, line, 30, y, _Globals.windowWidth - 40, 0);
    y += 25;

    if (slots[i].used) {
      char date[64];
      strftime(date, sizeof(date), "%Y-%m-%d %H:%M", localtime(&slots[i].time));
      sdlWriteText(_Globals.defaultSmallFont, color, date, 50, y, 0, 0);
    }
    y += 27;
  }
}

static void gm_drawConfirm(const char *question, const char *detail)
{
  sdlWriteText(_Globals.defaultFont, _theme->textColor, (char *)question, 30, HEADER_HEIGHT + 60, _Globals.windowWidth - 40, 0);
  if (detail != NULL)
    sdlWriteText(_Globals.defaultSmallFont, _theme->textColor, (char *)detail, 30, HEADER_HEIGHT + 95, _Globals.windowWidth - 40, 0);
  sdlWriteText(_Globals.defaultFont, _theme->selectedTextColor, "A: Yes    B: No", 30, HEADER_HEIGHT + 140, 0, 0);
  gm_drawFooter(1, 1);
}

void drawGameMenu()
{
  if (page == gm_main || page == gm_save || page == gm_load) {
    if (_Globals.input[_Keys.KeyL1] == 1) {
      _Globals.input[_Keys.KeyL1] = 0;
      page = gm_main;
      _Globals.status = GameStats;
      return;
    }

    if (_Globals.input[_Keys.KeyR1] == 1) {
      _Globals.input[_Keys.KeyR1] = 0;
      page = gm_main;
      _Globals.status = Game;
      return;
    }
  }

  switch (page) {
    case gm_main:
      if (_Globals.input[_Keys.KeyDown] == 1) {
        _Globals.input[_Keys.KeyDown] = 0;
        mainIdx = (mainIdx + 1) % mainItemsCount;
        Mix_PlayChannel(-1, sound_menu_change, 0);
      } else if (_Globals.input[_Keys.KeyUp] == 1) {
        _Globals.input[_Keys.KeyUp] = 0;
        mainIdx = (mainIdx + mainItemsCount - 1) % mainItemsCount;
        Mix_PlayChannel(-1, sound_menu_change, 0);
      } else if (_Globals.input[_Keys.KeyA] == 1) {
        _Globals.input[_Keys.KeyA] = 0;
        if (mainIdx == 0)
          gm_enterSlots(gm_save);
        else if (mainIdx == 1)
          gm_enterSlots(gm_load);
        else
          page = gm_confirm_quit;
        return;
      } else if (_Globals.input[_Keys.KeyB] == 1) {
        _Globals.input[_Keys.KeyB] = 0;
        _Globals.status = Game;
        return;
      }

      sdlWriteText(_Globals.titleFont, _theme->textColor, "Game menu", 30, HEADER_HEIGHT + 10, 0, 0);
      for (int i = 0; i < mainItemsCount; i++) {
        int y = HEADER_HEIGHT + 40 + i * 30;
        if (i == mainItemsCount - 1) {
          y += 10;
        }
        if (i == mainIdx)
          sdlDrawSelection(y - 2, sdlSelectionHeight());
        sdlWriteText(_Globals.defaultFont, i == mainIdx ? _theme->titleTextColor : _theme->textColor, (char *)mainItems[i], 30, y, 0, 0);
      }
      gm_drawFooter(1, 1);
      break;

    case gm_save:
    case gm_load:
      if (_Globals.input[_Keys.KeyDown] == 1) {
        _Globals.input[_Keys.KeyDown] = 0;
        gm_moveSlot(1);
      } else if (_Globals.input[_Keys.KeyUp] == 1) {
        _Globals.input[_Keys.KeyUp] = 0;
        gm_moveSlot(-1);
      } else if (_Globals.input[_Keys.KeyA] == 1) {
        _Globals.input[_Keys.KeyA] = 0;
        if (page == gm_save) {
          if (slots[slotIdx].used)
            page = gm_confirm_overwrite;
          else
            gm_doSave();
        } else if (slots[slotIdx].used) {
          if (pg_loadGame(slotIdx)) {
            page = gm_main;
            _Globals.status = Game;
          } else {
            gm_showMessage("Load failed: the save is not valid", gm_load);
          }
        }
        return;
      } else if (_Globals.input[_Keys.KeyB] == 1) {
        _Globals.input[_Keys.KeyB] = 0;
        page = gm_main;
        return;
      }

      gm_drawSlots(page == gm_save ? "Save game" : "Load game");
      gm_drawFooter(1, 1);
      break;

    case gm_confirm_overwrite:
      if (_Globals.input[_Keys.KeyA] == 1) {
        _Globals.input[_Keys.KeyA] = 0;
        gm_doSave();
        return;
      } else if (_Globals.input[_Keys.KeyB] == 1) {
        _Globals.input[_Keys.KeyB] = 0;
        page = gm_save;
        return;
      }
      {
        char question[64];
        snprintf(question, sizeof(question), "Overwrite slot %i?", slotIdx + 1);
        gm_drawConfirm(question, slots[slotIdx].name);
      }
      break;

    case gm_confirm_quit:
      if (_Globals.input[_Keys.KeyA] == 1) {
        _Globals.input[_Keys.KeyA] = 0;
        page = gm_main;
        mainIdx = 0;
        pg_dispose();
        _Globals.status = MainMenu;
        return;
      } else if (_Globals.input[_Keys.KeyB] == 1) {
        _Globals.input[_Keys.KeyB] = 0;
        page = gm_main;
        return;
      }
      gm_drawConfirm("Quit the game?", "The progress after the last save will be lost.");
      break;

    case gm_message:
      // Close the message with A/B or after 2 seconds
      if (_Globals.input[_Keys.KeyA] == 1 || _Globals.input[_Keys.KeyB] == 1 || SDL_GetTicks() - messageTime > 2000) {
        _Globals.input[_Keys.KeyA] = 0;
        _Globals.input[_Keys.KeyB] = 0;
        if (messageNextPage == gm_load)
          gm_refreshSlots();
        page = messageNextPage;
        return;
      }
      sdlWriteText(_Globals.defaultFont, _theme->selectedTextColor, message, 30, HEADER_HEIGHT + 60, _Globals.windowWidth - 40, 0);
      gm_drawFooter(1, 0);
      break;
  }
}

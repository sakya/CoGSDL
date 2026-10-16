#include "CoGSDL.h"
#include "gui_playgame.h"
#include "gui_generic_menu.h"

enum gm_page
{
  gm_main = 0,
  gm_save = 1,
  gm_load = 2,
  gm_confirm_overwrite = 3,
  gm_confirm_quit = 4,
  gm_message = 5
};

enum gm_main_item
{
  gm_item_save = 0,
  gm_item_load,
  gm_item_quit
};

static enum gm_page page = gm_main;
static save_info slots[SAVE_SLOTS];
static char message[256];
static Uint32 messageTime = 0;
/// @brief The page shown after the message
static enum gm_page messageNextPage = gm_main;

static gme_item mainItems[] = {
  { .id = gm_item_save, .text = "Save" },
  { .id = gm_item_load, .text = "Load" },
  // "Quit" is separated from the other items
  { .id = gm_item_quit, .text = "Quit", .spaceBefore = 10 }
};
static gme_menu mainMenu;
static int mainMenuInitialized = 0;

/// @brief Menu of the save/load pages: the id of an item is the slot index
static gme_item slotsItems[SAVE_SLOTS];
static char slotsTexts[SAVE_SLOTS][320];
static char slotsDates[SAVE_SLOTS][64];
static gme_menu slotsMenu;

/// @brief Index of the selected slot in the save/load pages
static int gm_slotIdx()
{
  return slotsMenu.selectedIdx >= 0 ? slotsMenu.selectedIdx : 0;
}

/// @brief Read the saves and update the items of the save/load pages (the selection is kept)
static void gm_refreshSlots()
{
  int selected = slotsMenu.selectedIdx;
  memset(slotsItems, 0, sizeof(slotsItems));
  for (int i = 0; i < SAVE_SLOTS; i++) {
    pg_getSaveInfo(i, &slots[i]);
    gme_item *item = &slotsItems[i];
    item->id = i;
    if (slots[i].used) {
      snprintf(slotsTexts[i], sizeof(slotsTexts[i]), "%i. %s", i + 1, slots[i].name);
      strftime(slotsDates[i], sizeof(slotsDates[i]), "%Y-%m-%d %H:%M", localtime(&slots[i].time));
      item->subText = slotsDates[i];
    } else {
      snprintf(slotsTexts[i], sizeof(slotsTexts[i]), "%i. Empty", i + 1);
      // In the load page only the used slots can be selected
      item->disabled = page == gm_load;
    }
    item->text = slotsTexts[i];
  }

  gme_init(&slotsMenu, slotsItems, SAVE_SLOTS);
  slotsMenu.y = HEADER_HEIGHT + 48;
  slotsMenu.wrap = 1;
  if (selected >= 0)
    gme_select(&slotsMenu, selected);
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

static void gm_enterSlots(enum gm_page slotsPage)
{
  page = slotsPage;
  // Start from the first slot (in the load page the first used one)
  slotsMenu.selectedIdx = -1;
  gm_refreshSlots();
  if (slotsPage == gm_load && gm_usedSlots() == 0)
    gm_showMessage("No saved games", gm_main);
}

static void gm_doSave()
{
  char text[64];
  if (pg_saveGame(gm_slotIdx())) {
    snprintf(text, sizeof(text), "Game saved in slot %i", gm_slotIdx() + 1);
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

static void gm_drawTitle(const char *title, const gme_menu *menu)
{
  sdlWriteText(_Globals.titleFont, _theme->textColor, (char *)title, menu->margin + menu->textIndent, HEADER_HEIGHT + 10, 0, 0);
}

static void gm_drawConfirm(const char *question, const char *detail)
{
  sdlWriteText(_Globals.defaultFont, _theme->textColor, (char *)question, 30, HEADER_HEIGHT + 60, _Globals.windowWidth - 40, 0);
  if (detail != NULL)
    sdlWriteText(_Globals.defaultSmallFont, _theme->textColor, (char *)detail, 30, HEADER_HEIGHT + 95, _Globals.windowWidth - 40, 0);
  sdlWriteText(_Globals.defaultFont, _theme->selectedTextColor, "B: No    A: Yes", 30, HEADER_HEIGHT + 140, 0, 0);
  gm_drawFooter(1, 1);
}

void drawGameMenu()
{
  if (!mainMenuInitialized) {
    gme_init(&mainMenu, mainItems, sizeof(mainItems) / sizeof(mainItems[0]));
    mainMenu.y = HEADER_HEIGHT + 48;
    mainMenu.wrap = 1;
    mainMenuInitialized = 1;
  }

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
      switch (gme_handleInput(&mainMenu)) {
        case gme_event_selected:
          switch (gme_selectedItem(&mainMenu)->id) {
            case gm_item_save:
              gm_enterSlots(gm_save);
              break;
            case gm_item_load:
              gm_enterSlots(gm_load);
              break;
            case gm_item_quit:
              page = gm_confirm_quit;
              break;
          }
          return;
        case gme_event_back:
          _Globals.status = Game;
          return;
        default:
          break;
      }

      gm_drawTitle("Game menu", &mainMenu);
      gme_draw(&mainMenu);
      gm_drawFooter(1, 1);
      break;

    case gm_save:
    case gm_load:
      switch (gme_handleInput(&slotsMenu)) {
        case gme_event_selected:
          if (page == gm_save) {
            if (slots[gm_slotIdx()].used)
              page = gm_confirm_overwrite;
            else
              gm_doSave();
          } else if (slots[gm_slotIdx()].used) {
            if (pg_loadGame(gm_slotIdx())) {
              page = gm_main;
              _Globals.status = Game;
            } else {
              gm_showMessage("Load failed: the save is not valid", gm_load);
            }
          }
          return;
        case gme_event_back:
          page = gm_main;
          return;
        default:
          break;
      }

      gm_drawTitle(page == gm_save ? "Save game" : "Load game", &slotsMenu);
      gme_draw(&slotsMenu);
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
        snprintf(question, sizeof(question), "Overwrite slot %i?", gm_slotIdx() + 1);
        gm_drawConfirm(question, slots[gm_slotIdx()].name);
      }
      break;

    case gm_confirm_quit:
      if (_Globals.input[_Keys.KeyA] == 1) {
        _Globals.input[_Keys.KeyA] = 0;
        page = gm_main;
        gme_select(&mainMenu, 0);
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
        page = messageNextPage;
        if (page == gm_load)
          gm_refreshSlots();
        return;
      }
      sdlWriteText(_Globals.defaultFont, _theme->selectedTextColor, message, 30, HEADER_HEIGHT + 60, _Globals.windowWidth - 40, 0);
      gm_drawFooter(1, 0);
      break;
  }
}

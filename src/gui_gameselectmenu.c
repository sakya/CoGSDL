#include "CoGSDL.h"
#include "gui_playgame.h"
#include "gui_generic_menu.h"

static SDL_Surface *iconSurface = NULL;

/// @brief Menu with the list of the games (title and author)
static gme_menu gamesMenu;
static gme_item *gamesItems = NULL;
static int gamesItemsCount = -1;

/// @brief Set to 1 when the saves of the selected game are shown
static int slotsPage = 0;
/// @brief Menu of the saves page: item 0 = new game, 1..SAVE_SLOTS = slot
static gme_menu slotsMenu;
static gme_item slotsItems[SAVE_SLOTS + 1];
static char slotsTexts[SAVE_SLOTS][320];
static char slotsDates[SAVE_SLOTS][64];
static save_info slots[SAVE_SLOTS];
static char message[128];
static Uint32 messageTime = 0;

/// @brief Load the selected game (runningGame) and its icon
static void gs_openGame()
{
  runningGame = malloc(sizeof(game));
  loadGame(_Globals.games[gamesMenu.selectedIdx].path, runningGame, 0);
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

static void gs_freeIcon()
{
  if (iconSurface != NULL) {
    sdlFreeSurface(iconSurface);
    iconSurface = NULL;
  }
}

/// @brief Build the items of the games menu (when the list of the games changes)
static void gs_initGamesMenu()
{
  if (gamesItems != NULL && gamesItemsCount == _Globals.gamesCount)
    return;

  free(gamesItems);
  gamesItems = NULL;
  gamesItemsCount = _Globals.gamesCount;
  if (gamesItemsCount > 0) {
    gamesItems = calloc(gamesItemsCount, sizeof(gme_item));
    if (gamesItems == NULL)
      gamesItemsCount = 0;
  }
  for (int i = 0; i < gamesItemsCount; i++) {
    gamesItems[i].id = i;
    gamesItems[i].text = _Globals.games[i].title;
    gamesItems[i].subText = _Globals.games[i].author;
  }

  gme_init(&gamesMenu, gamesItems, gamesItemsCount);
  // From the last game go back to the first one (and vice versa)
  gamesMenu.wrap = 1;
  gs_freeIcon();
}

/// @brief Build the items of the saves page from slots[]
static void gs_initSlotsMenu()
{
  memset(slotsItems, 0, sizeof(slotsItems));
  slotsItems[0].id = 0;
  slotsItems[0].text = "New game";
  for (int i = 0; i < SAVE_SLOTS; i++) {
    gme_item *item = &slotsItems[i + 1];
    item->id = i + 1;
    if (slots[i].used) {
      snprintf(slotsTexts[i], sizeof(slotsTexts[i]), "%i. %s", i + 1, slots[i].name);
      strftime(slotsDates[i], sizeof(slotsDates[i]), "%Y-%m-%d %H:%M", localtime(&slots[i].time));
      item->subText = slotsDates[i];
    } else {
      snprintf(slotsTexts[i], sizeof(slotsTexts[i]), "%i. Empty", i + 1);
      item->disabled = 1;
    }
    item->text = slotsTexts[i];
  }
  // Space for the "Load game" title before the first slot
  slotsItems[1].spaceBefore = sdlTitleHeight() + 10;

  gme_init(&slotsMenu, slotsItems, SAVE_SLOTS + 1);
  slotsMenu.y = HEADER_HEIGHT + 43;
  // Leave space for the error message at the bottom
  slotsMenu.height = _Globals.windowHeight - HEADER_HEIGHT - slotsMenu.y - get_line_height(_Globals.defaultSmallFont) - 6;
}

/// @brief The page with "New game" and the saves of the selected game
static void gs_drawSlotsPage()
{
  switch (gme_handleInput(&slotsMenu)) {
    case gme_event_selected: {
      int item = gme_selectedItem(&slotsMenu)->id;
      if (item == 0) {
        // New game: drawGame starts it
        slotsPage = 0;
        _Globals.status = Game;
        return;
      }

      pg_init();
      if (pg_loadGame(item - 1)) {
        slotsPage = 0;
        _Globals.status = Game;
        return;
      }
      // The state of the game is not changed by a failed load, but the game must be opened again
      pg_dispose();
      gs_openGame();
      snprintf(message, sizeof(message), "Load failed: the save is not valid");
      messageTime = SDL_GetTicks();
      break;
    }
    case gme_event_back:
      gs_closeGame();
      slotsPage = 0;
      return;
    default:
      break;
  }

  const int x = slotsMenu.margin + slotsMenu.textIndent;
  int y = HEADER_HEIGHT + 8;
  // Game image and title (as in the header during the game)
  int titleX = x;
  if (runningGameIcon != NULL) {
    sdlBlitImage(runningGameIcon, x, y - 2, HEADER_HEIGHT, HEADER_HEIGHT);
    titleX += HEADER_HEIGHT + 6;
  }
  sdlWriteText(_Globals.defaultFont, _theme->textColor, runningGame->title, titleX, y, _Globals.windowWidth - titleX - x, 0);

  gme_draw(&slotsMenu);

  // "Load game" title, in the space before the first slot (if not scrolled away)
  if (slotsMenu.firstIdx == 0) {
    y = slotsMenu.y + gme_itemHeight(&slotsItems[0]) + slotsMenu.spacing + 5;
    sdlDrawTitle("Load game", x, y);
  }

  if (messageTime > 0 && SDL_GetTicks() - messageTime < 2500)
    sdlWriteText(_Globals.defaultSmallFont, _theme->selectedTextColor, message, x, _Globals.windowHeight - HEADER_HEIGHT - 22, 0, 0);

  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(button_b, _Globals.windowWidth - 70, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}

void gameSelectDispose()
{
  gs_freeIcon();
  free(gamesItems);
  gamesItems = NULL;
  gamesItemsCount = -1;
}

void drawGameSelectMenu()
{
  // The saves page needs the game opened by gs_openGame
  if (slotsPage && runningGame == NULL)
    slotsPage = 0;
  if (slotsPage) {
    gs_drawSlotsPage();
    return;
  }

  gs_initGamesMenu();

  switch (gme_handleInput(&gamesMenu)) {
    case gme_event_moved:
      // The image of the new selected game is loaded when drawn
      gs_freeIcon();
      break;
    case gme_event_selected: {
      gs_openGame();

      // If the game has saves, choose between a new game and a save
      int usedSlots = 0;
      for (int i = 0; i < SAVE_SLOTS; i++)
        usedSlots += pg_getSaveInfo(i, &slots[i]);
      if (usedSlots > 0) {
        gs_initSlotsMenu();
        slotsPage = 1;
        messageTime = 0;
        return;
      }

      _Globals.status = Game;
      return;
    }
    case gme_event_back:
      gs_freeIcon();
      _Globals.status = MainMenu;
      return;
    default:
      break;
  }

  gme_draw(&gamesMenu);

  gme_item *selected = gme_selectedItem(&gamesMenu);
  if (selected != NULL) {
    game *selGame = &_Globals.games[selected->id];
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

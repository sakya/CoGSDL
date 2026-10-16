#include "CoGSDL.h"
#include "gui_playgame.h"
#include "gui_generic_menu.h"

enum mm_item_id
{
  mm_continue = 0,
  mm_games,
  mm_settings,
  mm_about,
  mm_quit
};

static double about_angle = 0;
/// @brief -1: to check, 0: no saves, 1: "Continue" can be selected
static int mm_canContinue = -1;
/// @brief The game of the most recent save (loaded by "Continue")
static int mm_continueGame = -1;

static gme_item mm_items[] = {
  { .id = mm_continue, .text = "Continue" },
  { .id = mm_games, .text = "Games" },
  { .id = mm_settings, .text = "Settings" },
  { .id = mm_about, .text = "About" },
  // "Quit" is separated from the other items
  { .id = mm_quit, .text = "Quit", .spaceBefore = 15 }
};
static gme_menu mm_menu;
static int mm_menuInitialized = 0;

void drawMainMenu()
{
  if (!mm_menuInitialized) {
    gme_init(&mm_menu, mm_items, sizeof(mm_items) / sizeof(mm_items[0]));
    mm_menu.y = 170;
    mm_menu.height = _Globals.windowHeight - HEADER_HEIGHT - mm_menu.y - 10;
    mm_menuInitialized = 1;
  }

  // "Continue" can be selected only if there is a save (checked when the menu is shown)
  if (mm_canContinue < 0) {
    int gameIndex, slot;
    mm_canContinue = sv_findLastSave(&gameIndex, &slot);
    mm_continueGame = mm_canContinue ? gameIndex : -1;

    gme_item *item = &mm_items[mm_continue];
    item->disabled = !mm_canContinue;
    // The game to load is shown below "Continue"
    item->subText = NULL;
    if (mm_canContinue && mm_continueGame >= 0 && mm_continueGame < _Globals.gamesCount)
      item->subText = _Globals.games[mm_continueGame].title;
    // Moves the selection to "Games" if "Continue" has been disabled
    gme_select(&mm_menu, mm_menu.selectedIdx);
  }

  if (gme_handleInput(&mm_menu) == gme_event_selected) {
    // The saves are checked again when the menu is shown again
    mm_canContinue = -1;
    switch (gme_selectedItem(&mm_menu)->id) {
      case mm_continue:
        // Load the most recent save (of any game)
        if (sv_continueLastSave())
          _Globals.status = Game;
        break;
      case mm_games:
        _Globals.status = GameSelect;
        break;
      case mm_settings:
        _Globals.status = Settings;
        break;
      case mm_about:
        _Globals.status = About;
        break;
      case mm_quit:
        _Globals.quit = 1;
        break;
    }
  }

  SDL_Texture* texture = sdlGetImageTexture(app_icon, 100, 100);
  SDL_Rect rect = {30, 50, 100, 100};
  SDL_Point center = { 50, 50};
  SDL_RendererFlip flip = SDL_FLIP_NONE;
  SDL_RenderCopyEx(_Globals.renderer, texture, NULL, &rect, about_angle, &center, flip);

  about_angle += 0.5;
  if (about_angle >= 360)
    about_angle = 0;

  sdlWriteText(_Globals.defaultFont, _theme->textColor, "CoGSDL", 140, 75, 0, 0);
  sdlWriteText(_Globals.defaultSmallFont, _theme->textColor, "A ChoiceScript interpreter", 140, 100, 0, 0);

  gme_draw(&mm_menu);

  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}

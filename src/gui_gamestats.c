#include "CoGSDL.h"
#include "gui_playgame.h"

/// @brief Status set by B (the screen that opened the stats)
static int gs_returnStatus = Game;

void gameStatsOpen(int returnStatus)
{
  gs_returnStatus = returnStatus;
  _Globals.status = GameStats;
}

/// @brief Close the stats screen and go to the given status
static void gs_close(enum status status)
{
  pg_statsClose();
  _Globals.status = status;
  gs_returnStatus = Game;
}

void drawGameStats()
{
  if (_Globals.input[_Keys.KeyL1] == 1) {
    _Globals.input[_Keys.KeyL1] = 0;
    gs_close(Game);
    return;
  }

  if (_Globals.input[_Keys.KeyR1] == 1) {
    _Globals.input[_Keys.KeyR1] = 0;
    gs_close(GameMenu);
    return;
  }

  if (_Globals.input[_Keys.KeyB] == 1) {
    _Globals.input[_Keys.KeyB] = 0;
    playMenuBackSound();
    gs_close(gs_returnStatus);
    return;
  }

  // The stats screen executes scenes/choicescript_stats.txt
  if (!pg_isStatsActive() && !pg_statsOpen()) {
    sdlWriteText(_Globals.defaultFont, _theme->textColor, "No stats available for this game", TEXT_MARGIN, HEADER_HEIGHT + TEXT_MARGIN, 0, 0);
  } else {
    pg_drawPage();
  }

  // Back button
  sdlBlitImage(button_b, 40, _Globals.windowHeight - 30, 30, 30);
  sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, "Back", 72, _Globals.windowHeight - 28, 0, 0);
}

#include "CoGSDL.h"

void drawGameMenu()
{
  if (_Globals.input[_Keys.KeyL1] == 1) {
    _Globals.input[_Keys.KeyL1] = 0;
    _Globals.status = GameStats;
    return;
  }

  if (_Globals.input[_Keys.KeyR1] == 1) {
    _Globals.input[_Keys.KeyR1] = 0;
    _Globals.status = Game;
    return;
  }

  sdlWriteText(_Globals.defaultFont, _theme->textColor, "Menu", 30, 40, 0, 0);
}
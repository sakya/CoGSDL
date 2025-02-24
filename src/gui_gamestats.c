#include "CoGSDL.h"

void drawGameStats()
{
  if (_Globals.input[_Keys.KeyL1] == 1) {
    _Globals.input[_Keys.KeyL1] = 0;
    _Globals.status = Game;
    return;
  }

  if (_Globals.input[_Keys.KeyR1] == 1) {
    _Globals.input[_Keys.KeyR1] = 0;
    _Globals.status = GameMenu;
    return;
  }

  sdlWriteText(_Globals.defaultFont, _theme->textColor, "Stats", 30, 40, 0, 0);
}
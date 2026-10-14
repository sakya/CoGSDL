#include "CoGSDL.h"

void drawSettings()
{
  if (_Globals.input[_Keys.KeyB] == 1) {
      _Globals.status = MainMenu;
      return;
  }
}
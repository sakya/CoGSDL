#include "CoGSDL.h"

void gu_drawFooter(int showPad, const char* messageA, const char* messageB)
{
  if (showPad) {
    sdlBlitImage(dpad_up_down, 5, FOOTER_BUTTON_Y, 30, 30);
  }

  const int centerYText = sdlTextCenterY(_Globals.defaultFont, _Globals.windowHeight - HEADER_HEIGHT, HEADER_HEIGHT);
  if (messageB != NULL) {
    if (showPad) {
      sdlBlitImage(button_b, 40, FOOTER_BUTTON_Y, 30, 30);
    } else {
      sdlBlitImage(button_b, 5, FOOTER_BUTTON_Y, 30, 30);
    }
    sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, (char *)messageB, showPad ? 72 : 42, centerYText, 0, 0);
  }

  if (messageA != NULL) {
    sdlBlitImage(button_a, _Globals.windowWidth - 35, FOOTER_BUTTON_Y, 30, 30);
    int width;
    TTF_MeasureUTF8(_Globals.defaultFont, messageA, _Globals.windowWidth, &width, NULL);
    sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, (char *)messageA, _Globals.windowWidth - 35 - width - 5, centerYText, 0, 0);
  }
}
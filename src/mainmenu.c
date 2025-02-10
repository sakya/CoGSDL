#include "CoGSDL.h"

double mmAngle = 0;
int mmIndex = 0;

void drawMainMenu()
{
  if (_Globals.input[_Keys.KeyDown] == 1) {
    if (mmIndex < 2) {
      mmIndex++;
      Mix_PlayChannel(-1, sound_menu_change, 0);
    }
    _Globals.input[_Keys.KeyDown] = 0;
  } else if (_Globals.input[_Keys.KeyUp] == 1) {
    if (mmIndex > 0) {
      mmIndex--;
      Mix_PlayChannel(-1, sound_menu_change, 0);
    }
    _Globals.input[_Keys.KeyUp] = 0;
  } else if (_Globals.input[_Keys.KeyA] == 1) {
    switch (mmIndex) {
      case 0:
        break;
      case 1:
        _Globals.status = GameSelect;
        break;
      case 2:
        _Globals.quit = 1;
        break;
    }
    _Globals.input[_Keys.KeyA] = 0;
  }

  SDL_Texture* texture = SDL_CreateTextureFromSurface(_Globals.renderer, app_icon);
  SDL_Rect rect = {30, 50, 100, 100};
  SDL_Rect crop = {0, 0, app_icon->w, app_icon->h};
  SDL_Point center = { 50, 50};
  SDL_RendererFlip flip = SDL_FLIP_NONE;
  SDL_RenderCopyEx(_Globals.renderer, texture, &crop , &rect, mmAngle, &center, flip);
  SDL_DestroyTexture(texture);

  mmAngle += 0.5;
  if (mmAngle >= 360)
    mmAngle = 0;

  sdlWriteText(_Globals.defaultFont, colorText, "CoGSDL", 140, 75, 0, 0);
  sdlWriteText(_Globals.defaultSmallFont, colorText, "A Choice of Games interpreter", 140, 100, 0, 0);

  sdlWriteText(_Globals.defaultFont, mmIndex == 0 ? colorSelectedText : colorText, "Continue", 30, 200, 0, 0);
  sdlWriteText(_Globals.defaultFont, mmIndex == 1 ? colorSelectedText : colorText, "Games", 30, 230, 0, 0);
  sdlWriteText(_Globals.defaultFont, mmIndex == 2 ? colorSelectedText : colorText, "Quit", 30, 270, 0, 0);

  sdlBlitImage(button_a, _Globals.WINDOW_WIDTH - 35, _Globals.WINDOW_HEIGHT - 30, 30, 30);
  sdlBlitImage(dpad_up_down, _Globals.WINDOW_WIDTH - 70, _Globals.WINDOW_HEIGHT - 30, 30, 30);
}
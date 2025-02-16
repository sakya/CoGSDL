#include "CoGSDL.h"

static double mmAngle = 0;
static int mmIndex = 0;

void drawMainMenu()
{
  if (_Globals.input[_Keys.KeyDown] == 1) {
    if (mmIndex < 3) {
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
        _Globals.status = About;
        break;
      case 3:
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

  sdlWriteText(_Globals.defaultFont, _theme->textColor, "CoGSDL", 140, 75, 0, 0);
  sdlWriteText(_Globals.defaultSmallFont, _theme->textColor, "A Choice of Games interpreter", 140, 100, 0, 0);

  sdlWriteText(_Globals.defaultFont, mmIndex == 0 ? _theme->selectedTextColor : _theme->textColor, "Continue", 30, 200, 0, 0);
  sdlWriteText(_Globals.defaultFont, mmIndex == 1 ? _theme->selectedTextColor : _theme->textColor, "Games", 30, 230, 0, 0);
  sdlWriteText(_Globals.defaultFont, mmIndex == 2 ? _theme->selectedTextColor : _theme->textColor, "About", 30, 260, 0, 0);
  sdlWriteText(_Globals.defaultFont, mmIndex == 3 ? _theme->selectedTextColor : _theme->textColor, "Quit", 30, 300, 0, 0);

  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}
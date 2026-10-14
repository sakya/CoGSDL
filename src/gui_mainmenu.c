#include "CoGSDL.h"

static double about_angle = 0;
static int mm_index = 0;

void drawMainMenu()
{
  if (_Globals.input[_Keys.KeyDown] == 1) {
    if (mm_index < 4) {
      mm_index++;
      Mix_PlayChannel(-1, sound_menu_change, 0);
    }
    _Globals.input[_Keys.KeyDown] = 0;
  } else if (_Globals.input[_Keys.KeyUp] == 1) {
    if (mm_index > 0) {
      mm_index--;
      Mix_PlayChannel(-1, sound_menu_change, 0);
    }
    _Globals.input[_Keys.KeyUp] = 0;
  } else if (_Globals.input[_Keys.KeyA] == 1) {
    switch (mm_index) {
      case 0:
        break;
      case 1:
        _Globals.status = GameSelect;
        break;
      case 2:
        _Globals.status = Settings;
        break;
      case 3:
        _Globals.status = About;
        break;
      case 4:
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
  SDL_RenderCopyEx(_Globals.renderer, texture, &crop , &rect, about_angle, &center, flip);
  SDL_DestroyTexture(texture);

  about_angle += 0.5;
  if (about_angle >= 360)
    about_angle = 0;

  sdlWriteText(_Globals.defaultFont, _theme->textColor, "CoGSDL", 140, 75, 0, 0);
  sdlWriteText(_Globals.defaultSmallFont, _theme->textColor, "A ChoiceScript interpreter", 140, 100, 0, 0);

  const char *items[] = { "Continue", "Games", "Settings", "About", "Quit" };
  const int itemsY[] = { 200, 230, 260, 290, 340 };
  for (int i = 0; i < 5; i++) {
    if (i == mm_index)
      sdlDrawSelection(itemsY[i] - 2, sdlSelectionHeight());
    sdlWriteText(_Globals.defaultFont, i == mm_index ? _theme->titleTextColor : _theme->textColor, (char *)items[i], 30, itemsY[i], 0, 0);
  }

  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}
#include "CoGSDL.h"

static double about_angle = 0;

void drawAbout()
{
  if (_Globals.input[_Keys.KeyB] == 1) {
      _Globals.status = MainMenu;
      return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(_Globals.renderer, app_icon);
  SDL_Rect rect = {30, 50, 100, 100};
  SDL_Rect crop = {0, 0, app_icon->w, app_icon->h};
  SDL_Point center = {50, 50};
  SDL_RendererFlip flip = SDL_FLIP_NONE;
  SDL_RenderCopyEx(_Globals.renderer, texture, &crop, &rect, about_angle, &center, flip);
  SDL_DestroyTexture(texture);

  about_angle += 0.5;
  if (about_angle >= 360)
    about_angle = 0;

  sdlWriteText(_Globals.defaultFont, _theme->textColor, "CoGSDL", 140, 75, 0, 0);
  sdlWriteText(_Globals.defaultSmallFont, _theme->textColor, "A Choice of Games interpreter", 140, 100, 0, 0);
}
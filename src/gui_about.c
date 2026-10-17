#include "CoGSDL.h"

static double about_angle = 0;

void drawAbout()
{
  if (_Globals.input[_Keys.KeyB] == 1) {
      _Globals.input[_Keys.KeyB] = 0;
      playMenuBackSound();
      _Globals.status = MainMenu;
      return;
  }

  SDL_Texture *texture = sdlGetImageTexture(app_icon, 100, 100);
  SDL_Rect rect = {30, 50, 100, 100};
  SDL_Point center = {50, 50};
  SDL_RendererFlip flip = SDL_FLIP_NONE;
  SDL_RenderCopyEx(_Globals.renderer, texture, NULL, &rect, about_angle, &center, flip);

  about_angle += 0.5;
  if (about_angle >= 360)
    about_angle = 0;

  sdlWriteText(_Globals.defaultFont, _theme->textColor, "CoGSDL v" VERSION, 140, 75, 0, 0);
  sdlWriteText(_Globals.defaultSmallFont, _theme->textColor, "A ChoiceScript interpreter", 140, 100, 0, 0);

  int y = 200;
  sdlWriteText(_Globals.defaultFont, _theme->textColor, "© 2026 Paolo Iommarini - MIT License", 30, y, 0, 0);
  y += _Globals.defaultFontHeight;
  sdlWriteText(_Globals.defaultFont, _theme->textColor, "https://github.com/sakya/CoGSDL", 30, y, 0, 0);

  y += _Globals.defaultFontHeight + 15;
  sdlWriteText(_Globals.defaultFont, _theme->textColor, "Unofficial ChoiceScript interpreter, built with SDL2.", 30, y, 0, 0);
  y += _Globals.defaultFontHeight;
  sdlWriteText(_Globals.defaultFont, _theme->textColor, "Not affiliated with Choice of Games LLC.", 30, y, 0, 0);

  sdlBlitImage(button_b, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
}
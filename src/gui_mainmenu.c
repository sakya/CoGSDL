#include "CoGSDL.h"
#include "gui_playgame.h"

static double about_angle = 0;
static int mm_index = 0;
/// @brief -1: to check, 0: no saves, 1: "Continue" can be selected
static int mm_canContinue = -1;

void drawMainMenu()
{
  // "Continue" can be selected only if there is a save (checked when the menu is shown)
  if (mm_canContinue < 0) {
    int gameIndex, slot;
    mm_canContinue = pg_findLastSave(&gameIndex, &slot);
  }
  const int firstIndex = mm_canContinue ? 0 : 1;
  if (mm_index < firstIndex)
    mm_index = firstIndex;

  if (_Globals.input[_Keys.KeyDown] == 1) {
    if (mm_index < 4) {
      mm_index++;
      Mix_PlayChannel(-1, sound_menu_change, 0);
    }
    _Globals.input[_Keys.KeyDown] = 0;
  } else if (_Globals.input[_Keys.KeyUp] == 1) {
    if (mm_index > firstIndex) {
      mm_index--;
      Mix_PlayChannel(-1, sound_menu_change, 0);
    }
    _Globals.input[_Keys.KeyUp] = 0;
  } else if (_Globals.input[_Keys.KeyA] == 1) {
    _Globals.input[_Keys.KeyA] = 0;
    // The saves are checked again when the menu is shown again
    mm_canContinue = -1;
    switch (mm_index) {
      case 0:
        // Load the most recent save (of any game)
        if (pg_continueLastSave())
          _Globals.status = Game;
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

  SDL_Texture* texture = sdlGetImageTexture(app_icon, 100, 100);
  SDL_Rect rect = {30, 50, 100, 100};
  SDL_Point center = { 50, 50};
  SDL_RendererFlip flip = SDL_FLIP_NONE;
  SDL_RenderCopyEx(_Globals.renderer, texture, NULL, &rect, about_angle, &center, flip);

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
    SDL_Color color = i == mm_index ? _theme->titleTextColor : _theme->textColor;
    if (i == 0 && !mm_canContinue)
      color = _theme->disabledTextColor;
    sdlWriteText(_Globals.defaultFont, color, (char *)items[i], 30, itemsY[i], 0, 0);
  }

  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}
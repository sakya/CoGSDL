#include "CoGSDL.h"
#include "gui_playgame.h"

extern game_status pg_game_status;
static int firstIdx = 0;
static int selectedIdx = 0;

void gc_draw();
void pg_renderTextToSurface(SDL_Surface *surface, const char* text, int scrollY);

void drawGameChoice()
{
  if (_Globals.input[_Keys.KeyDown] == 1 && selectedIdx >= 0) {
    _Globals.input[_Keys.KeyDown] = 0;
    selectedIdx++;
    if (selectedIdx == pg_game_status.choicesCount) {
      selectedIdx = 0;
      firstIdx = 0;
    }
    //if (selectedIdx - firstIdx + 1 > items)
    //  firstIdx++;
  } else if (_Globals.input[_Keys.KeyUp] == 1 && selectedIdx >= 0) {
    _Globals.input[_Keys.KeyUp] = 0;
    selectedIdx--;
    if (selectedIdx < 0) {
      selectedIdx = pg_game_status.choicesCount - 1;
      //firstIdx = pg_game_status.choicesCount - items;
      if (firstIdx < 0)
        firstIdx = 0;
    }
    if (selectedIdx < firstIdx)
      firstIdx = selectedIdx;
  } else if (_Globals.input[_Keys.KeyB] == 1) {
    _Globals.input[_Keys.KeyB] = 0;
    _Globals.status = Game;
    return;
  }

  gc_draw();
}

void gc_draw()
{
  int i = 0;
  int y = 40;
  choice* selChoice = &pg_game_status.choices[selectedIdx];
  for (i = firstIdx; i < pg_game_status.choicesCount; i++) {
    choice* c = &pg_game_status.choices[i];
    SDL_Color color = i == selectedIdx ? _theme->selectedTextColor : _theme->textColor;
    if (c != NULL) {
      sdlWriteText(_Globals.defaultFont, color, c->text, 10, y, 0, 0);

      y += 25;
      if (y >= _Globals.windowHeight - HEADER_HEIGHT)
        break;
    }
  }

  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);

  char select_msg[] = "Select";
  int width;
  TTF_MeasureUTF8(_Globals.defaultFont, select_msg, _Globals.windowWidth, &width, NULL);
  sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, select_msg, _Globals.windowWidth - 35 - width - 5, _Globals.windowHeight - 28, 0, 0);
}
#include "CoGSDL.h"
#include "gui_playgame.h"

extern game_status pg_game_status;
static int firstIdx = 0;
static int selectedIdx = 0;
static SDL_Surface* choice_surface[16];
static int choice_surface_count = 0;
static int line_height = 0;

void gc_init();
void gc_dispose();
void gc_draw();
void pg_renderTextToSurface(SDL_Surface *surface, const char* text, int scrollY);
char* pg_getNextLine(const char *text, int* startPosition, int maxWidth, int *wordWrap, int *last_x);

void drawGameChoice()
{
  const int old_selected_idx = selectedIdx;
  if (_Globals.input[_Keys.KeyDown] == 1 && selectedIdx >= 0) {
    _Globals.input[_Keys.KeyDown] = 0;
    selectedIdx++;
    if (selectedIdx == pg_game_status.choicesCount) {
      selectedIdx = 0;
      firstIdx = 0;
    }
  } else if (_Globals.input[_Keys.KeyUp] == 1 && selectedIdx >= 0) {
    _Globals.input[_Keys.KeyUp] = 0;
    selectedIdx--;
    if (selectedIdx < 0) {
      selectedIdx = pg_game_status.choicesCount - 1;
      if (firstIdx < 0)
        firstIdx = 0;
    }
    if (selectedIdx < firstIdx)
      firstIdx = selectedIdx;
  } else if (_Globals.input[_Keys.KeyA] == 1) {
    _Globals.input[_Keys.KeyA] = 0;
    if (selectedIdx >= 0) {
      choice* c = &pg_game_status.choices[selectedIdx];
      pg_game_status.selectedChoiceIndex = selectedIdx;
      logDebug("Selected choice: %s (%i)", c->text, c->position);
      c->used = 1;
      _Globals.status = Game;
      gc_dispose();
      return;
    }
  } else if (_Globals.input[_Keys.KeyB] == 1) {
    _Globals.input[_Keys.KeyB] = 0;
    _Globals.status = Game;
    gc_dispose();
    return;
  }

  if (old_selected_idx != selectedIdx) {
    Mix_PlayChannel(-1, sound_menu_change, 0);
  }

  if (!choice_surface_count) {
    gc_init();
  }
  gc_draw();
}

void gc_init()
{
  selectedIdx = 0;
  firstIdx = 0;

  SDL_Surface *surface = TTF_RenderUTF8_Blended(_Globals.textFont, "H", _theme->textColor);
  line_height = surface->h;
  SDL_FreeSurface(surface);

  pg_game_status.selectedChoiceIndex = -1;
  if (!choice_surface_count) {
    int text_width = _Globals.windowWidth - 2 * TEXT_MARGIN;

    for (int i = 0; i < pg_game_status.choicesCount; i++) {
      choice* c = &pg_game_status.choices[i];
      int lines = 0;
      int linePos = 0;
      char *line = NULL;
      while ((line = pg_getNextLine(c->text, &linePos, text_width, NULL, NULL)) != NULL) {
        lines++;
        free(line);
      }
      SDL_Surface  *surface = SDL_CreateRGBSurface(0, text_width, lines * line_height + 2 * TEXT_MARGIN, 32, 0, 0, 0, 0);
      if (surface == NULL) {
        logWarning("Failed to create surface");
        return;
      }
      SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, _theme->backgroundColor.r, _theme->backgroundColor.g, _theme->backgroundColor.b, 0));
      pg_renderTextToSurface(surface, c->text, 0);

      choice_surface[choice_surface_count] = surface;
      choice_surface_count++;
    }
  }
}

void gc_dispose()
{
  for (int i = 0; i < choice_surface_count; i++) {
    SDL_FreeSurface(choice_surface[i]);
  }
  choice_surface_count = 0;
}

void gc_draw()
{
  int i = 0;
  int y = HEADER_HEIGHT + TEXT_MARGIN;
  for (i = firstIdx; i < pg_game_status.choicesCount; i++) {
    choice* c = &pg_game_status.choices[i];
    if (!c->reusable && c->used)
      continue;

    int cropped = 0;
    int height = choice_surface[i]->h + 2;
    if (y + height > _Globals.windowHeight - HEADER_HEIGHT) {
      height = _Globals.windowHeight - HEADER_HEIGHT - y - TEXT_MARGIN;
      cropped = 1;
    }

    if (i == selectedIdx) {
      SDL_SetRenderDrawColor(_Globals.renderer, _theme->headerBackgroundColor.r, _theme->headerBackgroundColor.g, _theme->headerBackgroundColor.b, 255);

      SDL_Rect rect = { 0, y, _Globals.windowWidth, height };
      SDL_RenderFillRect(_Globals.renderer, &rect);

      if (cropped) {
        // Scroll
        firstIdx++;
      }
    }
    y += 1;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(_Globals.renderer, choice_surface[i]);
    SDL_Rect crop = { 0, 0, choice_surface[i]->w, height - 2};
    SDL_Rect rect = { TEXT_MARGIN, y, choice_surface[i]->w, height - 2 };
    SDL_RenderCopyEx(_Globals.renderer, texture, &crop , &rect, 0, NULL, SDL_FLIP_NONE);
    SDL_DestroyTexture(texture);
    y += choice_surface[i]->h + 1;
    if (y >= _Globals.windowHeight - HEADER_HEIGHT)
      break;
  }

  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);

  char select_msg[] = "Select";
  int width;
  TTF_MeasureUTF8(_Globals.defaultFont, select_msg, _Globals.windowWidth, &width, NULL);
  sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, select_msg, _Globals.windowWidth - 35 - width - 5, _Globals.windowHeight - 28, 0, 0);
}
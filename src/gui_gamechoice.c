#include "CoGSDL.h"
#include "gui_playgame.h"

extern game_status pg_game_status;
static int scroll_y = 0;
static int selectedIdx = 0;
static SDL_Surface* choice_surface[16];
static int choice_surface_count = 0;
static int line_height = 0;
static int total_height = 0;

void gc_init();
void gc_dispose();
void gc_draw();
void gc_select_next();
void gc_select_previous();

void pg_renderTextToSurface(SDL_Surface *surface, const char* text, int scrollY);
char* pg_getNextLine(const char *text, int* startPosition, int maxWidth, int *wordWrap, int *last_x);

void drawGameChoice()
{
  const int old_selected_idx = selectedIdx;
  if (_Globals.input[_Keys.KeyDown] == 1 && selectedIdx >= 0) {
    _Globals.input[_Keys.KeyDown] = 0;
    gc_select_next();
  } else if (_Globals.input[_Keys.KeyUp] == 1 && selectedIdx >= 0) {
    _Globals.input[_Keys.KeyUp] = 0;
    gc_select_previous();
  } else if (_Globals.input[_Keys.KeyA] == 1) {
    _Globals.input[_Keys.KeyA] = 0;
    if (selectedIdx >= 0) {
      choice* c = &pg_game_status.choices[selectedIdx];
      pg_game_status.selectedChoiceIndex = selectedIdx;
      logInfo("Selected choice: %s (%i)", c->text, c->position);
      c->used = 1;
      strcpy(pg_game_status.used_choices[pg_game_status.used_choices_count], c->id);
      pg_game_status.used_choices_count++;

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
  scroll_y = 0;

  line_height = get_line_height(_Globals.textFont);
  pg_game_status.selectedChoiceIndex = -1;
  total_height = 0;
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
      if (c->reusable || !c->used) {
        total_height += surface->h + 2;
      }
    }
  }
}

void gc_dispose()
{
  for (int i = 0; i < choice_surface_count; i++) {
    SDL_FreeSurface(choice_surface[i]);
  }
  choice_surface_count = 0;
  total_height = 0;
}

void gc_draw()
{
  int i = 0;
  int y = HEADER_HEIGHT + TEXT_MARGIN;
  for (i = 0; i < pg_game_status.choicesCount; i++) {
    choice* c = &pg_game_status.choices[i];
    int height = choice_surface[i]->h + 2;

    if (!c->reusable && c->used) {
      if (selectedIdx == i) {
        gc_select_next();
      }
      continue;
    }
    if (y - scroll_y + height < HEADER_HEIGHT + TEXT_MARGIN || y - scroll_y > _Globals.windowHeight - HEADER_HEIGHT) {
      y += height;
      continue;
    }

    int cropped = 0;
    int delta_y = 0;
    if (y - scroll_y < HEADER_HEIGHT + TEXT_MARGIN) {
      height -=  HEADER_HEIGHT + TEXT_MARGIN - (y - scroll_y);
      delta_y = HEADER_HEIGHT + TEXT_MARGIN - (y - scroll_y);
      cropped = 1;
    } else if (y - scroll_y + height > _Globals.windowHeight - HEADER_HEIGHT) {
      height = _Globals.windowHeight - HEADER_HEIGHT - y - TEXT_MARGIN;
      delta_y = 0;
      cropped = 1;
    }

    if (i == selectedIdx) {
      if (cropped) {
        // Scroll
        if (delta_y) {
          scroll_y -= delta_y;
        } else {
          scroll_y += choice_surface[i]->h + 2 - height;
        }
      }

      SDL_SetRenderDrawColor(_Globals.renderer, _theme->headerBackgroundColor.r, _theme->headerBackgroundColor.g, _theme->headerBackgroundColor.b, 255);
      SDL_Rect rect = { 0, y - scroll_y + delta_y, _Globals.windowWidth, height };
      SDL_RenderFillRect(_Globals.renderer, &rect);
    }
    y += 1;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(_Globals.renderer, choice_surface[i]);
    SDL_Rect crop = { 0, delta_y, choice_surface[i]->w, height - 2};
    SDL_Rect rect = { TEXT_MARGIN, y - scroll_y + delta_y, choice_surface[i]->w, height - 2 };
    SDL_RenderCopyEx(_Globals.renderer, texture, &crop , &rect, 0, NULL, SDL_FLIP_NONE);
    SDL_DestroyTexture(texture);
    y += choice_surface[i]->h + 1;
    if (y - scroll_y >= _Globals.windowHeight - HEADER_HEIGHT)
      break;
  }

  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);

  char select_msg[] = "Select";
  int width;
  TTF_MeasureUTF8(_Globals.defaultFont, select_msg, _Globals.windowWidth, &width, NULL);
  sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, select_msg, _Globals.windowWidth - 35 - width - 5, _Globals.windowHeight - 28, 0, 0);
}

void gc_select_next()
{
  selectedIdx++;
  if (selectedIdx == pg_game_status.choicesCount) {
    selectedIdx = 0;
  }

  while (!pg_game_status.choices[selectedIdx].reusable && pg_game_status.choices[selectedIdx].used) {
    selectedIdx++;
    if (selectedIdx == pg_game_status.choicesCount) {
      selectedIdx = 0;
    }
  }

  if (selectedIdx == 0) {
    scroll_y = 0;
  }
}

void gc_select_previous()
{
  selectedIdx--;
  if (selectedIdx < 0) {
    selectedIdx = pg_game_status.choicesCount - 1;
  }

  while (!pg_game_status.choices[selectedIdx].reusable && pg_game_status.choices[selectedIdx].used) {
    selectedIdx--;
    if (selectedIdx < 0) {
      selectedIdx = pg_game_status.choicesCount - 1;
    }
  }

  if (selectedIdx == pg_game_status.choicesCount - 1) {
    if (total_height > _Globals.windowHeight - 2 * HEADER_HEIGHT - 2 * TEXT_MARGIN) {
      scroll_y = total_height - (_Globals.windowHeight - 2 * HEADER_HEIGHT - 2 * TEXT_MARGIN) - choice_surface[selectedIdx]->h - 2;
    }
  }
}
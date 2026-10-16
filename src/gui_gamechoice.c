#include "CoGSDL.h"
#include "gui_playgame.h"
#include "gui_generic_menu.h"

extern game_status pg_game_status;

/// @brief Menu of the visible choices: the id of an item is the index of the choice
static gme_menu gc_menu;
static gme_item *gc_items = NULL;
static int gc_itemsCount = 0;
static int gc_initialized = 0;

void gc_init();
void gc_dispose();

/// @brief Check if a choice can be selected
/// Choices that cannot be selected (*selectable_if false, used with *hide_reuse or *disable_reuse) are hidden
static int gc_is_enabled(const choice *c)
{
  if (!c->selectable)
    return 0;
  if (c->used && (!c->reusable || c->disable_reuse))
    return 0;
  return 1;
}

/// @brief Check if a choice is visible (only selectable choices are shown)
static int gc_is_visible(const choice *c)
{
  return gc_is_enabled(c);
}

void pg_renderTextToSurface(SDL_Surface *surface, const char* text, int scrollY);
char* pg_getNextLine(const char *text, int* startPosition, int maxWidth, int *wordWrap, int *last_x);

/// @brief Width of the surfaces of the choices: the menu area minus the margins of the menu
static int gc_surfaceWidth()
{
  return _Globals.windowWidth - 2 * (gc_menu.margin + gc_menu.textIndent);
}

/// @brief Render the text of a choice on a transparent surface, with the game text font
/// (more lines, bold/italic): the surface includes a margin of TEXT_MARGIN pixels
static SDL_Surface *gc_renderChoice(const choice *c, int height, SDL_Color color)
{
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, gc_surfaceWidth(), height, 32, SDL_PIXELFORMAT_ARGB8888);
  if (surface == NULL)
    return NULL;
  SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, color.r, color.g, color.b, 0));

  // pg_renderTextToSurface uses the text color of the theme
  theme textTheme = *_theme;
  textTheme.textColor = color;
  const theme *savedTheme = _theme;
  _theme = &textTheme;
  pg_renderTextToSurface(surface, c->text, 0);
  _theme = savedTheme;
  return surface;
}

/// @brief Render the selected choice in titleTextColor (when it is selected the first time)
static void gc_renderSelected()
{
  gme_item *item = gme_selectedItem(&gc_menu);
  if (item == NULL || item->selectedSurface != NULL || item->surface == NULL)
    return;
  item->selectedSurface = gc_renderChoice(&pg_game_status.choices[item->id], item->surface->h, _theme->titleTextColor);
}

void drawGameChoice()
{
  if (!gc_initialized)
    gc_init();

  switch (gme_handleInput(&gc_menu)) {
    case gme_event_selected: {
      int index = gme_selectedItem(&gc_menu)->id;
      choice* c = &pg_game_status.choices[index];
      pg_game_status.selectedChoiceIndex = index;
      logInfo("Selected choice: %s (%i)", c->text, c->position);
      c->used = 1;
      int alreadyUsed = 0;
      for (int i = 0; i < pg_game_status.used_choices_count; i++) {
        if (strcmp(pg_game_status.used_choices[i], c->id) == 0) {
          alreadyUsed = 1;
          break;
        }
      }
      if (!alreadyUsed && pg_game_status.used_choices_count < (int)(sizeof(pg_game_status.used_choices) / sizeof(pg_game_status.used_choices[0]))) {
        strcpy(pg_game_status.used_choices[pg_game_status.used_choices_count], c->id);
        pg_game_status.used_choices_count++;
      }

      _Globals.status = pg_isStatsActive() ? GameStats : Game;
      gc_dispose();
      return;
    }
    case gme_event_back:
      _Globals.status = pg_isStatsActive() ? GameStats : Game;
      gc_dispose();
      return;
    default:
      break;
  }

  gc_renderSelected();
  gme_draw(&gc_menu);

  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(button_b, _Globals.windowWidth - 70, _Globals.windowHeight - 30, 30, 30);
}

void gc_init()
{
  pg_game_status.selectedChoiceIndex = -1;
  gc_dispose();
  if (pg_game_status.choicesCount > 0)
    gc_items = calloc(pg_game_status.choicesCount, sizeof(gme_item));

  // Layout of the menu (used by gc_surfaceWidth): the margins of the text are in the surfaces
  gme_init(&gc_menu, gc_items, 0);
  gc_menu.y = HEADER_HEIGHT + TEXT_MARGIN;
  gc_menu.height = _Globals.windowHeight - 2 * HEADER_HEIGHT - 2 * TEXT_MARGIN;
  gc_menu.textIndent = 0;
  // From the last choice go back to the first one (and vice versa)
  gc_menu.wrap = 1;
  // Scroll by pixels: the choices at the bottom are shown even if they fit only in part
  gc_menu.partialItems = 1;

  const int lineHeight = get_line_height(_Globals.textFont);
  const int textWidth = gc_surfaceWidth() - 2 * TEXT_MARGIN;
  for (int i = 0; gc_items != NULL && i < pg_game_status.choicesCount; i++) {
    choice* c = &pg_game_status.choices[i];
    // Only the choices that can be selected are shown
    if (!gc_is_visible(c))
      continue;

    int lines = 0;
    int linePos = 0;
    char *line = NULL;
    while ((line = pg_getNextLine(c->text, &linePos, textWidth, NULL, NULL)) != NULL) {
      lines++;
      free(line);
    }
    SDL_Surface *surface = gc_renderChoice(c, lines * lineHeight + 2 * TEXT_MARGIN, _theme->textColor);
    if (surface == NULL) {
      logWarning("Failed to create surface");
      continue;
    }

    gme_item *item = &gc_items[gc_itemsCount++];
    item->id = i;
    item->surface = surface;
  }

  gc_menu.itemsCount = gc_itemsCount;
  // Select the first choice
  gme_select(&gc_menu, 0);
  gc_initialized = 1;
}

void gc_dispose()
{
  for (int i = 0; i < gc_itemsCount; i++) {
    sdlFreeSurface(gc_items[i].surface);
    sdlFreeSurface(gc_items[i].selectedSurface);
  }
  free(gc_items);
  gc_items = NULL;
  gc_itemsCount = 0;
  gc_menu.itemsCount = 0;
  gc_initialized = 0;
}

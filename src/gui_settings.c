#include "CoGSDL.h"
#include "gui_generic_menu.h"
#include "gui_playgame.h"

#define SETTINGS_FILE "./settings.cfg"

typedef struct {
  const char *name;
  const theme *theme;
} theme_entry;

static const theme_entry themes[] = {
  { "Sepia", &sepiaTheme },
  { "Light", &lightTheme },
  { "Forest", &forestTheme }
};
static const int themesCount = sizeof(themes) / sizeof(themes[0]);

enum settings_row
{
  st_theme = 0,
  st_show_fps,
  st_audio_enabled,
  st_text_font_family,
  st_text_font_size,
  st_rows_count
};

#define ST_MAX_LIST_VALUES 16

static const char *st_themeNames[ST_MAX_LIST_VALUES];
static const char *st_booleanNames[] = { "false", "true" };
static const char *st_fontFamilyNames[ST_MAX_LIST_VALUES];

static gme_item st_items[st_rows_count] = {
  [st_theme] = { .type = gme_item_list, .id = st_theme, .text = "Theme", .values = st_themeNames, .wrapValues = 1 },
  [st_show_fps] = { .type = gme_item_list, .id = st_show_fps, .text = "Show FPS", .values = st_booleanNames, .valuesCount = 2, .wrapValues = 1 },
  [st_audio_enabled] = { .type = gme_item_list, .id = st_audio_enabled, .text = "Audio", .values = st_booleanNames, .valuesCount = 2, .wrapValues = 1 },
  [st_text_font_family] = { .type = gme_item_list, .id = st_text_font_family, .text = "Text font", .values = st_fontFamilyNames, .wrapValues = 1 },
  [st_text_font_size] = { .type = gme_item_number, .id = st_text_font_size, .text = "Text size", .minValue = TEXT_FONT_SIZE_MIN, .maxValue = TEXT_FONT_SIZE_MAX },
};
static gme_menu st_menu;
/// @brief Status set by B (the screen that opened the settings)
static int st_returnStatus = MainMenu;
static int st_menuInitialized = 0;

static const char *logLevelNames[] = { "Debug", "Info", "Warning", "Error" };

static int st_currentTheme()
{
  for (int i = 0; i < themesCount; i++) {
    if (themes[i].theme == _theme)
      return i;
  }
  return 0;
}

void settingsLoad()
{
  FILE *f = fopen(SETTINGS_FILE, "r");
  if (f == NULL) {
    // First run: create the file with the default values
    logInfo("\tCreating %s with the default settings", SETTINGS_FILE);
    settingsSave();
    return;
  }

  char line[256];
  while (fgets(line, sizeof(line), f) != NULL) {
    trim(line);
    if (startsWith("theme=", line)) {
      const char *value = line + 6;
      for (int i = 0; i < themesCount; i++) {
        if (stricmp(value, themes[i].name) == 0) {
          _theme = themes[i].theme;
          break;
        }
      }
    } else if (startsWith("audio_enabled=", line)) {
      const char *value = line + 14;
      _Globals.audio_enabled = stricmp(value, "true") == 0 || strcmp(value, "1") == 0;
    } else if (startsWith("show_fps=", line)) {
      const char *value = line + 9;
      _Globals.show_fps = stricmp(value, "true") == 0 || strcmp(value, "1") == 0;
    } else if (startsWith("text_font_family=", line)) {
      const char *value = line + 17;
      int found = 0;
      for (int i = 0; i < textFontFamiliesCount; i++) {
        if (stricmp(value, textFontFamilies[i].name) == 0) {
          _Globals.textFontFamily = i;
          found = 1;
          break;
        }
      }
      if (!found)
        logWarning("\tInvalid text_font_family in %s: %s", SETTINGS_FILE, value);
    } else if (startsWith("text_font_size=", line)) {
      int size = atoi(line + 15);
      if (size < TEXT_FONT_SIZE_MIN)
        size = TEXT_FONT_SIZE_MIN;
      else if (size > TEXT_FONT_SIZE_MAX)
        size = TEXT_FONT_SIZE_MAX;
      _Globals.textFontSize = size;
    } else if (startsWith("log_level=", line)) {
      const char *value = line + 10;
      int found = 0;
      for (int i = Debug; i <= Error; i++) {
        if (stricmp(value, logLevelNames[i]) == 0) {
          _Globals.logLevel = i;
          found = 1;
          break;
        }
      }
      if (!found)
        logWarning("\tInvalid log_level in %s: %s", SETTINGS_FILE, value);
    }
  }
  fclose(f);
  logInfo("\tTheme: %s", themes[st_currentTheme()].name);
}

void settingsSave()
{
  FILE *f = fopen(SETTINGS_FILE, "w");
  if (f == NULL) {
    logWarning("Cannot write %s", SETTINGS_FILE);
    return;
  }
  fprintf(f, "theme=%s\n", themes[st_currentTheme()].name);
  fprintf(f, "show_fps=%s\n", _Globals.show_fps ? "true" : "false");
  fprintf(f, "audio_enabled=%s\n", _Globals.audio_enabled ? "true" : "false");
  fprintf(f, "text_font_family=%s\n", textFontFamilies[_Globals.textFontFamily >= 0 && _Globals.textFontFamily < textFontFamiliesCount ? _Globals.textFontFamily : 0].name);
  fprintf(f, "text_font_size=%d\n", _Globals.textFontSize);
  fprintf(f, "log_level=%s\n", logLevelNames[_Globals.logLevel >= Debug && _Globals.logLevel <= Error ? _Globals.logLevel : Info]);
  fclose(f);
}

static void st_initMenu()
{
  st_items[st_theme].valuesCount = themesCount < ST_MAX_LIST_VALUES ? themesCount : ST_MAX_LIST_VALUES;
  for (int i = 0; i < st_items[st_theme].valuesCount; i++)
    st_themeNames[i] = themes[i].name;

  st_items[st_text_font_family].valuesCount = textFontFamiliesCount < ST_MAX_LIST_VALUES ? textFontFamiliesCount : ST_MAX_LIST_VALUES;
  for (int i = 0; i < st_items[st_text_font_family].valuesCount; i++)
    st_fontFamilyNames[i] = textFontFamilies[i].name;

  gme_init(&st_menu, st_items, st_rows_count);
  st_menu.y = HEADER_HEIGHT + 10 + sdlTitleHeight() + 6;
}

/// @brief Copy the current settings into the menu items
static void st_syncItems()
{
  st_items[st_theme].valueIdx = st_currentTheme();
  st_items[st_show_fps].valueIdx = _Globals.show_fps ? 1 : 0;
  st_items[st_audio_enabled].valueIdx = _Globals.audio_enabled ? 1 : 0;
  st_items[st_text_font_family].valueIdx = _Globals.textFontFamily;
  st_items[st_text_font_size].value = _Globals.textFontSize;
}

/// @brief Apply the value of a menu item changed by the user
static void st_applyItem(const gme_item *item)
{
  switch (item->id) {
    case st_theme:
      _theme = themes[item->valueIdx].theme;
      break;
    case st_show_fps:
      _Globals.show_fps = item->valueIdx == 1;
      break;
    case st_audio_enabled:
      _Globals.audio_enabled = item->valueIdx == 1;
      // Stop the sound being played (e.g. the menu sound of this change)
      if (!_Globals.audio_enabled)
        Mix_HaltChannel(-1);
      break;
    case st_text_font_family: {
      int oldFamily = _Globals.textFontFamily;
      _Globals.textFontFamily = item->valueIdx;
      if (loadTextFonts() != 0) {
        // Font not available: keep the current one (the item is synced again)
        _Globals.textFontFamily = oldFamily;
        return;
      }
      break;
    }
    case st_text_font_size: {
      int oldSize = _Globals.textFontSize;
      _Globals.textFontSize = item->value;
      if (loadTextFonts() != 0) {
        _Globals.textFontSize = oldSize;
        return;
      }
      break;
    }
  }
  settingsSave();
  // A game is running (settings opened from the game menu): render its page again
  if (runningGame != NULL)
    pg_refreshLayout();
}

void settingsOpen(int returnStatus)
{
  st_returnStatus = returnStatus;
  _Globals.status = Settings;
}

void drawSettings()
{
  if (!st_menuInitialized) {
    st_initMenu();
    st_menuInitialized = 1;
  }
  st_syncItems();

  switch (gme_handleInput(&st_menu)) {
    case gme_event_value_changed:
      st_applyItem(gme_selectedItem(&st_menu));
      st_syncItems();
      break;
    case gme_event_back:
      playMenuBackSound();
      gme_select(&st_menu, 0);
      _Globals.status = st_returnStatus;
      st_returnStatus = MainMenu;
      return;
    default:
      break;
  }

  sdlDrawTitle("Settings", st_menu.margin + st_menu.textIndent, HEADER_HEIGHT + 10);
  gme_draw(&st_menu);

  // Preview of the game text font, below the menu
  const int selected = st_menu.selectedIdx;
  if ((selected == st_text_font_family || selected == st_text_font_size) && _Globals.textFont != NULL) {
    int y = st_menu.y;
    for (int i = 0; i < st_rows_count; i++)
      y += gme_itemHeight(&st_items[i]) + st_menu.spacing;
    sdlWriteText(_Globals.textFont, _theme->textColor, "The quick brown fox", st_menu.margin + st_menu.textIndent, y + 10, 0, 0);
  }

  gu_drawFooter(2, NULL, "Back");
}

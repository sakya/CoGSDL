#include "CoGSDL.h"

#define SETTINGS_FILE "./settings.cfg"

typedef struct {
  const char *name;
  const theme *theme;
} theme_entry;

static const theme_entry themes[] = {
  { "Sepia", &sepiaTheme },
  { "Light", &lightTheme }
};
static const int themesCount = sizeof(themes) / sizeof(themes[0]);

enum settings_row
{
  st_theme = 0,
  st_rows_count
};

static int rowIdx = 0;

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
    } else if (startsWith("show_fps=", line)) {
      const char *value = line + 9;
      _Globals.show_fps = stricmp(value, "true") == 0 || strcmp(value, "1") == 0;
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
  // Not shown in the settings screen: edit settings.cfg to change it
  fprintf(f, "show_fps=%s\n", _Globals.show_fps ? "true" : "false");
  fclose(f);
}

static void st_changeTheme(int delta)
{
  int idx = (st_currentTheme() + delta + themesCount) % themesCount;
  _theme = themes[idx].theme;
  settingsSave();
  Mix_PlayChannel(-1, sound_menu_change, 0);
}

void drawSettings()
{
  if (_Globals.input[_Keys.KeyDown] == 1) {
    _Globals.input[_Keys.KeyDown] = 0;
    if (rowIdx < st_rows_count - 1) {
      rowIdx++;
      Mix_PlayChannel(-1, sound_menu_change, 0);
    }
  } else if (_Globals.input[_Keys.KeyUp] == 1) {
    _Globals.input[_Keys.KeyUp] = 0;
    if (rowIdx > 0) {
      rowIdx--;
      Mix_PlayChannel(-1, sound_menu_change, 0);
    }
  } else if (_Globals.input[_Keys.KeyLeft] == 1 || _Globals.input[_Keys.KeyRight] == 1) {
    int delta = _Globals.input[_Keys.KeyRight] == 1 ? 1 : -1;
    _Globals.input[_Keys.KeyLeft] = 0;
    _Globals.input[_Keys.KeyRight] = 0;
    if (rowIdx == st_theme)
      st_changeTheme(delta);
  } else if (_Globals.input[_Keys.KeyB] == 1) {
    _Globals.input[_Keys.KeyB] = 0;
    rowIdx = 0;
    _Globals.status = MainMenu;
    return;
  }

  int y = HEADER_HEIGHT + 20;
  SDL_Color color = rowIdx == st_theme ? _theme->titleTextColor : _theme->textColor;
  if (rowIdx == st_theme)
    sdlDrawSelection(y - 2, sdlSelectionHeight());
  char value[64];
  snprintf(value, sizeof(value), "< %s >", themes[st_currentTheme()].name);
  sdlWriteText(_Globals.defaultFont, color, "Theme", 30, y, 0, 0);

  int width;
  TTF_MeasureUTF8(_Globals.defaultFont, value, _Globals.windowWidth, &width, NULL);
  sdlWriteText(_Globals.defaultFont, color, value, _Globals.windowWidth - width - 30, y, 0, 0);

  sdlBlitImage(button_b, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(dpad, 5, _Globals.windowHeight - 30, 30, 30);
}

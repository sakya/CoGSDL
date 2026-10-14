#include "CoGSDL.h"

#define SETTINGS_FILE "./settings.cfg"

typedef struct {
  const char *name;
  const theme *theme;
} theme_entry;

static const theme_entry themes[] = {
  { "Sepia", &sepiaTheme },
  { "Dark", &darkTheme },
  { "Light", &lightTheme }
};
static const int themesCount = sizeof(themes) / sizeof(themes[0]);

static int themeIdx = -1;

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
  if (f == NULL)
    return;

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
  fclose(f);
}

void drawSettings()
{
  if (themeIdx < 0)
    themeIdx = st_currentTheme();

  if (_Globals.input[_Keys.KeyDown] == 1) {
    _Globals.input[_Keys.KeyDown] = 0;
    themeIdx = (themeIdx + 1) % themesCount;
    Mix_PlayChannel(-1, sound_menu_change, 0);
  } else if (_Globals.input[_Keys.KeyUp] == 1) {
    _Globals.input[_Keys.KeyUp] = 0;
    themeIdx = (themeIdx + themesCount - 1) % themesCount;
    Mix_PlayChannel(-1, sound_menu_change, 0);
  } else if (_Globals.input[_Keys.KeyA] == 1) {
    _Globals.input[_Keys.KeyA] = 0;
    if (themes[themeIdx].theme != _theme) {
      _theme = themes[themeIdx].theme;
      settingsSave();
    }
  } else if (_Globals.input[_Keys.KeyB] == 1) {
    _Globals.input[_Keys.KeyB] = 0;
    themeIdx = -1;
    _Globals.status = MainMenu;
    return;
  }

  sdlWriteText(_Globals.defaultFont, _theme->textColor, "Theme", 30, HEADER_HEIGHT + 10, 0, 0);

  for (int i = 0; i < themesCount; i++) {
    char item[64];
    if (themes[i].theme == _theme)
      snprintf(item, sizeof(item), "%s (in use)", themes[i].name);
    else
      snprintf(item, sizeof(item), "%s", themes[i].name);
    sdlWriteText(_Globals.defaultFont, i == themeIdx ? _theme->selectedTextColor : _theme->textColor,
                 item, 50, HEADER_HEIGHT + 50 + i * 30, 0, 0);
  }

  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(button_b, _Globals.windowWidth - 70, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}

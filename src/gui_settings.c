#include "CoGSDL.h"

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
  st_text_font_family,
  st_text_font_size,
  st_rows_count
};

static int rowIdx = 0;

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
  fprintf(f, "text_font_family=%s\n", textFontFamilies[_Globals.textFontFamily >= 0 && _Globals.textFontFamily < textFontFamiliesCount ? _Globals.textFontFamily : 0].name);
  fprintf(f, "text_font_size=%d\n", _Globals.textFontSize);
  fprintf(f, "log_level=%s\n", logLevelNames[_Globals.logLevel >= Debug && _Globals.logLevel <= Error ? _Globals.logLevel : Info]);
  fclose(f);
}

static void st_changeTheme(int delta)
{
  int idx = (st_currentTheme() + delta + themesCount) % themesCount;
  _theme = themes[idx].theme;
  settingsSave();
  Mix_PlayChannel(-1, sound_menu_change, 0);
}

static void st_toggleShowFps()
{
  _Globals.show_fps = !_Globals.show_fps;
  settingsSave();
  Mix_PlayChannel(-1, sound_menu_change, 0);
}

static void st_changeTextFontFamily(int delta)
{
  int oldFamily = _Globals.textFontFamily;
  _Globals.textFontFamily = (oldFamily + delta + textFontFamiliesCount) % textFontFamiliesCount;
  if (loadTextFonts() != 0) {
    _Globals.textFontFamily = oldFamily;
    return;
  }
  settingsSave();
  Mix_PlayChannel(-1, sound_menu_change, 0);
}

static void st_changeTextFontSize(int delta)
{
  int size = _Globals.textFontSize + delta;
  if (size < TEXT_FONT_SIZE_MIN || size > TEXT_FONT_SIZE_MAX)
    return;

  int oldSize = _Globals.textFontSize;
  _Globals.textFontSize = size;
  if (loadTextFonts() != 0) {
    _Globals.textFontSize = oldSize;
    return;
  }
  settingsSave();
  Mix_PlayChannel(-1, sound_menu_change, 0);
}

// Draws a settings row: label on the left, "< value >" on the right
static void st_drawRow(int row, int y, const char *label, const char *valueText)
{
  SDL_Color color = rowIdx == row ? _theme->titleTextColor : _theme->textColor;
  if (rowIdx == row)
    sdlDrawSelection(y - 2, sdlSelectionHeight());
  sdlWriteText(_Globals.defaultFont, color, (char *)label, 30, y, 0, 0);

  char value[64];
  snprintf(value, sizeof(value), "< %s >", valueText);
  int width;
  TTF_MeasureUTF8(_Globals.defaultFont, value, _Globals.windowWidth, &width, NULL);
  sdlWriteText(_Globals.defaultFont, color, value, _Globals.windowWidth - width - 30, y, 0, 0);
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
    else if (rowIdx == st_show_fps)
      st_toggleShowFps();
    else if (rowIdx == st_text_font_family)
      st_changeTextFontFamily(delta);
    else if (rowIdx == st_text_font_size)
      st_changeTextFontSize(delta);
  } else if (_Globals.input[_Keys.KeyB] == 1) {
    _Globals.input[_Keys.KeyB] = 0;
    rowIdx = 0;
    _Globals.status = MainMenu;
    return;
  }

  int y = HEADER_HEIGHT + 20;
  const int rowHeight = 30;
  st_drawRow(st_theme, y, "Theme", themes[st_currentTheme()].name);
  st_drawRow(st_show_fps, y + rowHeight, "Show FPS", _Globals.show_fps ? "true" : "false");
  char fontSize[16];
  snprintf(fontSize, sizeof(fontSize), "%d", _Globals.textFontSize);
  st_drawRow(st_text_font_family, y + 2 * rowHeight, "Text font", textFontFamilies[_Globals.textFontFamily].name);
  st_drawRow(st_text_font_size, y + 3 * rowHeight, "Text size", fontSize);

  // Preview of the game text font
  if ((rowIdx == st_text_font_family || rowIdx == st_text_font_size) && _Globals.textFont != NULL)
    sdlWriteText(_Globals.textFont, _theme->textColor, "The quick brown fox", 30, y + 4 * rowHeight + 10, 0, 0);

  sdlBlitImage(button_b, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
  sdlBlitImage(dpad, 5, _Globals.windowHeight - 30, 30, 30);
}

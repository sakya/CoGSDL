#include "CoGSDL.h"

// On screen keyboard
//
// Controls:
//   D-pad   move the selection
//   A       press the selected key
//   B       delete the last character
//   X       space (text only)
//   Y       shift (text only)
//   Start   confirm
//   Select  cancel

#define OSK_COLS 10
#define OSK_CHAR_ROWS 4
#define OSK_BUFFER 1024

#define OSK_MARGIN 20
#define OSK_KEY_WIDTH 56
#define OSK_KEY_HEIGHT 50
#define OSK_KEY_GAP 4

enum osk_layout
{
  osk_lower = 0,
  osk_upper,
  osk_symbols,
  osk_numbers,
  osk_layouts_count
};

// An empty string is a missing key (the keys of a row are left aligned)
static const char *osk_keys[osk_layouts_count][OSK_CHAR_ROWS][OSK_COLS] = {
  {
    { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" },
    { "q", "w", "e", "r", "t", "y", "u", "i", "o", "p" },
    { "a", "s", "d", "f", "g", "h", "j", "k", "l", "'" },
    { "z", "x", "c", "v", "b", "n", "m", ",", ".", "-" }
  },
  {
    { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" },
    { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P" },
    { "A", "S", "D", "F", "G", "H", "J", "K", "L", "\"" },
    { "Z", "X", "C", "V", "B", "N", "M", ";", ":", "_" }
  },
  {
    { "!", "?", "@", "#", "$", "%", "&", "*", "(", ")" },
    { "+", "=", "/", "\\", "<", ">", "[", "]", "{", "}" },
    { "\xC3\xA0", "\xC3\xA8", "\xC3\xA9", "\xC3\xAC", "\xC3\xB2", "\xC3\xB9", "\xC3\xA7", "\xC3\xB1", "\xC3\xBC", "\xC3\x9F" },
    { "\xE2\x82\xAC", "\xC2\xA3", "~", "^", "`", "|", "\xC2\xBF", "\xC2\xA1", "\xC2\xAB", "\xC2\xBB" }
  },
  {
    { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" },
    { "-", ".", "", "", "", "", "", "", "", "" },
    { "", "", "", "", "", "", "", "", "", "" },
    { "", "", "", "", "", "", "", "", "", "" }
  }
};

/// @brief The keys of the last row (each one is 2 columns wide)
enum osk_special_key
{
  osk_key_shift = 0,
  osk_key_symbols,
  osk_key_space,
  osk_key_delete,
  osk_key_ok
};

static const char *osk_special_labels[] = { "Shift", "?123", "Space", "Del", "OK" };
static const enum osk_special_key osk_text_special_keys[] = { osk_key_shift, osk_key_symbols, osk_key_space, osk_key_delete, osk_key_ok };
static const enum osk_special_key osk_numeric_special_keys[] = { osk_key_delete, osk_key_ok };

static char osk_title[256];
static char osk_text[OSK_BUFFER];
static int osk_maxLength = 0;
static int osk_allowEmpty = 0;
static int osk_numeric = 0;
static osk_validator osk_validate = NULL;
static enum status osk_returnStatus = MainMenu;
static enum osk_layout osk_currentLayout = osk_lower;
/// @brief Back to lower case after the next letter
static int osk_shiftOnce = 0;
static int osk_row = 0;
static int osk_col = 0;
/// @brief 1: confirmed text available, 0: none
static int osk_resultReady = 0;
static char osk_error[256];
static Uint32 osk_errorTime = 0;

/// @brief Number of UTF-8 characters
static int osk_utf8Length(const char *text)
{
  int count = 0;
  for (; *text; text++) {
    if ((*text & 0xC0) != 0x80)
      count++;
  }
  return count;
}

/// @brief Number of rows of character keys of the current layout
static int osk_charRows()
{
  int rows = 0;
  for (int r = 0; r < OSK_CHAR_ROWS; r++) {
    if (strlen(osk_keys[osk_currentLayout][r][0]) > 0)
      rows = r + 1;
  }
  return rows;
}

/// @brief Number of keys in a row (the last row has the special keys)
static int osk_rowKeys(int row)
{
  if (row == osk_charRows())
    return osk_numeric ? (int)(sizeof(osk_numeric_special_keys) / sizeof(osk_numeric_special_keys[0]))
                       : (int)(sizeof(osk_text_special_keys) / sizeof(osk_text_special_keys[0]));
  int count = 0;
  while (count < OSK_COLS && strlen(osk_keys[osk_currentLayout][row][count]) > 0)
    count++;
  return count;
}

static enum osk_special_key osk_specialKey(int index)
{
  return osk_numeric ? osk_numeric_special_keys[index] : osk_text_special_keys[index];
}

static void osk_openKeyboard(const char *title, const char *text, int maxLength, int allowEmpty, int numeric, osk_validator validate)
{
  snprintf(osk_title, sizeof(osk_title), "%s", title ? title : "");
  snprintf(osk_text, sizeof(osk_text), "%s", text ? text : "");
  osk_maxLength = maxLength > 0 && maxLength < OSK_BUFFER / 4 ? maxLength : OSK_BUFFER / 4 - 1;
  osk_allowEmpty = allowEmpty;
  osk_numeric = numeric;
  osk_validate = validate;
  osk_returnStatus = _Globals.status;
  osk_resultReady = 0;
  osk_errorTime = 0;
  if (numeric) {
    osk_currentLayout = osk_numbers;
    osk_shiftOnce = 0;
    osk_row = 0;
  } else {
    // Start with a capital letter
    osk_currentLayout = strlen(osk_text) == 0 ? osk_upper : osk_lower;
    osk_shiftOnce = osk_currentLayout == osk_upper;
    osk_row = 1;
  }
  osk_col = 0;

  _Globals.status = OnScreenKeyboard;
}

void osk_open(const char *title, const char *text, int maxLength, int allowEmpty)
{
  osk_openKeyboard(title, text, maxLength, allowEmpty, 0, NULL);
}

void osk_openNumeric(const char *title, const char *text, int maxLength, osk_validator validate)
{
  osk_openKeyboard(title, text, maxLength, 0, 1, validate);
}

int osk_getResult(char *text, size_t size)
{
  if (!osk_resultReady)
    return 0;
  osk_resultReady = 0;
  snprintf(text, size, "%s", osk_text);
  return 1;
}

static void osk_close(int confirmed)
{
  osk_resultReady = confirmed;
  _Globals.status = osk_returnStatus;
}

static void osk_showError(const char *message)
{
  snprintf(osk_error, sizeof(osk_error), "%s", message);
  osk_errorTime = SDL_GetTicks();
  // 0 means "no error"
  if (osk_errorTime == 0)
    osk_errorTime = 1;
}

static void osk_type(const char *chars)
{
  if (osk_utf8Length(osk_text) >= osk_maxLength)
    return;
  if (strlen(osk_text) + strlen(chars) >= sizeof(osk_text))
    return;
  strcat(osk_text, chars);

  if (osk_shiftOnce && osk_currentLayout == osk_upper && isalpha((unsigned char)chars[0])) {
    osk_currentLayout = osk_lower;
    osk_shiftOnce = 0;
  }
}

static void osk_delete()
{
  int len = strlen(osk_text);
  if (len == 0)
    return;
  // Remove the continuation bytes of a multi-byte character
  while (len > 0 && (osk_text[len - 1] & 0xC0) == 0x80)
    len--;
  if (len > 0)
    len--;
  osk_text[len] = '\0';
}

/// @brief Change the layout keeping the selection on an existing key
static void osk_setLayout(enum osk_layout layout)
{
  int wasSpecialRow = osk_row == osk_charRows();
  osk_currentLayout = layout;
  if (wasSpecialRow)
    osk_row = osk_charRows();
  else if (osk_row >= osk_charRows())
    osk_row = osk_charRows() - 1;
  if (osk_col >= osk_rowKeys(osk_row))
    osk_col = osk_rowKeys(osk_row) - 1;
}

static void osk_toggleShift()
{
  if (osk_numeric)
    return;
  if (osk_currentLayout == osk_upper) {
    osk_setLayout(osk_lower);
    osk_shiftOnce = 0;
  } else {
    osk_setLayout(osk_upper);
    osk_shiftOnce = 1;
  }
}

static void osk_confirm()
{
  char trimmed[OSK_BUFFER];
  snprintf(trimmed, sizeof(trimmed), "%s", osk_text);
  trim(trimmed);
  if (!osk_allowEmpty && !osk_validate && strlen(trimmed) == 0) {
    osk_showError("Don't just leave it blank! Type something!");
    return;
  }
  if (osk_validate) {
    const char *error = osk_validate(trimmed);
    if (error != NULL) {
      osk_showError(error);
      return;
    }
  }
  osk_close(1);
}

static void osk_pressSelected()
{
  if (osk_row < osk_charRows()) {
    osk_type(osk_keys[osk_currentLayout][osk_row][osk_col]);
    return;
  }

  switch (osk_specialKey(osk_col)) {
    case osk_key_shift:
      osk_toggleShift();
      break;
    case osk_key_symbols:
      osk_setLayout(osk_currentLayout == osk_symbols ? osk_lower : osk_symbols);
      osk_shiftOnce = 0;
      break;
    case osk_key_space:
      osk_type(" ");
      break;
    case osk_key_delete:
      osk_delete();
      break;
    case osk_key_ok:
      osk_confirm();
      break;
  }
}

/// @brief Move the selection, the keys of the last row are 2 columns wide
static void osk_move(int dRow, int dCol)
{
  const int rows = osk_charRows() + 1;
  const int specialRow = rows - 1;
  if (dRow != 0) {
    int newRow = (osk_row + dRow + rows) % rows;
    int col = osk_col;
    if (osk_row == specialRow && newRow != specialRow)
      col = col * 2;
    else if (osk_row != specialRow && newRow == specialRow)
      col = col / 2;
    if (col >= osk_rowKeys(newRow))
      col = osk_rowKeys(newRow) - 1;
    osk_row = newRow;
    osk_col = col;
  }
  if (dCol != 0) {
    int cols = osk_rowKeys(osk_row);
    osk_col = (osk_col + dCol + cols) % cols;
  }
  playMenuSound();
}

static void osk_drawKey(const char *label, int x, int y, int w, int h, int selected)
{
  SDL_Rect rect = { x, y, w, h };
  SDL_Color textColor = _theme->textColor;
  if (selected) {
    SDL_SetRenderDrawColor(_Globals.renderer, _theme->selectedTextColor.r, _theme->selectedTextColor.g, _theme->selectedTextColor.b, 255);
    SDL_RenderFillRect(_Globals.renderer, &rect);
    textColor = _theme->backgroundColor;
  } else {
    SDL_SetRenderDrawColor(_Globals.renderer, _theme->textColor.r, _theme->textColor.g, _theme->textColor.b, 255);
    SDL_RenderDrawRect(_Globals.renderer, &rect);
  }

  int tw = 0, th = 0;
  TTF_SizeUTF8(_Globals.defaultFont, label, &tw, &th);
  sdlWriteText(_Globals.defaultFont, textColor, (char *)label, x + (w - tw) / 2, y + (h - th) / 2, 0, 0);
}

static void osk_drawTextBox(int x, int y, int w, int h)
{
  SDL_Rect rect = { x, y, w, h };
  SDL_SetRenderDrawColor(_Globals.renderer, _theme->textColor.r, _theme->textColor.g, _theme->textColor.b, 255);
  SDL_RenderDrawRect(_Globals.renderer, &rect);

  // Show the end of the text if it doesn't fit in the box
  const int maxWidth = w - 20;
  const char *visible = osk_text;
  int tw = 0, th = 0;
  TTF_SizeUTF8(_Globals.defaultFont, visible, &tw, &th);
  while (*visible && tw > maxWidth) {
    visible++;
    while ((*visible & 0xC0) == 0x80)
      visible++;
    TTF_SizeUTF8(_Globals.defaultFont, visible, &tw, &th);
  }
  if (th == 0)
    th = get_line_height(_Globals.defaultFont);

  int textY = y + (h - th) / 2;
  sdlWriteText(_Globals.defaultFont, _theme->textColor, (char *)visible, x + 8, textY, 0, 0);

  // Blinking cursor
  if ((SDL_GetTicks() / 500) % 2 == 0) {
    SDL_Rect cursor = { x + 8 + (*visible ? tw : 0) + 1, textY + 2, 2, th - 4 };
    SDL_SetRenderDrawColor(_Globals.renderer, _theme->selectedTextColor.r, _theme->selectedTextColor.g, _theme->selectedTextColor.b, 255);
    SDL_RenderFillRect(_Globals.renderer, &cursor);
  }
}

void drawOsk()
{
  if (_Globals.input[_Keys.KeyUp] == 1) {
    _Globals.input[_Keys.KeyUp] = 0;
    osk_move(-1, 0);
  } else if (_Globals.input[_Keys.KeyDown] == 1) {
    _Globals.input[_Keys.KeyDown] = 0;
    osk_move(1, 0);
  } else if (_Globals.input[_Keys.KeyLeft] == 1) {
    _Globals.input[_Keys.KeyLeft] = 0;
    osk_move(0, -1);
  } else if (_Globals.input[_Keys.KeyRight] == 1) {
    _Globals.input[_Keys.KeyRight] = 0;
    osk_move(0, 1);
  } else if (_Globals.input[_Keys.KeyA] == 1) {
    _Globals.input[_Keys.KeyA] = 0;
    osk_pressSelected();
  } else if (_Globals.input[_Keys.KeyB] == 1) {
    _Globals.input[_Keys.KeyB] = 0;
    osk_delete();
  } else if (_Globals.input[_Keys.KeyX] == 1) {
    _Globals.input[_Keys.KeyX] = 0;
    if (!osk_numeric)
      osk_type(" ");
  } else if (_Globals.input[_Keys.KeyY] == 1) {
    _Globals.input[_Keys.KeyY] = 0;
    osk_toggleShift();
  } else if (_Globals.input[_Keys.KeyStart] == 1) {
    _Globals.input[_Keys.KeyStart] = 0;
    osk_confirm();
  } else if (_Globals.input[_Keys.KeySelect] == 1) {
    _Globals.input[_Keys.KeySelect] = 0;
    osk_close(0);
  }
  if (_Globals.status != OnScreenKeyboard)
    return;

  int y = HEADER_HEIGHT + 8;
  if (strlen(osk_title) > 0) {
    // Show the end of the title if it is too long
    const int maxWidth = _Globals.windowWidth - 2 * OSK_MARGIN;
    const char *visible = osk_title;
    char title[sizeof(osk_title) + 4];
    int tw = 0;
    TTF_SizeUTF8(_Globals.defaultSmallFont, visible, &tw, NULL);
    snprintf(title, sizeof(title), "%s", visible);
    while (*visible && tw > maxWidth) {
      visible++;
      while ((*visible & 0xC0) == 0x80)
        visible++;
      snprintf(title, sizeof(title), "...%s", visible);
      TTF_SizeUTF8(_Globals.defaultSmallFont, title, &tw, NULL);
    }
    sdlWriteText(_Globals.defaultSmallFont, _theme->textColor, title, OSK_MARGIN, y, 0, 0);
  }

  const int boxY = HEADER_HEIGHT + 32;
  const int boxH = 40;
  osk_drawTextBox(OSK_MARGIN, boxY, _Globals.windowWidth - 2 * OSK_MARGIN, boxH);

  if (osk_errorTime > 0 && SDL_GetTicks() - osk_errorTime < 2500)
    sdlWriteText(_Globals.defaultSmallFont, _theme->selectedTextColor, osk_error, OSK_MARGIN, boxY + boxH + 4, 0, 0);

  // Keys
  const int keysY = HEADER_HEIGHT + 100;
  const int step = OSK_KEY_WIDTH + OSK_KEY_GAP;
  const int charRows = osk_charRows();
  for (int r = 0; r < charRows; r++) {
    for (int c = 0; c < osk_rowKeys(r); c++) {
      osk_drawKey(osk_keys[osk_currentLayout][r][c],
                  OSK_MARGIN + c * step, keysY + r * (OSK_KEY_HEIGHT + OSK_KEY_GAP),
                  OSK_KEY_WIDTH, OSK_KEY_HEIGHT, r == osk_row && c == osk_col);
    }
  }
  for (int k = 0; k < osk_rowKeys(charRows); k++) {
    enum osk_special_key key = osk_specialKey(k);
    const char *label = osk_special_labels[key];
    if (key == osk_key_shift && osk_currentLayout == osk_upper)
      label = "SHIFT";
    else if (key == osk_key_symbols && osk_currentLayout == osk_symbols)
      label = "abc";
    osk_drawKey(label,
                OSK_MARGIN + k * 2 * step, keysY + charRows * (OSK_KEY_HEIGHT + OSK_KEY_GAP),
                2 * OSK_KEY_WIDTH + OSK_KEY_GAP, OSK_KEY_HEIGHT, osk_row == charRows && k == osk_col);
  }

  // Footer
  sdlBlitImage(dpad, 5, _Globals.windowHeight - 30, 30, 30);
  sdlWriteText(_Globals.defaultSmallFont, _theme->titleTextColor,
               osk_numeric ? "B: Delete   Start: OK   Select: Cancel"
                           : "B: Delete   X: Space   Y: Shift   Start: OK   Select: Cancel",
               45, _Globals.windowHeight - 26, 0, 0);
  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
}

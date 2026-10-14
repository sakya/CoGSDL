#include "CoGSDL.h"

// On screen keyboard
//
// Controls:
//   D-pad   move the selection
//   A       press the selected key
//   B       delete the last character
//   X       space
//   Y       shift (lower/upper case)
//   Start   confirm
//   Select  cancel

#define OSK_COLS 10
#define OSK_CHAR_ROWS 4
#define OSK_ROWS (OSK_CHAR_ROWS + 1)
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
  osk_layouts_count
};

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
  }
};

/// @brief The keys of the last row (each one is 2 columns wide)
enum osk_special_key
{
  osk_key_shift = 0,
  osk_key_symbols,
  osk_key_space,
  osk_key_delete,
  osk_key_ok,
  osk_special_keys_count
};

static const char *osk_special_labels[osk_special_keys_count] = { "Shift", "?123", "Space", "Del", "OK" };

static char osk_title[256];
static char osk_text[OSK_BUFFER];
static int osk_maxLength = 0;
static int osk_allowEmpty = 0;
static enum status osk_returnStatus = MainMenu;
static enum osk_layout osk_currentLayout = osk_lower;
/// @brief Back to lower case after the next letter
static int osk_shiftOnce = 0;
static int osk_row = 0;
static int osk_col = 0;
/// @brief 1: confirmed text available, 0: none
static int osk_resultReady = 0;
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

void osk_open(const char *title, const char *text, int maxLength, int allowEmpty)
{
  snprintf(osk_title, sizeof(osk_title), "%s", title ? title : "");
  snprintf(osk_text, sizeof(osk_text), "%s", text ? text : "");
  osk_maxLength = maxLength > 0 && maxLength < OSK_BUFFER / 4 ? maxLength : OSK_BUFFER / 4 - 1;
  osk_allowEmpty = allowEmpty;
  osk_returnStatus = _Globals.status;
  osk_resultReady = 0;
  osk_errorTime = 0;
  osk_row = 1;
  osk_col = 0;
  // Start with a capital letter
  osk_currentLayout = strlen(osk_text) == 0 ? osk_upper : osk_lower;
  osk_shiftOnce = osk_currentLayout == osk_upper;

  _Globals.status = OnScreenKeyboard;
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

static void osk_toggleShift()
{
  if (osk_currentLayout == osk_upper) {
    osk_currentLayout = osk_lower;
    osk_shiftOnce = 0;
  } else {
    osk_currentLayout = osk_upper;
    osk_shiftOnce = 1;
  }
}

static void osk_confirm()
{
  char trimmed[OSK_BUFFER];
  snprintf(trimmed, sizeof(trimmed), "%s", osk_text);
  trim(trimmed);
  if (!osk_allowEmpty && strlen(trimmed) == 0) {
    osk_errorTime = SDL_GetTicks();
    return;
  }
  osk_close(1);
}

static void osk_pressSelected()
{
  if (osk_row < OSK_CHAR_ROWS) {
    osk_type(osk_keys[osk_currentLayout][osk_row][osk_col]);
    return;
  }

  switch (osk_col) {
    case osk_key_shift:
      osk_toggleShift();
      break;
    case osk_key_symbols:
      osk_currentLayout = osk_currentLayout == osk_symbols ? osk_lower : osk_symbols;
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

/// @brief Move the selection, the last row has 5 keys 2 columns wide
static void osk_move(int dRow, int dCol)
{
  if (dRow != 0) {
    int newRow = (osk_row + dRow + OSK_ROWS) % OSK_ROWS;
    if (osk_row == OSK_CHAR_ROWS && newRow != OSK_CHAR_ROWS)
      osk_col = osk_col * 2;
    else if (osk_row != OSK_CHAR_ROWS && newRow == OSK_CHAR_ROWS)
      osk_col = osk_col / 2;
    osk_row = newRow;
  }
  if (dCol != 0) {
    int cols = osk_row == OSK_CHAR_ROWS ? osk_special_keys_count : OSK_COLS;
    osk_col = (osk_col + dCol + cols) % cols;
  }
  Mix_PlayChannel(-1, sound_menu_change, 0);
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
    th = TTF_FontHeight(_Globals.defaultFont);

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

  if (osk_errorTime > 0 && SDL_GetTicks() - osk_errorTime < 2000)
    sdlWriteText(_Globals.defaultSmallFont, _theme->selectedTextColor, "Don't just leave it blank! Type something!", OSK_MARGIN, boxY + boxH + 4, 0, 0);

  // Keys
  const int keysY = HEADER_HEIGHT + 100;
  const int step = OSK_KEY_WIDTH + OSK_KEY_GAP;
  for (int r = 0; r < OSK_CHAR_ROWS; r++) {
    for (int c = 0; c < OSK_COLS; c++) {
      osk_drawKey(osk_keys[osk_currentLayout][r][c],
                  OSK_MARGIN + c * step, keysY + r * (OSK_KEY_HEIGHT + OSK_KEY_GAP),
                  OSK_KEY_WIDTH, OSK_KEY_HEIGHT, r == osk_row && c == osk_col);
    }
  }
  for (int k = 0; k < osk_special_keys_count; k++) {
    const char *label = osk_special_labels[k];
    if (k == osk_key_shift && osk_currentLayout == osk_upper)
      label = "SHIFT";
    else if (k == osk_key_symbols && osk_currentLayout == osk_symbols)
      label = "abc";
    osk_drawKey(label,
                OSK_MARGIN + k * 2 * step, keysY + OSK_CHAR_ROWS * (OSK_KEY_HEIGHT + OSK_KEY_GAP),
                2 * OSK_KEY_WIDTH + OSK_KEY_GAP, OSK_KEY_HEIGHT, osk_row == OSK_CHAR_ROWS && k == osk_col);
  }

  // Footer
  sdlBlitImage(dpad, 5, _Globals.windowHeight - 30, 30, 30);
  sdlWriteText(_Globals.defaultSmallFont, _theme->titleTextColor,
               "B: Delete   X: Space   Y: Shift   Start: OK   Select: Cancel",
               45, _Globals.windowHeight - 26, 0, 0);
  sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);
}

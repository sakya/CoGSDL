#include "CoGSDL.h"
#include "gui_playgame.h"

void pg_init();
void pg_dispose();
void pg_exec();
void pg_renderText(const char* text, int x, int y, int width, int height, int scrollY);
int pg_renderPageBreak();
int pg_indexOfWordEnd(const char *string, const int startFrom);
void pg_getCommand(const char *line, char *command) ;
void pg_goToNextScene();
void pg_appendText(const char *text, int newLine);
int pg_needsNewLine(const char* text);
label* pg_get_label(const char *labelName);
/// @brief Get a variable (temp or global)
/// @param varName The variable name
/// @return The varible or NULL
variable* pg_getVar(const char *varName);
/// @brief Set a variable (temp or global) value
/// @param varName The variable name
/// @param value The value
/// @return 1 on success
int pg_setVarValue(const char *varName, const char *value);
variable* pg_add_temp_var();
int pg_delete_temp_var(int index);
void pg_clearTempVars();
void pg_clearChoices();
/// @brief Get the end position of an if statement
/// @param fp The file pointer
/// @param indentation The if indentation
/// @return The ebd if position
long pg_get_if_end(FILE *fp, int indentation);
/// @brief Get the next line from text
/// @param text The string
/// @param position The starting position. Updated to the starting position of the next line.
/// @param maxWidth The maximum width in pixels of the rendered line
/// @param wordWrap Set to 1 if the line was wrapped
/// @param last_x Set to the last x position blitted
/// @return A line of text or NULL. The position is updated with the position of the next line
char *pg_getNextLine(const char *text, int *position, int maxWidth, int *wordWrap, int *last_x);

/// @brief Manage the image and text_image command
/// @param surface The surface to blit the image
/// @param text The text of the line with
/// @param y The y value where the image have to be drawn
/// @return The height of the image
int pg_manageImageCommand(SDL_Surface *surface, const char *text, int y);
/// @brief Manage an if
/// @param text The text of the line with the if command
/// @param indentation The line indentation
/// @param endPosition The end position of the if statement
/// @return The new file position where continue execution
long pg_manageIfCommand(const char *text, int indentation, long *endPosition);
/// @brief Evaluate an if condition. The condition does not contains parentheses or AND/OR
/// @param condition The condition
/// @return 1 if true, 0 if false
int pg_evaluateSimpleCondition(const char *condition);
int pg_evaluateCondition(const char *condition, int indentation);
int pg_evaluateExpression(const char *expression, char *value);
int pg_evaluateBoolean(const char *condition);
int pg_evaluateArithmetic(const char *expression, double *result);
/// @brief Replace ${}, $!{}, $!!{} and @{} in a text
/// @param text The text
/// @return The expanded text (must be freed) or NULL
char *pg_expandVariables(const char *text);
/// @brief Manage the goto command
/// @param text The text of the line with the goto command
/// @return The new file position
long pg_manageGotoCommand(const char *text);
long pg_manageGosubCommand(const char *text);
long pg_manageGosubSceneCommand(const char *text);
/// @brief Manage the temp command
/// @param text The text of the line with the temp command
/// @return The new file position
void pg_manageTempCommand(const char *text);
/// @brief Manage the choice command
/// @return The new file position
long pg_manageChoiceCommand();
/// @brief Manage the finish command
/// @param text
void pg_manageFinishCommand(const char *text);
void pg_manageCheckPurchaseCommand(const char *text);

int pg_get_operator(const char *condition, char *operator);
void pg_clear_image_cache();
image_cache_item* pg_get_image_from_cache(const char *name);
image_cache_item* pg_add_image_to_cache(const char *name);

void pg_read_labels();
void pg_clear_labels();

game_status pg_game_status;
static int line_height = 0;
static image_cache_item image_cache[32];
static int image_cache_count = 0;
static SDL_Surface* text_surface = NULL;
static int scroll_height;

void drawGame()
{
  int scrollY = pg_game_status.scrollY;
  if (_Globals.input[_Keys.KeyL1] == 1) {
    _Globals.input[_Keys.KeyL1] = 0;
    _Globals.status = GameMenu;
    return;
  }

  if (_Globals.input[_Keys.KeyR1] == 1) {
    _Globals.input[_Keys.KeyR1] = 0;
    _Globals.status = GameStats;
    return;
  }

  if (pg_game_status.canScrollDown) {
    if (_Globals.input[_Keys.KeyDown] == 1) {
      _Globals.input[_Keys.KeyDown]= 0;
      pg_game_status.scrollY += scroll_height;
    }
  }

  if (_Globals.input[_Keys.KeyUp] == 1) {
    _Globals.input[_Keys.KeyUp]= 0;
    pg_game_status.scrollY -= scroll_height;
    if (pg_game_status.scrollY < 0)
      pg_game_status.scrollY = 0;
  }

  if (_Globals.input[_Keys.KeyA] == 1) {
    if (!pg_game_status.canScrollDown && pg_game_status.showPageBreak) {
      //Next
      _Globals.input[_Keys.KeyA] = 0;
      pg_exec();
    } else if (!pg_game_status.canScrollDown && pg_game_status.choicesCount > 0) {
      // Choice
      _Globals.input[_Keys.KeyA] = 0;
      _Globals.status = GameChoice;
      pg_game_status.selectedChoiceIndex = -1;
      return;
    }
  }

  if (_Globals.input[_Keys.KeyStart] == 1) {
    _Globals.input[_Keys.KeyStart] = 0;
    pg_dispose();

    _Globals.status = GameSelect;
    return;
  }

  if (scrollY != pg_game_status.scrollY) {
    SDL_FreeSurface(text_surface);
    text_surface = NULL;
  }

  // Initialize
  if (pg_game_status.fp == NULL) {
    pg_init();
    pg_exec();
  }

  // A choice was selected
  if (pg_game_status.selectedChoiceIndex >= 0) {
    choice *c = &pg_game_status.choices[pg_game_status.selectedChoiceIndex];
    pg_game_status.selectedChoiceIndex = -1;
    fseek(pg_game_status.fp, c->position, SEEK_SET);
    pg_exec();
  }

  if (pg_game_status.text != NULL) {
    pg_renderText(pg_game_status.text, TEXT_MARGIN, HEADER_HEIGHT + TEXT_MARGIN, _Globals.windowWidth, _Globals.windowHeight - 2 * HEADER_HEIGHT, pg_game_status.scrollY);
    if (!pg_game_status.textFullyRendered) {
      pg_game_status.canScrollDown = 1;
    }
  }

  if (pg_game_status.showPageBreak) {
    if (pg_renderPageBreak()) {
      pg_game_status.canScrollDown = 0;
    } else {
      pg_game_status.canScrollDown = 1;
    }
  }

  if (pg_game_status.textFullyRendered && pg_game_status.choicesCount > 0) {
    pg_game_status.canScrollDown = 0;
    strcpy(pg_game_status.pageBreakMessage, "Choice");
    pg_renderPageBreak();
  }
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}

void pg_init()
{
  pg_clear_image_cache();
  if (text_surface) {
    SDL_FreeSurface(text_surface);
    text_surface = NULL;
  }

  char filePath[256];
  strcpy(filePath, runningGame->path);
  strcat(filePath, "/scenes/startup.txt");

  pg_game_status.fp = fopen(filePath, "rb");
  if (!pg_game_status.fp) {
    logError("Failed to open file %s", filePath);
    return;
  }
  logInfo("Opened file %s", filePath);
  pg_clear_labels();

  fseek(pg_game_status.fp, 0L, SEEK_END);
  pg_game_status.fpSize = ftell(pg_game_status.fp);
  fseek(pg_game_status.fp, 0L, SEEK_SET);

  pg_clearChoices();
  pg_game_status.showPageBreak = 0;
  pg_game_status.tempVariables = NULL;
  pg_game_status.text = NULL;
  pg_game_status.sceneIndex = 0;
  pg_game_status.fpPosition = runningGame->startPosition;
  pg_game_status.scrollY = 0;
  pg_game_status.textFullyRendered = 0;
  pg_game_status.lastRenderedY = 0;
  pg_game_status.selectedChoiceIndex = -1;
  pg_game_status.gosubCount = 0;
  pg_game_status.ifInfoCount = 0;
  pg_game_status.used_choices_count = 0;

  logDebug("Position set to %li", pg_game_status.fpPosition);
  fseek(pg_game_status.fp, pg_game_status.fpPosition, SEEK_SET);

  line_height = get_line_height(_Globals.textFont);
  logDebug("Line height %i", line_height);

  if (_Globals.scroll == scroll_page) {
    int lines = (_Globals.windowHeight - 2 * HEADER_HEIGHT - 2 * TEXT_MARGIN) / line_height;
    scroll_height = lines * line_height;
  } else {
    scroll_height = line_height;
  }
  logDebug("scroll_height=%i", scroll_height);
}

void pg_dispose()
{
  pg_clear_image_cache();
  pg_clearTempVars();
  pg_clearChoices();
  pg_clear_labels();
  if (text_surface) {
    SDL_FreeSurface(text_surface);
    text_surface = NULL;
  }

  pg_game_status.scrollY = 0;
  pg_game_status.textFullyRendered = 0;
  pg_game_status.lastRenderedY = 0;
  pg_game_status.gosubCount = 0;
  pg_game_status.ifInfoCount = 0;
  pg_game_status.used_choices_count = 0;

  free(pg_game_status.text);
  pg_game_status.text = NULL;

  fclose(pg_game_status.fp);
  pg_game_status.fp = NULL;
  pg_game_status.fpPosition = 0;

  freeGame(runningGame);
  runningGame = NULL;

  SDL_FreeSurface(runningGameIcon);
  runningGameIcon = NULL;
}

void pg_renderTextToSurface(SDL_Surface *surface, const char* text, int scrollY)
{
  if (!surface || text == NULL || strlen(text) == 0)
    return;

  int width = surface->w;
  int height = surface->h;

  char test[5];
  TTF_Font *font = _Globals.textFont;

  pg_game_status.textFullyRendered = 1;
  pg_game_status.lastRenderedY = 0;

  int x = TEXT_MARGIN;
  int y = TEXT_MARGIN - scrollY;
  int text_width = width - 2 * TEXT_MARGIN;
  char *line = NULL;
  int linePos = 0;
  int wrap = 0;
  int last_x = 0;
  while ((line = pg_getNextLine(text, &linePos, text_width, &wrap, &last_x)) != NULL) {
    if (startsWithWord("*text_image", line) || startsWithWord("*image", line)) {
      // Draw image
      // text_image filename.png alignment optional text
      // image filename.png alignment optional text
      y += pg_manageImageCommand(surface, line, y);
      x = TEXT_MARGIN;
      free(line);
      continue;
    }

    if (y < 0) {
      y += line_height;
      free(line);
      continue;
    }

    if (y + line_height > height) {
      pg_game_status.textFullyRendered = 0;
      free(line);
      break;
    }

    int lineLen = strlen(line);
    int pixels_to_justify = 0;
    int pixels_to_justify_first = 0;
    if (_Globals.justify_text && wrap) {
      int delta_x = text_width - last_x;
      if (delta_x > 0) {
        int spaces = 0;
        for (int i = 0; i < lineLen; i++) {
          if (line[i] == ' ')
            spaces++;
        }

        if (spaces > 0) {
          pixels_to_justify = delta_x / spaces;
          pixels_to_justify_first = delta_x - pixels_to_justify * spaces;
        }
      }
    }

    if (lineLen > 0) {
      int i = 0;
      while (i < lineLen) {
        // Check font change:
        if (line[i] == '[') {
          // Italic
          if (i + 2 < lineLen && line[i + 1] == 'i' && line[i + 2] == ']') {
            font = _Globals.textItalicFont;
            i += 3;
            continue;
          } else if (i + 3 < lineLen && line[i + 1] == '/' && line[i + 2] == 'i' && line[i + 3] == ']') {
            font = _Globals.textFont;
            i += 4;
            continue;
          }

          // Bold
          if (i + 2 < lineLen && line[i + 1] == 'b' && line[i + 2] == ']') {
            font = _Globals.textBoldFont;
            i += 3;
            continue;
          } else if (i + 3 < lineLen && line[i + 1] == '/' && line[i + 2] == 'b' && line[i + 3] == ']') {
            font = _Globals.textFont;
            i += 4;
            continue;
          }
        }

        int charBytes = u8seqlen(&line[i]);
        memset(&test, 0, sizeof(test));
        strncpy(test, &line[i], charBytes);

        SDL_Surface *char_surface = TTF_RenderUTF8_Blended(font, test, _theme->textColor);
        SDL_Rect source_rect = { 0, 0, char_surface->w, char_surface->h};
        SDL_Rect dest_rect = { x, y, source_rect.w, source_rect.h };

        if (SDL_BlitSurface(char_surface, &source_rect, surface, &dest_rect)) {
          logError("SDL_BlitSurface Fail: %s", SDL_GetError());
        }
        SDL_FreeSurface(char_surface);
        x += source_rect.w;

        if (charBytes == 1 && line[i] == ' ') {
          if (pixels_to_justify_first) {
            x += pixels_to_justify_first;
            pixels_to_justify_first = 0;
          }
          x += pixels_to_justify;
        }
        pg_game_status.lastRenderedY = y - scrollY + line_height;
        i += charBytes;
      }
    }

    free(line);
    y += line_height;
    x = TEXT_MARGIN;
    if (y - scrollY + line_height > _Globals.windowHeight - HEADER_HEIGHT) {
      pg_game_status.textFullyRendered = 0;
      break;
    }
  }
}

void pg_renderText(const char* text, int x, int y, int width, int height, int scrollY)
{
  if (text_surface == NULL) {
    text_surface = SDL_CreateRGBSurface(0, width, height, 32, 0, 0, 0, 0);
    if (text_surface == NULL) {
      logWarning("Failed to create text surface");
      return;
    }
    SDL_FillRect(text_surface, NULL, SDL_MapRGBA(text_surface->format, _theme->backgroundColor.r, _theme->backgroundColor.g, _theme->backgroundColor.b, 0));
    pg_renderTextToSurface(text_surface, text, scrollY);
  }

  sdlBlitImage(text_surface, 0, HEADER_HEIGHT, 0, 0);
}

/* ---------------------------------------------------------------------------
 * Text substitution ("Text tricks")
 *
 *   ${expr}            -> value of expr
 *   $!{expr}           -> value of expr, first letter capitalized
 *   $!!{expr}          -> value of expr, all letters capitalized
 *   @{cond a|b}        -> a if cond is true, b if false
 *   @{num a|b|c...}    -> the num-th option (1-based)
 *   @!{...} / @!!{...} -> multireplace with capitalization
 *
 * Options of a multireplace can contain ${} substitutions.
 * ------------------------------------------------------------------------- */

struct pg_strbuf
{
  char *buf;
  size_t len;
  size_t cap;
} typedef pg_strbuf;

static void pg_sb_append(pg_strbuf *sb, const char *s, size_t n)
{
  if (!s || n == 0)
    return;

  if (sb->len + n + 1 > sb->cap) {
    size_t newCap = sb->cap ? sb->cap : 64;
    while (sb->len + n + 1 > newCap)
      newCap *= 2;
    char *tmp = realloc(sb->buf, newCap);
    if (tmp == NULL) {
      logWarning("Realloc failed");
      return;
    }
    sb->buf = tmp;
    sb->cap = newCap;
  }
  memcpy(sb->buf + sb->len, s, n);
  sb->len += n;
  sb->buf[sb->len] = '\0';
}

static char *pg_strndup(const char *s, size_t n)
{
  char *res = malloc(n + 1);
  if (res == NULL)
    return NULL;
  memcpy(res, s, n);
  res[n] = '\0';
  return res;
}

/// @brief Case insensitive compare of the first n chars
static int pg_strnicmp(const char *s1, const char *s2, size_t n)
{
  for (size_t i = 0; i < n; i++) {
    int c1 = tolower((unsigned char)s1[i]);
    int c2 = tolower((unsigned char)s2[i]);
    if (c1 != c2)
      return c1 < c2 ? -1 : 1;
    if (c1 == '\0')
      return 0;
  }
  return 0;
}

/// @brief Find the char closing the one at position open ('{' -> '}', '(' -> ')'), handling nesting
/// @return The position of the closing char or -1
static int pg_findClosing(const char *text, int open)
{
  const char openChar = text[open];
  const char closeChar = openChar == '{' ? '}' : ')';
  int depth = 0;
  int inQuote = 0;
  for (int i = open; text[i]; i++) {
    if (openChar == '(' && text[i] == '"' && (i == 0 || text[i - 1] != '\\')) {
      inQuote = !inQuote;
      continue;
    }
    if (inQuote)
      continue;
    if (text[i] == openChar) {
      depth++;
    } else if (text[i] == closeChar) {
      depth--;
      if (depth == 0)
        return i;
    }
  }
  return -1;
}

/// @brief Evaluate a boolean condition supporting parentheses, and, or, not
/// @param condition The condition
/// @return 1 if true, 0 if false
int pg_evaluateBoolean(const char *condition)
{
  if (!condition)
    return 0;

  char *cond = pg_strndup(condition, strlen(condition));
  trim(cond);

  // Remove parentheses wrapping the whole condition
  int len = strlen(cond);
  while (len > 1 && cond[0] == '(' && pg_findClosing(cond, 0) == len - 1) {
    memmove(cond, cond + 1, len - 2);
    cond[len - 2] = '\0';
    trim(cond);
    len = strlen(cond);
  }

  // not(...)
  if (len > 3 && pg_strnicmp(cond, "not", 3) == 0 && (cond[3] == '(' || isspace((unsigned char)cond[3]))) {
    int res = !pg_evaluateBoolean(cond + 3);
    free(cond);
    return res;
  }

  // Search a top level "and"/"or"
  int depth = 0;
  int inQuote = 0;
  for (int i = 0; i < len; i++) {
    if (cond[i] == '"' && (i == 0 || cond[i - 1] != '\\'))
      inQuote = !inQuote;
    if (inQuote)
      continue;
    if (cond[i] == '(') {
      depth++;
    } else if (cond[i] == ')') {
      depth--;
    } else if (depth == 0 && (isspace((unsigned char)cond[i]) || cond[i] == ')')) {
      int opLen = 0;
      int isAnd = 0;
      if (pg_strnicmp(&cond[i + 1], "and", 3) == 0 && (isspace((unsigned char)cond[i + 4]) || cond[i + 4] == '(')) {
        opLen = 3;
        isAnd = 1;
      } else if (pg_strnicmp(&cond[i + 1], "or", 2) == 0 && (isspace((unsigned char)cond[i + 3]) || cond[i + 3] == '(')) {
        opLen = 2;
      }

      if (opLen) {
        char *left = pg_strndup(cond, i);
        char *right = pg_strndup(&cond[i + 1 + opLen], len - i - 1 - opLen);
        int l = pg_evaluateBoolean(left);
        int r = pg_evaluateBoolean(right);
        free(left);
        free(right);
        free(cond);
        return isAnd ? (l && r) : (l || r);
      }
    }
  }

  int res = pg_evaluateSimpleCondition(cond);
  free(cond);
  return res;
}

static void pg_capitalize(char *text, int mode)
{
  if (!text || mode == 0)
    return;

  if (mode == 1) {
    // First letter only
    for (int i = 0; text[i]; i++) {
      if (!isspace((unsigned char)text[i])) {
        text[i] = toupper((unsigned char)text[i]);
        break;
      }
    }
  } else {
    for (int i = 0; text[i]; i++)
      text[i] = toupper((unsigned char)text[i]);
  }
}

/// @brief Get the 0-based index of the option to use in a multireplace
/// @param condition The multireplace condition (variable name or expression)
/// @return The index or -1
static int pg_multireplaceIndex(const char *condition)
{
  char opr[3];
  int isSimpleValue = indexOf(condition, '(', 0) < 0 &&
                      indexOf(condition, ' ', 0) < 0 &&
                      pg_get_operator(condition, opr) < 0;

  if (isSimpleValue) {
    char value[4096];
    pg_evaluateExpression(condition, value);
    trim(value);
    if (stricmp(value, "true") == 0)
      return 0;
    if (stricmp(value, "false") == 0)
      return 1;
    int intValue;
    if (strlen(value) > 0 && is_number(value, &intValue))
      return intValue - 1;

    logWarning("Invalid multireplace value: %s=%s", condition, value);
    return -1;
  }

  return pg_evaluateBoolean(condition) ? 0 : 1;
}

/// @brief Evaluate the content of a multireplace @{cond opt1|opt2|...}
/// @param content The text between the braces
/// @return The selected option (expanded). Must be freed.
static char *pg_multireplace(const char *content)
{
  int len = strlen(content);
  int start = 0;
  while (start < len && isspace((unsigned char)content[start]))
    start++;

  // Condition
  int condEnd;
  if (content[start] == '(') {
    condEnd = pg_findClosing(content, start);
    if (condEnd < 0) {
      logWarning("Multireplace: missing ')' in %s", content);
      return pg_strndup("", 0);
    }
    condEnd++;
  } else {
    condEnd = start;
    while (condEnd < len && !isspace((unsigned char)content[condEnd]))
      condEnd++;
  }
  char *condition = pg_strndup(content + start, condEnd - start);

  // Skip the separator between condition and options
  int optStart = condEnd;
  if (optStart < len && isspace((unsigned char)content[optStart]))
    optStart++;

  int index = pg_multireplaceIndex(condition);
  logDebug("\tMultireplace %s -> option %i", condition, index + 1);
  free(condition);

  if (index < 0) {
    return pg_strndup("", 0);
  }

  // Find the option (split on '|' outside nested braces)
  int depth = 0;
  int current = 0;
  int partStart = optStart;
  for (int i = optStart; i <= len; i++) {
    if (i < len && content[i] == '{') {
      depth++;
    } else if (i < len && content[i] == '}') {
      depth--;
    } else if (i == len || (content[i] == '|' && depth == 0)) {
      if (current == index) {
        char *option = pg_strndup(content + partStart, i - partStart);
        char *res = pg_expandVariables(option);
        free(option);
        return res;
      }
      current++;
      partStart = i + 1;
    }
  }

  logWarning("Multireplace: option %i not found in %s", index + 1, content);
  return pg_strndup("", 0);
}

char *pg_expandVariables(const char *text)
{
  if (!text)
    return NULL;

  pg_strbuf sb = { NULL, 0, 0 };

  int textLen = strlen(text);
  int i = 0;
  while (i < textLen) {
    if (text[i] == '$' || text[i] == '@') {
      // Capitalization: $!{} $!!{} @!{} @!!{}
      int j = i + 1;
      int caps = 0;
      while (j < textLen && text[j] == '!' && caps < 2) {
        caps++;
        j++;
      }

      if (j < textLen && text[j] == '{') {
        int close = pg_findClosing(text, j);
        if (close > j) {
          char *content = pg_strndup(text + j + 1, close - j - 1);
          char *replacement = NULL;
          if (text[i] == '$') {
            trim(content);
            char value[4096];
            pg_evaluateExpression(content, value);
            replacement = pg_strndup(value, strlen(value));
          } else {
            replacement = pg_multireplace(content);
          }
          pg_capitalize(replacement, caps);
          logDebug("\tReplaced %.*s -> %s", close - i + 1, text + i, replacement);

          if (replacement)
            pg_sb_append(&sb, replacement, strlen(replacement));
          free(replacement);
          free(content);
          i = close + 1;
          continue;
        } else {
          logWarning("Missing '}' in %s", text + i);
        }
      }
    }

    pg_sb_append(&sb, &text[i], 1);
    i++;
  }

  if (sb.buf == NULL)
    return pg_strndup("", 0);
  return sb.buf;
}

int pg_renderPageBreak()
{
  if (pg_game_status.textFullyRendered) {
    sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);

    int width;
    TTF_MeasureUTF8(_Globals.defaultFont, pg_game_status.pageBreakMessage, _Globals.windowWidth, &width, NULL);
    sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, pg_game_status.pageBreakMessage, _Globals.windowWidth - 35 - width - 5, _Globals.windowHeight - 28, 0, 0);

    return 1;
  }

  return 0;
}

/// @brief Exec the next line(s)
/// @return 1 if something was executed
int pg_exec_primitive()
{
  char line[4096];
  int indentation = -1;

  if(pg_game_status.text != NULL) {
    free(pg_game_status.text);
    pg_game_status.text = NULL;
  }
  if (text_surface) {
    SDL_FreeSurface(text_surface);
    text_surface = NULL;
  }

  pg_game_status.showPageBreak = 0;
  pg_game_status.scrollY = 0;
  pg_clearChoices();

  if (pg_game_status.fpPosition == pg_game_status.fpSize) {
    logDebug("Reached EOF (file size=%i), going to the next scene", pg_game_status.fpSize);
    pg_goToNextScene();

    return 0;
  }

  int textStarted = 0;
  while (fgets(line, sizeof(line), pg_game_status.fp)) {
    logDebug("line=%s", line);
    int lineIndentation = isEmpty(line)
      ? indentation
      : getIndentation(line);
    if (indentation < 0)
      indentation = lineIndentation;

    if (lineIndentation < indentation) {
      // Indentation changed, check if
      if (pg_game_status.ifInfoCount > 0) {
        trim(line);
        if_info *ifInfo = &pg_game_status.ifInfo[pg_game_status.ifInfoCount - 1];
        int if_ended = pg_game_status.fpPosition == ifInfo->end_position || startsWith("*else", line) || startsWith("*elseif", line);
        if (ifInfo->indentation == lineIndentation && if_ended) {
          indentation = ifInfo->indentation;
          fseek(pg_game_status.fp, ifInfo->end_position, SEEK_SET);

          ifInfo->indentation = 0;
          ifInfo->end_position = 0;
          pg_game_status.ifInfoCount--;

          continue;
        }
      }
    }

    if (isComment(line))
      continue;

    trim(line);
    int lineLen = strlen(line);
    if (line[0] == '*') {
      // Command
      char command[256];
      pg_getCommand(line, command);
      logDebug("Command [%s]=%s", command, line);
      if (stricmp(command, "label") == 0) {
        // We can skip labels
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }
      if (stricmp(command, "params") == 0) {
        // We can skip params because we create the temporary variable
        // when executing the gosub command
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "bug") == 0) {
        logWarning("Bug found (%i)", pg_game_status.fpPosition);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "check_purchase") == 0) {
        // check_purchase name1 name2
        pg_manageCheckPurchaseCommand(line);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "page_break") == 0) {
        pg_game_status.showPageBreak = 1;
        strcpy(pg_game_status.pageBreakMessage, "Next");
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        break;
      }

      if (stricmp(command, "line_break") == 0) {
        pg_appendText("\n", 0);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "text_image") == 0 || stricmp(command, "image") == 0) {
        // Add the command to the text as it should be rendered with it
        pg_appendText(line, 1);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "finish") == 0) {
        pg_manageFinishCommand(line);
        pg_game_status.showPageBreak = 1;
        pg_game_status.fpPosition = pg_game_status.fpSize;
        break;
      }

      if (stricmp(command, "goto") == 0) {
        // goto
        long newPosition = pg_manageGotoCommand(line);
        logDebug("%s -> %i", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        indentation = -1;

        if (textStarted)
          pg_appendText("\n", 0);
        textStarted = 0;
        continue;
      }

      if (stricmp(command, "gosub") == 0) {
        //gosub
        long newPosition = pg_manageGosubCommand(line);
        logDebug("%s -> %i", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        indentation = -1;

        if (textStarted)
          pg_appendText("\n", 0);
        textStarted = 0;
        continue;
      }

      if (stricmp(command, "gosub_scene") == 0) {
        //gosub_scene
        long newPosition = pg_manageGosubSceneCommand(line);
        logDebug("%s -> %i", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        indentation = -1;

        if (textStarted)
          pg_appendText("\n", 0);
        textStarted = 0;
        continue;
      }

      if (stricmp(command, "return") == 0) {
        if (pg_game_status.gosubCount == 0) {
          logWarning("No previous position found");
          continue;
        }

        gosub_info *info = &pg_game_status.gosub[pg_game_status.gosubCount];
        long newPosition = info->position;
        pg_game_status.fpPosition = newPosition;
        if (info->fp != NULL) {
          fclose(pg_game_status.fp);
          pg_game_status.fp = info->fp;
          pg_clear_labels();
        }
        fseek(pg_game_status.fp, pg_game_status.fpPosition, SEEK_SET);
        pg_game_status.gosubCount--;
        indentation = -1;
        logDebug("%s -> %i", line, newPosition);

        info->position = 0;
        info->fp = NULL;

        // Clear params temp variables
        for (int i = 0; i < pg_game_status.tempVariablesCount; i++) {
          if (startsWith("param_", pg_game_status.tempVariables[i].name)) {
            if (pg_delete_temp_var(i)) {
              i--;
            }
          }
        }
        continue;
      }

      if (stricmp(command, "set") == 0) {
        // set varName expression
        int idx = 5;
        int wordEnd = pg_indexOfWordEnd(line, idx);
        if (wordEnd < 0) {
          logWarning("Missing variable name");
          continue;
        }
        char varName[256];
        char value[512];
        subString(line, varName, idx, wordEnd - idx);
        idx += strlen(varName);
        subString(line, value, idx + 1, lineLen - idx);
        pg_setVarValue(varName, value);
        continue;
      }

      if (stricmp(command, "if") == 0) {
        // if
        long end_pos;
        long newPosition = pg_manageIfCommand(line, lineIndentation, &end_pos);
        logDebug("%s -> %i", line, newPosition);

        pg_game_status.ifInfo[pg_game_status.ifInfoCount].indentation = lineIndentation;
        pg_game_status.ifInfo[pg_game_status.ifInfoCount].end_position = end_pos;
        pg_game_status.ifInfoCount++;

        pg_game_status.fpPosition = newPosition;
        fseek(pg_game_status.fp, pg_game_status.fpPosition, SEEK_SET);
        indentation = -1;
        continue;
      }

      if (stricmp(command, "temp") == 0) {
        // temp varName value
        pg_manageTempCommand(line);
        continue;
      }

      if (stricmp(command, "choice") == 0 || stricmp(command, "fake_choice") == 0) {
        // choice/fake_choice
        long newPosition = pg_manageChoiceCommand();
        logDebug("Choice -> %i", newPosition);
        pg_game_status.fpPosition = newPosition;
        break;
      }

      logWarning("Unknown command: %s", command);
    } else {
      // Text
      if (lineLen) {
        // Expand variables now: following *set commands could change their values
        char *expanded = pg_expandVariables(line);
        const char *textLine = expanded ? expanded : line;
        if (pg_needsNewLine(textLine) && !endsWith("\n", pg_game_status.text))
          pg_appendText("\n", 0);
        pg_appendText(textLine, 0);
        free(expanded);
        textStarted = 1;
      } else if(textStarted) {
        // New paragraph
        pg_appendText("\n\n", 0);
      }
      pg_game_status.fpPosition = ftell(pg_game_status.fp);
    }
  }
  if (ftell(pg_game_status.fp) == pg_game_status.fpSize) {
    pg_game_status.fpPosition = pg_game_status.fpSize;
  }

  if (pg_game_status.text != NULL) {
    // Remove trailing new lines (except one)
    int end = strlen(pg_game_status.text) - 1;
    for (int i = end; i >= 0; i--) {
      if (pg_game_status.text[i] == '\n') {
        pg_game_status.text[i] = '\0';
      } else {
        break;
      }
    }
  }

  if (pg_game_status.text == NULL && pg_game_status.fpPosition == pg_game_status.fpSize) {
    logDebug("Reached EOF (file size=%i), going to the next scene", pg_game_status.fpSize);
    pg_goToNextScene();

    return 0;
  } else {
    logDebug("Text to render:\n%s", pg_game_status.text);
  }

  return 1;
}

void pg_exec()
{
  while (!pg_exec_primitive());
}

void pg_appendText(const char *text, int newLine)
{
  if (!text)
    return;

  const int textLen = strlen(text);
  const int delta = newLine ? 2 : 1;
  if (pg_game_status.text == NULL) {
    pg_game_status.text = malloc(textLen + delta);
    memset(pg_game_status.text, 0, textLen + delta);
  } else {
    char *tmp = realloc(pg_game_status.text, strlen(pg_game_status.text) + textLen + delta);
    if (tmp != NULL) {
      memset(tmp + strlen(tmp), 0, textLen + delta);
      pg_game_status.text = tmp;
      tmp = NULL;
    } else {
      logWarning("Realloc failed");
      pg_game_status.text = NULL;
      return;
    }
  }

  strncat(pg_game_status.text, text, textLen);
  if (newLine) {
    strcat(pg_game_status.text, "\n");
  }
}

int pg_indexOfWordEnd(const char *string, const int startFrom)
{
  if (!string)
      return -1;

  int idx = startFrom;
  while (u8nextchar(string, &idx)) {
    if (string[idx] == ' ' || string[idx] == '\t' || string[idx] == '\n' || string[idx] == ',' ||
        string[idx] == '.' || string[idx] == ';' || string[idx] == '!' || string[idx] == '?') {
      return idx;
    }
  }

  return -1;
}

void pg_getCommand(const char *line, char* command)
{
  if (!line || !command)
    return;

  int idx = 0;
  int lineLen = strlen(line);
  if (lineLen > 0 && line[0] == '*') {
    for (int i = 1; i < lineLen; i++) {
      if (line[i] == ' ' || line[i] == '\t')
        break;
      command[idx++] = line[i];
    }
  }
  command[idx] = '\0';
}

int pg_needsNewLine(const char *text)
{
  if (!text)
    return 0;

  // Bullet point
  if (strlen(text) > 2 && text[0] == -30 && text[1] == -128 && text[2] == -94)
    return 1;
  return 0;
}

void pg_goToNextScene()
{
  if (pg_game_status.sceneIndex + 1 >= runningGame->scenesCount) {
    logWarning("No next scene");
    return;
  }

  pg_game_status.sceneIndex++;
  pg_game_status.fpPosition = 0;
  pg_clear_labels();
  pg_clearTempVars();

  char filePath[256];
  strcpy(filePath, runningGame->path);
  strcat(filePath, "/scenes/");
  strcat(filePath, runningGame->scenes[pg_game_status.sceneIndex]);
  strcat(filePath, ".txt");

  if (pg_game_status.fp) {
    fclose(pg_game_status.fp);
    pg_game_status.fp = NULL;
  }
  pg_clear_image_cache();
  pg_game_status.fp = fopen(filePath, "rb");
  if (!pg_game_status.fp) {
    logError("Failed to open file %s", filePath);
    return;
  }
  logInfo("Opened file %s", filePath);
  pg_clear_labels();

  fseek(pg_game_status.fp, 0L, SEEK_END);
  pg_game_status.fpSize = ftell(pg_game_status.fp);
  fseek(pg_game_status.fp, 0L, SEEK_SET);
  pg_game_status.fpPosition = 0;
}

int pg_manageImageCommand(SDL_Surface *surface, const char *text, int y)
{
  char imageName[255];
  char alignment[64];

  int start = indexOfWordBreak(text, 0);
  if (start < 0)
    return 0;

  int idx = indexOfWordBreak(text, start + 1);
  if (idx < 0)
    return 0;
  subString(text, imageName, start + 1, idx - start - 1);
  start = idx;
  idx = indexOfWordBreak(text, idx + 1);
  if (idx < 0)
    return 0;
  subString(text, alignment, start + 1, idx - start - 1);

  image_cache_item *image = pg_get_image_from_cache(imageName);
  if (image && image->surface) {
    SDL_Rect destRect = { TEXT_MARGIN, y, image->w, image->h };

    float ratio = (float)destRect.w / (float)destRect.h;
    if (destRect.w > _Globals.windowWidth - 2 * TEXT_MARGIN) {
      destRect.w = _Globals.windowWidth - 2 * TEXT_MARGIN;
      destRect.h = destRect.w / ratio;
    }
    if (destRect.h > _Globals.windowHeight - 2 * HEADER_HEIGHT - 2 * TEXT_MARGIN) {
      destRect.h = _Globals.windowHeight - 2 * HEADER_HEIGHT - 2 * TEXT_MARGIN;
      destRect.w = destRect.h * ratio;
    }

    SDL_Rect srcRect = { 0, 0, image->w, image->h };

    // Always return multiple of line_height to not break text scrolling
    int res = destRect.h + TEXT_MARGIN;
    int diff = res % line_height;
    if (diff)
      res += line_height - diff;

    if (y < 0) {
      int deltaY = -y;
      if (deltaY >= destRect.h) {
        return res;
      }

      int srcDeltaY = srcRect.h * deltaY / destRect.h;
      srcRect.y += srcDeltaY;
      srcRect.h -= srcDeltaY;

      destRect.y = 0;
      destRect.h -= deltaY;
    }

    if (y + destRect.h > surface->h) {
      int deltaY = y + destRect.h - surface->h;
      int srcDeltaY = srcRect.h * deltaY / destRect.h;
      srcRect.h -= srcDeltaY;
      destRect.h -= deltaY;
    }

    int deltaX = surface->w - destRect.w;
    if (deltaX > 0) {
      if (stricmp(alignment, "center") == 0) {
        destRect.x += deltaX / 2;
      } else if (stricmp(alignment, "right") == 0) {
        destRect.x = deltaX;
      }
    }

    if (destRect.h > 0) {
      SDL_UpperBlitScaled(image->surface, &srcRect, surface, &destRect);
    }
    return res;
  }
  return 0;
}

long pg_manageIfCommand(const char *text, int indentation, long *endPosition)
{
  char condition[4096];
  trim(condition);
  int textLen = strlen(text);
  subString(text, condition, 3, textLen - 3);
  trim(condition);
  textLen = strlen(text);

  *endPosition = pg_get_if_end(pg_game_status.fp, indentation);
  logDebug("if condition=%s, indentation=%i, end position=%li", condition, indentation, *endPosition);

  if (pg_evaluateCondition(condition, indentation)) {
    return ftell(pg_game_status.fp);
  } else {
    // Search for elseif, else or end
    char line[4096];

    while (fgets(line, sizeof(line), pg_game_status.fp)) {
      line[strcspn(line, "\r")] = '\0';
      line[strcspn(line, "\n")] = '\0';

      int lineIndentation = getIndentation(line);
      if (lineIndentation < indentation) {
        return *endPosition;
      }

      if (lineIndentation == indentation) {
        trim(line);
        if (stricmp(line, "*else") == 0) {
          return ftell(pg_game_status.fp);
        } else if (startsWith("*elseif", line)) {
          subString(line, condition, 8, strlen(line) - 8 + 1);
          if (pg_evaluateCondition(condition, indentation)) {
            return ftell(pg_game_status.fp);
          }
        } else {
          return *endPosition;
        }
      }
    }
  }

  logWarning("If not managed: %s", text);
  return *endPosition;
}

int pg_get_operator(const char *condition, char *operator)
{
  // Operators: ['=', '!=', '>', '>=', '<', '<=']
  operator[0] = '\0';
  int len = strlen(condition);
  for (int i = 0; i < len; i++) {
    if (condition[i] == '!' && i + 1 < len && condition[i + 1] == '=') {
      strcpy(operator, "!=");
      return i;
    }

    if (condition[i] == '>' && i + 1 < len && condition[i + 1] == '=') {
      strcpy(operator, ">=");
      return i;
    }

    if (condition[i] == '<' && i + 1 < len && condition[i + 1] == '=') {
      strcpy(operator, "<=");
      return i;
    }

    if (condition[i] == '>') {
      strcpy(operator, ">");
      return i;
    }

    if (condition[i] == '<') {
      strcpy(operator, "<");
      return i;
    }

    if (condition[i] == '=') {
      strcpy(operator, "=");
      return i;
    }
  }

  return -1;
}

int pg_evaluateCondition(const char *condition, int indentation)
{
  // Supports parentheses, and, or, not
  return pg_evaluateBoolean(condition);
}

int pg_evaluateSimpleCondition(const char *condition)
{
  char operator[3];
  int res;

  int opIdx = pg_get_operator(condition, operator);
  if (opIdx < 0) {
    // just a variable name (boolean condition)
    variable *var = pg_getVar(condition);
    if (var == NULL) {
      return 0;
    }
    res = var->value != NULL && stricmp(var->value, "true") == 0;
    logDebug("\tCondition (%s) -> %s", condition, res ? "true" : "false");
    return res;
  }

  // expr OPT expr
  char left_expression[1024];
  subString(condition, left_expression, 0, opIdx);
  trim(left_expression);
  char right_expression[1024];
  const int opLen = strlen(operator);
  subString(condition, right_expression, opIdx + opLen, strlen(condition) - opIdx - opLen);
  trim(right_expression);

  char left_value[1024];
  char right_value[1024];

  pg_evaluateExpression(left_expression, left_value);
  int int_value;
  if (is_number(left_value, &int_value)) {
    sprintf(left_value, "%010i", int_value);
  }
  pg_evaluateExpression(right_expression, right_value);
  if (is_number(right_value, &int_value)) {
    sprintf(right_value, "%010i", int_value);
  }

  logDebug("\tleft=%s (%s), right=%s (%s), op='%s'", left_expression, left_value, right_expression, right_value, operator);
  if (stricmp(operator, "=") == 0) {
    res = stricmp(left_value, right_value) == 0 ? 1 : 0;
  } else if (stricmp(operator, "!=") == 0) {
    res = stricmp(left_value, right_value) != 0 ? 1 : 0;
  } else if (stricmp(operator, ">") == 0) {
    res = stricmp(left_value, right_value) > 0 ? 1 : 0;
  } else if (stricmp(operator, ">=") == 0) {
    res = stricmp(left_value, right_value) >= 0 ? 1 : 0;
  } else if (stricmp(operator, "<") == 0) {
    res = stricmp(left_value, right_value) < 0 ? 1 : 0;
  } else if (stricmp(operator, "<=") == 0) {
    res = stricmp(left_value, right_value) <= 0 ? 1 : 0;
  } else {
    logWarning("\tUnknown operator: %s", operator);
    return 0;
  }

  logDebug("\tCondition (%s) -> %s", condition, res ? "true" : "false");
  return res;
}

/* ---------------------------------------------------------------------------
 * Arithmetic expressions: numbers, variables, parentheses, + - * / modulo
 * ------------------------------------------------------------------------- */

static int pg_arithExpr(const char **p, double *out);

static void pg_arithSkipSpaces(const char **p)
{
  while (**p && isspace((unsigned char)**p))
    (*p)++;
}

static int pg_arithFactor(const char **p, double *out)
{
  pg_arithSkipSpaces(p);
  if (**p == '(') {
    (*p)++;
    if (!pg_arithExpr(p, out))
      return 0;
    pg_arithSkipSpaces(p);
    if (**p != ')')
      return 0;
    (*p)++;
    return 1;
  }

  if (**p == '-') {
    (*p)++;
    if (!pg_arithFactor(p, out))
      return 0;
    *out = -*out;
    return 1;
  }

  if (isdigit((unsigned char)**p) || **p == '.') {
    char *end;
    *out = strtod(*p, &end);
    *p = end;
    return 1;
  }

  if (isalpha((unsigned char)**p) || **p == '_') {
    char name[256];
    int len = 0;
    while ((isalnum((unsigned char)**p) || **p == '_') && len < (int)sizeof(name) - 1) {
      name[len++] = **p;
      (*p)++;
    }
    name[len] = '\0';

    variable *var = pg_getVar(name);
    if (var == NULL || var->value == NULL)
      return 0;
    char *end;
    *out = strtod(var->value, &end);
    if (end == var->value) {
      logWarning("Variable %s is not a number (%s)", name, var->value);
      return 0;
    }
    return 1;
  }

  return 0;
}

static int pg_arithTerm(const char **p, double *out)
{
  if (!pg_arithFactor(p, out))
    return 0;

  while (1) {
    pg_arithSkipSpaces(p);
    char op = **p;
    if (op == '*' || op == '/') {
      (*p)++;
    } else if (pg_strnicmp(*p, "modulo", 6) == 0 && !isalnum((unsigned char)(*p)[6]) && (*p)[6] != '_') {
      op = '%';
      *p += 6;
    } else {
      return 1;
    }

    double right;
    if (!pg_arithFactor(p, &right))
      return 0;
    if (op == '*') {
      *out *= right;
    } else if (right == 0) {
      logWarning("Division by zero");
      return 0;
    } else if (op == '/') {
      *out /= right;
    } else {
      *out = fmod(*out, right);
    }
  }
}

static int pg_arithExpr(const char **p, double *out)
{
  if (!pg_arithTerm(p, out))
    return 0;

  while (1) {
    pg_arithSkipSpaces(p);
    char op = **p;
    if (op != '+' && op != '-')
      return 1;
    (*p)++;

    double right;
    if (!pg_arithTerm(p, &right))
      return 0;
    *out = op == '+' ? *out + right : *out - right;
  }
}

/// @brief Evaluate an arithmetic expression
/// @param expression The expression
/// @param result The result
/// @return 1 on success
int pg_evaluateArithmetic(const char *expression, double *result)
{
  const char *p = expression;
  if (!pg_arithExpr(&p, result))
    return 0;
  pg_arithSkipSpaces(&p);
  return *p == '\0';
}

int pg_evaluateExpression(const char *expression, char *value)
{
  value[0] = '\0';
  if (!expression)
    return 0;

  int expLen = strlen(expression);
  if (expLen > 1 && expression[0] == '"' && expression[expLen - 1] == '"') {
    // String constant (can contain ${} substitutions)
    subString(expression, value, 1, expLen - 2);
    if (indexOf(value, '{', 0) >= 0) {
      char *expanded = pg_expandVariables(value);
      if (expanded) {
        strncpy(value, expanded, 1023);
        value[1023] = '\0';
        free(expanded);
      }
    }

    return 1;
  }

  int int_value;
  if (expLen > 2 && expression[0] == '%' && (expression[1] == '+' || expression[1] == '-')) {
    if (is_number(expression + 2, &int_value)) {
      // Fairmath: %+10
      strcpy(value, expression);
      return 1;
    } else {
      // Fairmath: %+var_name
      variable *var = pg_getVar(expression + 2);
      if (var != NULL) {
        strncpy(value, expression, 2);
        strcat(value, var->value);

        return 1;
      }
    }
  }

  if (is_number(expression, &int_value)) {
    // Number constant
    strcpy(value, expression);
    return 1;
  }

  // Boolean value
  if (stricmp(expression, "true") == 0 || stricmp(expression, "false") == 0) {
    strcpy(value, expression);
    return 1;
  }

  // Arithmetic expression: 3 * (var + 1)
  if (strpbrk(expression, "+-*/()") != NULL || strstr(expression, " modulo ") != NULL) {
    double result;
    if (pg_evaluateArithmetic(expression, &result)) {
      if (fabs(result - round(result)) < 1e-9)
        sprintf(value, "%li", (long)round(result));
      else
        sprintf(value, "%g", result);
      return 1;
    }
  }

  // Variable name
  variable *var = pg_getVar(expression);
  if (var != NULL) {
    strcpy(value, var->value);
    return 1;
  }

  logWarning("Expression not evaluated: %s", expression);
  return 0;
}

long pg_manageGotoCommand(const char *text)
{
  int start = indexOfWordBreak(text, 0);
  if (start < 0) {
    logWarning("goto without label name");
    return ftell(pg_game_status.fp);
  }

  char labelName[256];
  subString(text, labelName, start + 1, strlen(text) - start - 1);

  label *lbl = pg_get_label(labelName);
  if (lbl == NULL) {
    logWarning("Label %s not found", labelName);
    return ftell(pg_game_status.fp);
  }
  fseek(pg_game_status.fp, lbl->position, SEEK_SET);
  return lbl->position;
}

long pg_manageGosubCommand(const char *text)
{
  int start = indexOfWordBreak(text, 0);
  if (start < 0) {
    logWarning("gosub without label name");
    return pg_game_status.fpPosition;
  }

  label *lbl = NULL;
  int end;
  int word_count = 0;
  while ((end = indexOfWordBreak(text, start + 1)) >= 0) {
    char *par_value = malloc(end - start + 1);
    subString(text, par_value, start + 1, end - start - 1);
    start = end;

    if (word_count == 0) {
      // label name
      lbl = pg_get_label(par_value);
      if (lbl == NULL) {
        logWarning("Label %s not found", par_value);
        return pg_game_status.fpPosition;
      }
    } else {
      // parameter
      variable *var = pg_add_temp_var();
      var->name = malloc(32);
      sprintf(var->name, "param_%i", word_count);
      var->type = get_var_type(par_value);
      setVarValue(var, par_value);
    }

    free(par_value);
    word_count++;
  }

  if (word_count - 1 >= 0) {
    variable *var = pg_add_temp_var();
    var->name = malloc(32);
    strcpy(var->name, "param_count");
    var->type = number;
    char count[11];
    sprintf(count, "%i", word_count - 1);
    setVarValue(var, count);
  }

  pg_game_status.gosubCount++;
  gosub_info *gosub = &pg_game_status.gosub[pg_game_status.gosubCount];
  gosub->fp = NULL;
  gosub->position = ftell(pg_game_status.fp);
  fseek(pg_game_status.fp, lbl->position, SEEK_SET);

  return lbl->position;
}

long pg_manageGosubSceneCommand(const char *text)
{
  int start = indexOfWordBreak(text, 0);
  if (start < 0) {
    logWarning("gosub_scene without scene name");
    return pg_game_status.fpPosition;
  }

  char label_name[256];
  FILE *fp = NULL;
  int end;
  int word_count = 0;
  while ((end = indexOfWordBreak(text, start + 1)) >= 0) {
    char *par_value = malloc(end - start + 1);
    subString(text, par_value, start + 1, end - start - 1);
    start = end;

    logDebug("par_value=%s", par_value);
    if (word_count == 0) {
      // scene name
      char filePath[256];
      strcpy(filePath, runningGame->path);
      strcat(filePath, "/scenes/");
      strcat(filePath, par_value);
      strcat(filePath, ".txt");

      fp = fopen(filePath, "rb");
      if (fp == NULL) {
        logError("Failed to open file %s", filePath);
      }
      logInfo("Opened file %s", filePath);
    } else if (word_count == 1) {
      // label name
      strcpy(label_name, par_value);
    } else {
      // parameter
      variable *var = pg_add_temp_var();
      var->name = malloc(32);
      sprintf(var->name, "param_%i", word_count);
      var->type = get_var_type(par_value);
      setVarValue(var, par_value);
    }

    free(par_value);
    word_count++;
  }

  if (word_count - 2 >= 0) {
    variable *var = pg_add_temp_var();
    var->name = malloc(32);
    strcpy(var->name, "param_count");
    var->type = number;
    char count[11];
    sprintf(count, "%i", word_count - 2);
    setVarValue(var, count);
  }

  pg_game_status.gosubCount++;
  gosub_info *gosub = &pg_game_status.gosub[pg_game_status.gosubCount];
  gosub->fp = pg_game_status.fp;
  gosub->position = ftell(pg_game_status.fp);

  pg_game_status.fp = fp;
  pg_clear_labels();

  label *lbl = NULL;
  if (strlen(label_name)) {
    lbl = pg_get_label(label_name);
    if (lbl == NULL) {
      logWarning("Label %s not found", label_name);
    }
  }
  fseek(pg_game_status.fp, lbl == NULL ? 0 : lbl->position, SEEK_SET);

  return lbl == NULL ? 0 : lbl->position;
}

void pg_manageTempCommand(const char *text)
{
  int idx = indexOfWordBreak(text, 6);

  variable *var = pg_add_temp_var();
  var->name = malloc(idx - 6 + 1);
  subString(text, var->name, 6, idx - 6);
  var->value = NULL;

  char *tempValue = malloc(strlen(text) - idx + 1);
  subString(text, tempValue, idx + 1, strlen(text) - idx);
  trim(tempValue);
  var->type = get_var_type(tempValue);
  setVarValue(var, tempValue);
  free(tempValue);

  logDebug("\tTEMPVAR %s=%s (type %s)", var->name, var->value, var->type == string ? "string" : var->type == boolean ? "boolean" : "number");
}

long pg_manageChoiceCommand()
{
  char line[4096];
  int choiceIndentation = -1;
  int ifIndentation = -1;
  int lineStartPosition = 0;
  long nextLineStartPosition = 0;

  pg_game_status.choicesCount = 0;
  while (fgets(line, sizeof(line), pg_game_status.fp)) {
    lineStartPosition = nextLineStartPosition;
    nextLineStartPosition = ftell(pg_game_status.fp);
    line[strcspn(line, "\r")] = '\0';
    line[strcspn(line, "\n")] = '\0';

    if (choiceIndentation < 0) {
      choiceIndentation = getIndentation(line);
    }

    int lineIndentation = getIndentation(line);
    if (strlen(line) > 0 && lineIndentation < choiceIndentation) {
      break;
    }

    trim(line);
    if (lineIndentation == choiceIndentation || lineIndentation == ifIndentation) {
      // Option modifiers: *hide_reuse, *disable_reuse, *allow_reuse, *selectable_if (cond), *if (cond)
      int start = 0;
      int lineLen = strlen(line);
      int reusable = runningGame->hide_reuse ? 0 : 1;
      int disableReuse = 0;
      int selectable = 1;
      int visible = 1;
      int isBlockIf = 0;
      int unsupported = 0;
      while (start < lineLen && line[start] == '*') {
        char command[256];
        pg_getCommand(line + start, command);
        int next = start + 1 + strlen(command);
        while (next < lineLen && isspace((unsigned char)line[next]))
          next++;

        if (stricmp(command, "hide_reuse") == 0) {
          reusable = 0;
          disableReuse = 0;
        } else if (stricmp(command, "disable_reuse") == 0) {
          reusable = 1;
          disableReuse = 1;
        } else if (stricmp(command, "allow_reuse") == 0) {
          reusable = 1;
          disableReuse = 0;
        } else if (stricmp(command, "selectable_if") == 0 || stricmp(command, "if") == 0) {
          // The condition must be enclosed in parentheses
          int condEnd = line[next] == '(' ? pg_findClosing(line, next) : -1;
          if (condEnd < 0 || indexOf(line, '#', condEnd) < 0) {
            if (stricmp(command, "if") == 0)
              isBlockIf = 1;
            else
              unsupported = 1;
            break;
          }

          char *condition = pg_strndup(line + next, condEnd - next + 1);
          int res = pg_evaluateCondition(condition, choiceIndentation);
          logDebug("\t*%s %s -> %s", command, condition, res ? "true" : "false");
          free(condition);
          if (stricmp(command, "if") == 0) {
            visible = visible && res;
          } else {
            selectable = selectable && res;
          }

          next = condEnd + 1;
          while (next < lineLen && isspace((unsigned char)line[next]))
            next++;
        } else {
          unsupported = 1;
          break;
        }
        start = next;
      }

      if (isBlockIf) {
        // *if block containing options
        long end_pos;
        long new_pos = pg_manageIfCommand(line, choiceIndentation, &end_pos);
        logDebug("%s -> %i", line, new_pos);
        fseek(pg_game_status.fp, new_pos, SEEK_SET);
        // Get indentation of the next line
        fgets(line, sizeof(line), pg_game_status.fp);
        ifIndentation = getIndentation(line);
        fseek(pg_game_status.fp, new_pos, SEEK_SET);
        nextLineStartPosition = new_pos;
        continue;
      }

      if (unsupported) {
        char command[256];
        pg_getCommand(line + start, command);
        logWarning("Unsupported command in choice: %s", command);
        continue;
      }

      if (start < lineLen && line[start] == '#') {
        if (!visible) {
          logDebug("\tChoice hidden: %s", line);
          continue;
        }

        pg_game_status.choicesCount++;
        if (pg_game_status.choices == NULL) {
          pg_game_status.choices = malloc(sizeof(choice) * pg_game_status.choicesCount);
        } else {
          choice *tmp = realloc(pg_game_status.choices, sizeof(choice) * pg_game_status.choicesCount);
          if (tmp != NULL) {
            pg_game_status.choices = tmp;
            tmp = NULL;
          } else {
            logWarning("Realloc failed");
            pg_game_status.choicesCount--;
            continue;
          }
        }

        choice *choice = &pg_game_status.choices[pg_game_status.choicesCount - 1];

        start++;
        choice->reusable = reusable;
        choice->disable_reuse = disableReuse;
        choice->selectable = selectable;
        choice->text = malloc(strlen(line) - start + 1);
        choice->position = ftell(pg_game_status.fp);
        sprintf(choice->id, "%i_%li", pg_game_status.sceneIndex, choice->position);
        choice->used = 0;
        for (int i = 0; i < pg_game_status.used_choices_count; i++) {
          if (strcmp(choice->id, pg_game_status.used_choices[i]) == 0) {
            choice->used = 1;
            break;
          }
        }
        subString(line, choice->text, start, strlen(line) - start);
        char *expanded = pg_expandVariables(choice->text);
        if (expanded) {
          free(choice->text);
          choice->text = expanded;
        }
        logDebug("\tChoice: %s (pos=%li, reusable=%i, selectable=%i)", choice->text, choice->position, choice->reusable, choice->selectable);
      }
    }
  }

  if (pg_game_status.choicesCount == 0) {
    logWarning("Empty choice");
  }
  return lineStartPosition;
}

void pg_manageFinishCommand(const char *text)
{
  int lineLen = strlen(text);
  if (lineLen > 7) {
    if (lineLen - 8 > sizeof(pg_game_status.pageBreakMessage)) {
      logWarning("Finish message too long");
      strcpy(pg_game_status.pageBreakMessage, "Next");
    } else {
      subString(text, pg_game_status.pageBreakMessage, 8, lineLen - 8);
    }
  } else {
    strcpy(pg_game_status.pageBreakMessage, "Next Chapter");
  }
}

void pg_manageCheckPurchaseCommand(const char *text)
{
  int start = 16;
  int end;
  while ((end = indexOfWordBreak(text, start)) >= 0) {
    char word[256];
    subString(text, word, start, end - start);
    start = end + 1;

    char var_name[300];
    sprintf(var_name, "choice_purchased_%s", word);
    // Always se the var value to "true"
    addGameVar(runningGame, boolean, var_name, "true");
    logDebug("check_purchase %s=%s", var_name, "true");
  };
}

void pg_clear_image_cache()
{
  for (int i = 0; i < image_cache_count; i++) {
    free(image_cache[i].name);
    image_cache[i].name = NULL;
    SDL_FreeSurface(image_cache[i].surface);
    image_cache[i].surface = NULL;
    image_cache[i].w = 0;
    image_cache[i].h = 0;
  }
  image_cache_count = 0;
}

image_cache_item* pg_get_image_from_cache(const char *name)
{
  for (int i = 0; i < image_cache_count; i++) {
    if (stricmp(image_cache[i].name, name) == 0)
      return &image_cache[i];
  }

  return pg_add_image_to_cache(name);
}

image_cache_item *pg_add_image_to_cache(const char *name)
{
  if (image_cache_count + 1 >= sizeof(image_cache) / sizeof(image_cache_item)) {
    logWarning("Cannot add image to cache (cache full)");
    return NULL;
  }

  if (image_cache[image_cache_count].name != NULL) {
    free(image_cache[image_cache_count].name);
    image_cache[image_cache_count].name = NULL;
  }
  if (image_cache[image_cache_count].surface != NULL) {
    SDL_FreeSurface(image_cache[image_cache_count].surface);
    image_cache[image_cache_count].surface = NULL;
  }

  image_cache[image_cache_count].name = malloc(strlen(name) + 1);
  strcpy(image_cache[image_cache_count].name, name);

  char filePath[256];
  strcpy(filePath, runningGame->path);
  strcat(filePath, "/");
  strcat(filePath, name);
  SDL_Surface* surface = IMG_Load(filePath);
  if (surface == NULL) {
    logWarning("Failed to load image from %s", filePath);
    return NULL;
  }

  image_cache[image_cache_count].w = surface->w;
  image_cache[image_cache_count].h = surface->h;
  image_cache[image_cache_count].surface = surface;
  return &image_cache[image_cache_count++];
}

label* pg_get_label(const char *labelName)
{
  if (pg_game_status.labelsCount < 0)
    pg_read_labels();

  int i = 0;
  while (i < pg_game_status.labelsCount) {
    if (stricmp(pg_game_status.labels[i].name, labelName) == 0)
      return &pg_game_status.labels[i];
    i++;
  }
  return NULL;
}

variable* pg_getVar(const char *varName)
{
  int i = 0;
  for (i = 0; i < pg_game_status.tempVariablesCount; i++) {
    if (stricmp(pg_game_status.tempVariables[i].name, varName) == 0) {
      return &pg_game_status.tempVariables[i];
    }
  }

  return getGameVar(varName, runningGame);
}

int pg_setVarValue(const char *varName, const char *value)
{
  variable *var = pg_getVar(varName);
  if (var == NULL)
    return 0;

  char evaluated[1024];
  pg_evaluateExpression(value, evaluated);
  setVarValue(var, evaluated);
  return 1;
}

variable* pg_add_temp_var()
{
  if (pg_game_status.tempVariables == NULL) {
    pg_game_status.tempVariables = malloc(sizeof(variable) * (pg_game_status.tempVariablesCount + 1));
  } else {
    variable *tmp = realloc(pg_game_status.tempVariables, sizeof(variable) * (pg_game_status.tempVariablesCount + 1));
    if (tmp != NULL) {
      pg_game_status.tempVariables = tmp;
      tmp = NULL;
    } else {
      logWarning("Realloc failed");
      return NULL;
    }
  }

  variable *res = &pg_game_status.tempVariables[pg_game_status.tempVariablesCount++];
  res->name = NULL;
  res->value = NULL;
  return res;
}

int pg_delete_temp_var(int index)
{
  if (index < 0 || index >= pg_game_status.tempVariablesCount)
    return 0;

  variable *var = &pg_game_status.tempVariables[index];
  free(var->name);
  var->name = NULL;
  free(var->value);
  var->value =  NULL;

  if (pg_game_status.tempVariablesCount == 1) {
    free(pg_game_status.tempVariables);
    pg_game_status.tempVariables = NULL;
    pg_game_status.tempVariablesCount = 0;

    return 1;
  }

  for (int i = index; i < pg_game_status.tempVariablesCount - 1; i++) {
    pg_game_status.tempVariables[i] = pg_game_status.tempVariables[i + 1];
  }
  pg_game_status.tempVariablesCount--;

  variable *tmp = realloc(pg_game_status.tempVariables, sizeof(variable) * (pg_game_status.tempVariablesCount));
  if (tmp != NULL) {
    pg_game_status.tempVariables = tmp;
    tmp = NULL;
  } else {
    logWarning("Realloc failed");
    return 0;
  }

  return 1;
}

void pg_clearTempVars()
{
  if (pg_game_status.tempVariables != NULL) {
    for (int i = 0; i < pg_game_status.tempVariablesCount; i++) {
      free(pg_game_status.tempVariables[i].name);
      pg_game_status.tempVariables[i].name = NULL;
      free(pg_game_status.tempVariables[i].value);
      pg_game_status.tempVariables[i].value = NULL;
    }
    free(pg_game_status.tempVariables);
    pg_game_status.tempVariables = NULL;
  }
  pg_game_status.tempVariablesCount = 0;
}

void pg_clearChoices()
{
  if (pg_game_status.choices != NULL) {
    for (int i = 0; i < pg_game_status.choicesCount; i++) {
      free(pg_game_status.choices[i].text);
      pg_game_status.choices[i].text = NULL;
    }
    free(pg_game_status.choices);
    pg_game_status.choices = NULL;
  }
  pg_game_status.choicesCount = 0;
}

char* pg_getNextLine(const char *text, int* startPosition, int maxWidth, int *wordWrap, int *last_x)
{
  if (wordWrap != NULL)
    *wordWrap = 0;
  if (last_x != NULL)
    *last_x = 0;

  if (!text || maxWidth <= 0)
    return NULL;

  int textLen = strlen(text);
  if (!textLen || *startPosition >= textLen - 1)
    return NULL;

  char *res = NULL;
  char test[5];
  int width = 0;
  int endOfLinePosition = *startPosition;
  int endOfPreviousWordPosition = -1;
  int previousWordWidth = 0;
  int i = *startPosition;
  int is_command = -1;

  TTF_Font *font = _Globals.textFont;
  while (i < textLen) {
    endOfLinePosition = i;
    if (text[i] == '\n') {
      break;
    }

    if (!isspace(text[i]) && is_command < 0) {
      // If this line contains a command, don't wrap it
      is_command = text[i] == '*' ? 1 : 0;
    }

    // Check font change:
    if (text[i] == '[')  {
      // Italic
      if (i + 2 < textLen && text[i + 1] == 'i' && text[i + 2] == ']') {
        font = _Globals.textItalicFont;
        i += 3;
        endOfLinePosition = i;
        continue;
      } else if (i + 3 < textLen && text[i + 1] == '/' && text[i + 2] == 'i' && text[i + 3] == ']') {
        font = _Globals.textFont;
        i += 4;
        endOfLinePosition = i;
        continue;
      }

      // Bold
      if (i + 2 < textLen && text[i + 1] == 'b' && text[i + 2] == ']') {
        font = _Globals.textBoldFont;
        i += 3;
        endOfLinePosition = i;
        continue;
      } else if (i + 3 < textLen && text[i + 1] == '/' && text[i + 2] == 'b' && text[i + 3] == ']') {
        font = _Globals.textFont;
        i += 4;
        endOfLinePosition = i;
        continue;
      }
    }

    int charBytes = u8seqlen(&text[i]);
    memset(&test, 0, sizeof(test));
    strncpy(test, &text[i], charBytes);
    int charWidth;
    TTF_MeasureUTF8(font, test, _Globals.windowWidth, &charWidth, NULL);
    if (!is_command && width + charWidth > maxWidth) {
      // Wrap line
      if (endOfPreviousWordPosition < 0) {
        // We cannot word-break
        endOfLinePosition--;
      } else {
        endOfLinePosition = endOfPreviousWordPosition;
        if (wordWrap != NULL)
          *wordWrap = 1;
        if (last_x != NULL)
          *last_x = previousWordWidth;
      }
      break;
    }
    width += charWidth;
    if (text[i] == ' ' || text[i] == '\t') {
      endOfPreviousWordPosition = i;
      previousWordWidth = width - charWidth;
    }
    i += charBytes;
    if (i == textLen) {
      endOfLinePosition = i - 1;
    }
  }

  res = malloc(endOfLinePosition - *startPosition + 2);
  int idx = 0;
  for (int j = *startPosition; j <= endOfLinePosition; j++) {
    if (text[j] != '\r' && text[j] != '\n' && (idx > 0 || text[j] != ' '))
      res[idx++] = text[j];
  }
  res[idx] = '\0';

  // Remove trailing spaces
  while (--idx >= 0 && res[idx] == ' ') {
    res[idx] = '\0';
  }

  if (text[endOfLinePosition] == '\n' || text[endOfLinePosition] == ' ') {
    *startPosition = endOfLinePosition + 1;
  } else {
    *startPosition = endOfLinePosition;
  }
  return res;
}

long pg_get_if_end(FILE *fp, int indentation)
{
  char line[4096];
  char command[256];
  long pos = ftell(fp);
  long start_line_pos = pos;
  while (fgets(line, sizeof(line), fp)) {
    int line_indentation = getIndentation(line);
    if (line_indentation < indentation)
      break;

    if (indentation == line_indentation && !isEmpty(line)) {
      trim(line);
      pg_getCommand(line, command);
      if (stricmp(command, "else") == 0 || stricmp(command, "elseif") == 0)
        continue;

      fseek(fp, pos, SEEK_SET);
      return start_line_pos;
    }

    start_line_pos = ftell(fp);
  }

  fseek(fp, pos, SEEK_SET);
  return start_line_pos;
}

void pg_clear_labels()
{
  if (pg_game_status.labels == NULL) {
    pg_game_status.labelsCount = -1;
    return;
  }

  for (int i = 0; i < pg_game_status.labelsCount; i++) {
    label* lbl = &pg_game_status.labels[i];
    free(lbl->name);
  }

  free(pg_game_status.labels);
  pg_game_status.labels = NULL;
  pg_game_status.labelsCount = -1;
}

void pg_read_labels()
{
  logDebug("Reading all labels");
  long pos = ftell(pg_game_status.fp);
  fseek(pg_game_status.fp, 0, SEEK_SET);
  pg_game_status.labelsCount = 0;
  char line[4096];
  int lineStartPosition = 0;
  long nextLineStartPosition = 0;
  while (fgets(line, sizeof(line), pg_game_status.fp)) {
    lineStartPosition = nextLineStartPosition;
    nextLineStartPosition = ftell(pg_game_status.fp);

    trim(line);
    if (startsWithWord("*label", line)) {
      pg_game_status.labelsCount++;
      if (pg_game_status.labels == NULL) {
        pg_game_status.labels = malloc(sizeof(label) * pg_game_status.labelsCount);
      } else {
        label *tmp = realloc(pg_game_status.labels, sizeof(label) * pg_game_status.labelsCount);
        if (tmp != NULL) {
          pg_game_status.labels = tmp;
          tmp = NULL;
        } else {
          logWarning("Realloc failed");
          break;
        }
      }

      pg_game_status.labels[pg_game_status.labelsCount - 1].name = malloc(strlen(line) - 7 + 1);
      subString(line, pg_game_status.labels[pg_game_status.labelsCount - 1].name, 7, strlen(line) - 7);
      pg_game_status.labels[pg_game_status.labelsCount - 1].position = lineStartPosition;

      logDebug("\tLabel name=%s, position=%li", pg_game_status.labels[pg_game_status.labelsCount - 1].name, pg_game_status.labels[pg_game_status.labelsCount - 1].position);
    }
  }
  fseek(pg_game_status.fp, pos, SEEK_SET);
}
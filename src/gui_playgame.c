#include "CoGSDL.h"
#include "gui_playgame.h"

void pg_init();
void pg_dispose();
static void pg_clearParams();
void pg_exec();
void pg_renderText(const char* text, int x, int y, int width, int height, int scrollY);
int pg_renderPageBreak();
int pg_indexOfWordEnd(const char *string, const int startFrom);
void pg_getCommand(const char *line, char *command) ;
/// @brief Go to the next scene of the scene list
/// @return 0 if there is no next scene (the game is ended)
int pg_goToNextScene();
void pg_clearGosubStack();
static void pg_popGosub();
static void pg_getCommandArgs(const char *line, char *args, int size);
long pg_manageGotoSceneCommand(const char *text);
void pg_manageParamsCommand(const char *text);
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
/// @brief Continue the execution with the body of a choice option
/// @param index The option index
void pg_selectChoice(int index);
static int pg_isBlank(const char *line);
/// @brief Remove the choice contexts of the current gosub level (or deeper)
static void pg_clearChoiceContexts(int gosubCount);
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
/// @brief Manage an if (and the following *elseif/*else)
/// @param text The text of the line with the if command
/// @param indentation The line indentation
/// @return The new file position where continue execution (the file is positioned there)
long pg_manageIfCommand(const char *text, int indentation);
/// @brief Skip the *elseif/*else branches following an executed branch
/// @param indentation The *elseif/*else indentation
/// @return The new file position where continue execution (the file is positioned there)
long pg_skipElseChain(int indentation);
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
    int index = pg_game_status.selectedChoiceIndex;
    pg_game_status.selectedChoiceIndex = -1;
    pg_selectChoice(index);
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

void pg_selectChoice(int index)
{
  choice *c = &pg_game_status.choices[index];

  // When the option body ends, continue after the choice block
  if (pg_game_status.choiceContextsCount == sizeof(pg_game_status.choiceContexts) / sizeof(choice_context)) {
    logWarning("Too many nested choices");
    memmove(&pg_game_status.choiceContexts[0], &pg_game_status.choiceContexts[1], sizeof(choice_context) * (pg_game_status.choiceContextsCount - 1));
    pg_game_status.choiceContextsCount--;
  }
  choice_context *ctx = &pg_game_status.choiceContexts[pg_game_status.choiceContextsCount++];
  ctx->endPosition = pg_game_status.choiceEndPosition;
  ctx->indentation = c->indentation;
  ctx->gosubCount = pg_game_status.gosubCount;

  fseek(pg_game_status.fp, c->position, SEEK_SET);
  pg_game_status.fpPosition = c->position;
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
  snprintf(pg_game_status.sceneName, sizeof(pg_game_status.sceneName), "startup");
  pg_game_status.paramsCount = 0;
  pg_game_status.gameEnded = 0;
  pg_game_status.fpPosition = runningGame->startPosition;
  pg_game_status.scrollY = 0;
  pg_game_status.textFullyRendered = 0;
  pg_game_status.lastRenderedY = 0;
  pg_game_status.selectedChoiceIndex = -1;
  pg_game_status.gosubCount = 0;
  pg_game_status.used_choices_count = 0;
  pg_game_status.choiceContextsCount = 0;

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
  pg_clearGosubStack();
  pg_clearParams();
  pg_game_status.gameEnded = 0;
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
  pg_game_status.used_choices_count = 0;
  pg_game_status.choiceContextsCount = 0;

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
      continue;
    }
    if (cond[i] == ')')
      depth--;
    // "and"/"or" after a space or a closing parenthesis: (a = 1)or b
    if (depth == 0 && (isspace((unsigned char)cond[i]) || cond[i] == ')')) {
      int opLen = 0;
      int isAnd = 0;
      if (pg_strnicmp(&cond[i + 1], "and", 3) == 0 && (isspace((unsigned char)cond[i + 4]) || cond[i + 4] == '(')) {
        opLen = 3;
        isAnd = 1;
      } else if (pg_strnicmp(&cond[i + 1], "or", 2) == 0 && (isspace((unsigned char)cond[i + 3]) || cond[i + 3] == '(')) {
        opLen = 2;
      }

      if (opLen) {
        // Keep the closing parenthesis in the left condition
        char *left = pg_strndup(cond, cond[i] == ')' ? i + 1 : i);
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

  if (pg_game_status.gameEnded) {
    // Nothing more to execute: keep showing the last page
    pg_game_status.showPageBreak = 0;
    pg_clearChoices();
    if (pg_game_status.text == NULL)
      pg_appendText("The End", 0);
    return 1;
  }

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
    int lineIndentation = getIndentation(line);

    // End of the body of the selected option: continue after the choice block
    if (pg_game_status.choiceContextsCount > 0 && !pg_isBlank(line) && !isComment(line)) {
      choice_context *ctx = &pg_game_status.choiceContexts[pg_game_status.choiceContextsCount - 1];
      if (ctx->gosubCount == pg_game_status.gosubCount && lineIndentation <= ctx->indentation) {
        logDebug("End of option body -> %li", ctx->endPosition);
        pg_game_status.choiceContextsCount--;
        fseek(pg_game_status.fp, ctx->endPosition, SEEK_SET);
        pg_game_status.fpPosition = ctx->endPosition;
        continue;
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
        // params name1 name2...
        pg_manageParamsCommand(line);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "ending") == 0) {
        // End of the game
        pg_clearChoiceContexts(0);
        pg_game_status.gameEnded = 1;
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        break;
      }

      if (stricmp(command, "goto_scene") == 0) {
        // goto_scene scene [label]
        long newPosition = pg_manageGotoSceneCommand(line);
        logDebug("%s -> %li", line, newPosition);
        pg_game_status.fpPosition = newPosition;

        if (textStarted)
          pg_appendText("\n", 0);
        textStarted = 0;
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
        // page_break [button text]
        char message[sizeof(pg_game_status.pageBreakMessage)];
        pg_getCommandArgs(line, message, sizeof(message));
        pg_game_status.showPageBreak = 1;
        snprintf(pg_game_status.pageBreakMessage, sizeof(pg_game_status.pageBreakMessage), "%s", strlen(message) ? message : "Next");
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
        pg_clearChoiceContexts(0);
        pg_manageFinishCommand(line);
        pg_game_status.showPageBreak = 1;
        pg_game_status.fpPosition = pg_game_status.fpSize;
        break;
      }

      if (stricmp(command, "goto") == 0) {
        // goto: we leave the option body (if any)
        pg_clearChoiceContexts(pg_game_status.gosubCount);
        long newPosition = pg_manageGotoCommand(line);
        logDebug("%s -> %i", line, newPosition);
        pg_game_status.fpPosition = newPosition;

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

        if (textStarted)
          pg_appendText("\n", 0);
        textStarted = 0;
        continue;
      }

      if (stricmp(command, "return") == 0) {
        if (pg_game_status.gosubCount == 0) {
          logWarning("*return without *gosub");
          pg_game_status.fpPosition = ftell(pg_game_status.fp);
          continue;
        }
        pg_popGosub();
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        logDebug("%s -> %li", line, pg_game_status.fpPosition);
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
        char value[4096];
        subString(line, varName, idx, wordEnd - idx);
        idx += strlen(varName);
        subString(line, value, idx + 1, lineLen - idx);
        pg_setVarValue(varName, value);
        continue;
      }

      if (stricmp(command, "if") == 0) {
        // if: continue in the first branch with a true condition
        long newPosition = pg_manageIfCommand(line, lineIndentation);
        logDebug("%s -> %li", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        continue;
      }

      if (stricmp(command, "else") == 0 || stricmp(command, "elseif") == 0 || stricmp(command, "elsif") == 0) {
        // Reached at the end of the executed branch: skip the remaining branches
        long newPosition = pg_skipElseChain(lineIndentation);
        logDebug("%s -> %li", line, newPosition);
        pg_game_status.fpPosition = newPosition;
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

/// @brief Read a line from the current file, removing the line terminator
/// @param line The buffer
/// @param size The buffer size
/// @param lineStart Set to the position of the line start
/// @return 0 on EOF
static int pg_readLine(char *line, int size, long *lineStart)
{
  *lineStart = ftell(pg_game_status.fp);
  if (!fgets(line, size, pg_game_status.fp))
    return 0;
  line[strcspn(line, "\r\n")] = '\0';
  return 1;
}

/// @brief Check if a line is empty or contains only whitespaces
static int pg_isBlank(const char *line)
{
  for (int i = 0; line[i]; i++) {
    if (!isspace((unsigned char)line[i]))
      return 0;
  }
  return 1;
}

/// @brief Get the arguments of a command (the text after the command name)
/// @param line The trimmed line (*command args)
/// @param args The buffer for the arguments
/// @param size The buffer size
static void pg_getCommandArgs(const char *line, char *args, int size)
{
  int i = 0;
  if (line[i] == '*')
    i++;
  while (line[i] && !isspace((unsigned char)line[i]))
    i++;
  while (line[i] && isspace((unsigned char)line[i]))
    i++;
  snprintf(args, size, "%s", line + i);
  trim(args);
}

static int pg_isElseCommand(const char *command)
{
  return stricmp(command, "else") == 0 || stricmp(command, "elseif") == 0 || stricmp(command, "elsif") == 0;
}

/// @brief Skip the body of a false branch and search the next branch to execute
/// @param indentation The indentation of the *if/*elseif/*else lines
/// @return The position of the body of the first true *elseif or of the *else,
///         or the position of the first line after the if
static long pg_findNextBranch(int indentation)
{
  char line[4096];
  char command[256];
  char condition[4096];
  long lineStart;
  while (pg_readLine(line, sizeof(line), &lineStart)) {
    if (pg_isBlank(line) || isComment(line))
      continue;

    int lineIndentation = getIndentation(line);
    if (lineIndentation > indentation)
      continue; // Body of the false branch
    if (lineIndentation < indentation)
      break;

    trim(line);
    pg_getCommand(line, command);
    if (stricmp(command, "else") == 0) {
      return ftell(pg_game_status.fp);
    }
    if (stricmp(command, "elseif") == 0 || stricmp(command, "elsif") == 0) {
      pg_getCommandArgs(line, condition, sizeof(condition));
      int res = pg_evaluateCondition(condition, indentation);
      logDebug("\t*%s %s -> %s", command, condition, res ? "true" : "false");
      if (res)
        return ftell(pg_game_status.fp);
      continue;
    }
    break;
  }

  fseek(pg_game_status.fp, lineStart, SEEK_SET);
  return lineStart;
}

long pg_skipElseChain(int indentation)
{
  char line[4096];
  char command[256];
  long lineStart;
  while (pg_readLine(line, sizeof(line), &lineStart)) {
    if (pg_isBlank(line) || isComment(line))
      continue;

    int lineIndentation = getIndentation(line);
    if (lineIndentation > indentation)
      continue; // Body of a skipped branch
    if (lineIndentation < indentation)
      break;

    trim(line);
    pg_getCommand(line, command);
    if (pg_isElseCommand(command))
      continue;
    break;
  }

  fseek(pg_game_status.fp, lineStart, SEEK_SET);
  return lineStart;
}

long pg_manageIfCommand(const char *text, int indentation)
{
  char condition[4096];
  pg_getCommandArgs(text, condition, sizeof(condition));

  int res = pg_evaluateCondition(condition, indentation);
  logDebug("\t*if %s -> %s", condition, res ? "true" : "false");
  if (res)
    return ftell(pg_game_status.fp);

  return pg_findNextBranch(indentation);
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
    // just a variable name or a reference (boolean condition)
    char value[1024];
    pg_evaluateExpression(condition, value);
    trim(value);
    res = stricmp(value, "true") == 0;
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

    // Functions: round(expr)
    const char *afterName = *p;
    pg_arithSkipSpaces(p);
    if (**p == '(' && stricmp(name, "round") == 0) {
      double arg;
      if (!pg_arithFactor(p, &arg))
        return 0;
      *out = floor(arg + 0.5); // Same as JavaScript Math.round
      return 1;
    }
    *p = afterName;

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

/// @brief Find a top level '&' (string concatenation) outside quotes and parentheses
static int pg_findConcatenation(const char *expression)
{
  int depth = 0;
  int inQuote = 0;
  for (int i = 0; expression[i]; i++) {
    if (expression[i] == '"' && (i == 0 || expression[i - 1] != '\\'))
      inQuote = !inQuote;
    if (inQuote)
      continue;
    if (expression[i] == '(')
      depth++;
    else if (expression[i] == ')')
      depth--;
    else if (expression[i] == '&' && depth == 0)
      return i;
  }
  return -1;
}

/// @brief Resolve the name of a reference {expr}: the value of expr is the name of the variable
/// @param content The text between the braces
/// @param name Buffer for the variable name (or the value if it is a number or a boolean)
static void pg_evaluateReferenceName(const char *content, char *name, int size)
{
  char *inner = pg_strndup(content, strlen(content));
  trim(inner);
  char value[1024];
  pg_evaluateExpression(inner, value);
  free(inner);
  trim(value);
  snprintf(name, size, "%s", value);
}

/// @brief Replace the references {expr} with the value of the referenced variable
/// @return The new expression (must be freed) or NULL on error
static char *pg_resolveReferences(const char *expression)
{
  pg_strbuf sb = { NULL, 0, 0 };
  int inQuote = 0;
  int len = strlen(expression);
  for (int i = 0; i < len; i++) {
    if (expression[i] == '"' && (i == 0 || expression[i - 1] != '\\'))
      inQuote = !inQuote;

    if (!inQuote && expression[i] == '{') {
      int close = pg_findClosing(expression, i);
      if (close < 0) {
        logWarning("Missing '}' in %s", expression);
        free(sb.buf);
        return NULL;
      }
      char *content = pg_strndup(expression + i + 1, close - i - 1);
      char name[1024];
      pg_evaluateReferenceName(content, name, sizeof(name));
      free(content);

      int intValue;
      if ((strlen(name) > 0 && is_number(name, &intValue)) || stricmp(name, "true") == 0 || stricmp(name, "false") == 0) {
        // Same as ChoiceScript: numbers and booleans are their own value
        pg_sb_append(&sb, name, strlen(name));
      } else {
        variable *var = pg_getVar(name);
        if (var == NULL || var->value == NULL) {
          logWarning("Reference {%s}: variable not found", name);
          free(sb.buf);
          return NULL;
        }
        if (var->type == string)
          pg_sb_append(&sb, "\"", 1);
        pg_sb_append(&sb, var->value, strlen(var->value));
        if (var->type == string)
          pg_sb_append(&sb, "\"", 1);
      }
      i = close;
      continue;
    }
    pg_sb_append(&sb, &expression[i], 1);
  }
  if (sb.buf == NULL)
    return pg_strndup("", 0);
  return sb.buf;
}

int pg_evaluateExpression(const char *expression, char *value)
{
  value[0] = '\0';
  if (!expression)
    return 0;

  int expLen = strlen(expression);

  // References: {var_name_expression}
  if (strchr(expression, '{') != NULL && !(expLen > 1 && expression[0] == '"' && expression[expLen - 1] == '"')) {
    char *resolved = pg_resolveReferences(expression);
    if (resolved == NULL)
      return 0;
    int res = 0;
    if (strchr(resolved, '{') == NULL || strcmp(resolved, expression) != 0)
      res = pg_evaluateExpression(resolved, value);
    free(resolved);
    return res;
  }

  // String concatenation: "text" & var
  int concat = pg_findConcatenation(expression);
  if (concat > 0) {
    char *left = pg_strndup(expression, concat);
    char *right = pg_strndup(expression + concat + 1, expLen - concat - 1);
    trim(left);
    trim(right);
    char leftValue[1024];
    char rightValue[1024];
    int res = pg_evaluateExpression(left, leftValue) & pg_evaluateExpression(right, rightValue);
    // value is at least 1024 bytes
    size_t leftLen = strlen(leftValue);
    memcpy(value, leftValue, leftLen + 1);
    for (size_t i = 0; rightValue[i] && leftLen < 1023; i++)
      value[leftLen++] = rightValue[i];
    value[leftLen] = '\0';
    free(left);
    free(right);
    return res;
  }
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

/// @brief Open a scene file and make it the current one
/// @param sceneName The scene name (file name without extension)
/// @param closeCurrent Set to 1 to close the current file
/// @return 1 on success
static int pg_openScene(const char *sceneName, int closeCurrent)
{
  char filePath[512];
  snprintf(filePath, sizeof(filePath), "%s/scenes/%s.txt", runningGame->path, sceneName);
  FILE *fp = fopen(filePath, "rb");
  if (!fp) {
    logError("Failed to open file %s", filePath);
    return 0;
  }
  logInfo("Opened file %s", filePath);

  if (closeCurrent && pg_game_status.fp)
    fclose(pg_game_status.fp);
  pg_game_status.fp = fp;
  pg_clear_labels();
  pg_clear_image_cache();

  fseek(pg_game_status.fp, 0L, SEEK_END);
  pg_game_status.fpSize = ftell(pg_game_status.fp);
  fseek(pg_game_status.fp, 0L, SEEK_SET);

  // Skip the UTF8 BOM
  unsigned char bom[3];
  if (fread(bom, 1, 3, pg_game_status.fp) != 3 || bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF)
    fseek(pg_game_status.fp, 0L, SEEK_SET);
  pg_game_status.fpPosition = ftell(pg_game_status.fp);

  snprintf(pg_game_status.sceneName, sizeof(pg_game_status.sceneName), "%s", sceneName);
  // Scenes not in the scene list (e.g. used with *gosub_scene) keep the current index
  for (int i = 0; i < runningGame->scenesCount; i++) {
    if (stricmp(runningGame->scenes[i], sceneName) == 0) {
      pg_game_status.sceneIndex = i;
      break;
    }
  }
  return 1;
}

int pg_goToNextScene()
{
  if (pg_game_status.sceneIndex + 1 >= runningGame->scenesCount) {
    logInfo("No next scene: game ended");
    pg_game_status.gameEnded = 1;
    return 0;
  }

  pg_clearTempVars();
  pg_clearChoiceContexts(0);
  pg_clearGosubStack();
  if (!pg_openScene(runningGame->scenes[pg_game_status.sceneIndex + 1], 1)) {
    pg_game_status.gameEnded = 1;
    return 0;
  }
  return 1;
}

/// @brief Split command arguments in tokens: words, "strings", (expressions), {references}
/// @param text The arguments
/// @param tokens The tokens (must be freed)
/// @param maxTokens The maximum number of tokens
/// @return The number of tokens
static int pg_tokenizeArgs(const char *text, char **tokens, int maxTokens)
{
  int count = 0;
  int len = strlen(text);
  int i = 0;
  while (i < len) {
    while (i < len && isspace((unsigned char)text[i]))
      i++;
    if (i >= len)
      break;

    int start = i;
    if (text[i] == '"') {
      i++;
      while (i < len && !(text[i] == '"' && text[i - 1] != '\\'))
        i++;
      if (i < len)
        i++;
    } else if (text[i] == '(' || text[i] == '{') {
      int close = pg_findClosing(text, i);
      i = close < 0 ? len : close + 1;
    } else {
      while (i < len && !isspace((unsigned char)text[i]))
        i++;
    }

    if (count < maxTokens) {
      tokens[count++] = pg_strndup(text + start, i - start);
    } else {
      logWarning("Too many arguments: %s", text);
      break;
    }
  }
  return count;
}

static void pg_freeTokens(char **tokens, int count)
{
  for (int i = 0; i < count; i++)
    free(tokens[i]);
}

static void pg_clearParams()
{
  for (int i = 0; i < pg_game_status.paramsCount; i++) {
    free(pg_game_status.params[i]);
    pg_game_status.params[i] = NULL;
  }
  pg_game_status.paramsCount = 0;
}

/// @brief Evaluate the arguments of *gosub, *gosub_scene, *goto_scene (used by *params)
static void pg_setParams(char **tokens, int count)
{
  pg_clearParams();
  for (int i = 0; i < count && i < (int)(sizeof(pg_game_status.params) / sizeof(char*)); i++) {
    char value[4096];
    pg_evaluateExpression(tokens[i], value);
    pg_game_status.params[pg_game_status.paramsCount++] = pg_strndup(value, strlen(value));
    logDebug("\tparam_%i=%s", i + 1, value);
  }
}

static void pg_freeVariables(variable *vars, int count)
{
  if (vars == NULL)
    return;
  for (int i = 0; i < count; i++) {
    free(vars[i].name);
    free(vars[i].value);
  }
  free(vars);
}

/// @brief Create or set a temporary variable (the type is detected from the value)
static void pg_setTempVar(const char *name, const char *value)
{
  variable *var = NULL;
  for (int i = 0; i < pg_game_status.tempVariablesCount; i++) {
    if (stricmp(pg_game_status.tempVariables[i].name, name) == 0) {
      var = &pg_game_status.tempVariables[i];
      break;
    }
  }
  if (var == NULL) {
    var = pg_add_temp_var();
    if (var == NULL)
      return;
    var->name = pg_strndup(name, strlen(name));
  }

  int intValue;
  if (stricmp(value, "true") == 0 || stricmp(value, "false") == 0)
    var->type = boolean;
  else if (strlen(value) > 0 && is_number(value, &intValue))
    var->type = number;
  else
    var->type = string;

  free(var->value);
  var->value = pg_strndup(value, strlen(value));
  logDebug("\tTEMPVAR %s=%s", var->name, var->value);
}

/// @brief Push a gosub frame
/// @param isScene Set to 1 for *gosub_scene: the current file and temporary variables are saved
/// @return The frame or NULL
static gosub_info *pg_pushGosub(int isScene)
{
  if (pg_game_status.gosubCount + 1 >= (int)(sizeof(pg_game_status.gosub) / sizeof(gosub_info))) {
    logWarning("Too many nested gosub");
    return NULL;
  }

  pg_game_status.gosubCount++;
  gosub_info *frame = &pg_game_status.gosub[pg_game_status.gosubCount];
  memset(frame, 0, sizeof(gosub_info));
  frame->position = ftell(pg_game_status.fp);
  if (isScene) {
    frame->fp = pg_game_status.fp;
    frame->fpSize = pg_game_status.fpSize;
    frame->sceneIndex = pg_game_status.sceneIndex;
    snprintf(frame->sceneName, sizeof(frame->sceneName), "%s", pg_game_status.sceneName);
    // The scene called with gosub_scene has its own temporary variables
    frame->tempVariables = pg_game_status.tempVariables;
    frame->tempVariablesCount = pg_game_status.tempVariablesCount;
    pg_game_status.tempVariables = NULL;
    pg_game_status.tempVariablesCount = 0;
  }
  return frame;
}

/// @brief Pop the last gosub frame, restoring the file and the temporary variables of a *gosub_scene
static void pg_popGosub()
{
  gosub_info *frame = &pg_game_status.gosub[pg_game_status.gosubCount];
  if (frame->fp != NULL) {
    if (pg_game_status.fp)
      fclose(pg_game_status.fp);
    pg_game_status.fp = frame->fp;
    pg_game_status.fpSize = frame->fpSize;
    pg_game_status.sceneIndex = frame->sceneIndex;
    snprintf(pg_game_status.sceneName, sizeof(pg_game_status.sceneName), "%s", frame->sceneName);
    pg_clear_labels();

    pg_clearTempVars();
    pg_game_status.tempVariables = frame->tempVariables;
    pg_game_status.tempVariablesCount = frame->tempVariablesCount;
  }
  fseek(pg_game_status.fp, frame->position, SEEK_SET);
  memset(frame, 0, sizeof(gosub_info));
  pg_game_status.gosubCount--;
  // Remove the choice contexts of the subroutine
  pg_clearChoiceContexts(pg_game_status.gosubCount + 1);
}

void pg_clearGosubStack()
{
  for (int i = 1; i <= pg_game_status.gosubCount; i++) {
    gosub_info *frame = &pg_game_status.gosub[i];
    if (frame->fp != NULL)
      fclose(frame->fp);
    pg_freeVariables(frame->tempVariables, frame->tempVariablesCount);
    memset(frame, 0, sizeof(gosub_info));
  }
  pg_game_status.gosubCount = 0;
}

long pg_manageGosubCommand(const char *text)
{
  // *gosub label arg1 arg2...
  char args[4096];
  char *tokens[34];
  pg_getCommandArgs(text, args, sizeof(args));
  int count = pg_tokenizeArgs(args, tokens, 34);
  if (count == 0) {
    logWarning("gosub without label name");
    return ftell(pg_game_status.fp);
  }

  label *lbl = pg_get_label(tokens[0]);
  if (lbl == NULL) {
    logWarning("Label %s not found", tokens[0]);
    pg_freeTokens(tokens, count);
    return ftell(pg_game_status.fp);
  }
  long position = lbl->position;

  pg_setParams(tokens + 1, count - 1);
  pg_freeTokens(tokens, count);
  if (pg_pushGosub(0) == NULL)
    return ftell(pg_game_status.fp);

  fseek(pg_game_status.fp, position, SEEK_SET);
  return position;
}

long pg_manageGosubSceneCommand(const char *text)
{
  // *gosub_scene scene [label] [arg1 arg2...]
  char args[4096];
  char *tokens[34];
  pg_getCommandArgs(text, args, sizeof(args));
  int count = pg_tokenizeArgs(args, tokens, 34);
  if (count == 0) {
    logWarning("gosub_scene without scene name");
    return ftell(pg_game_status.fp);
  }

  // Evaluate the arguments with the variables of the current scene
  pg_setParams(tokens + 2, count > 2 ? count - 2 : 0);

  if (pg_pushGosub(1) == NULL) {
    pg_freeTokens(tokens, count);
    return ftell(pg_game_status.fp);
  }
  if (!pg_openScene(tokens[0], 0)) {
    // Restore the caller (the current file was not replaced)
    gosub_info *frame = &pg_game_status.gosub[pg_game_status.gosubCount];
    frame->fp = NULL;
    pg_game_status.tempVariables = frame->tempVariables;
    pg_game_status.tempVariablesCount = frame->tempVariablesCount;
    frame->tempVariables = NULL;
    pg_game_status.gosubCount--;
    pg_freeTokens(tokens, count);
    return ftell(pg_game_status.fp);
  }

  long position = pg_game_status.fpPosition;
  if (count > 1) {
    label *lbl = pg_get_label(tokens[1]);
    if (lbl == NULL)
      logWarning("Label %s not found in scene %s", tokens[1], tokens[0]);
    else
      position = lbl->position;
  }
  pg_freeTokens(tokens, count);

  fseek(pg_game_status.fp, position, SEEK_SET);
  return position;
}

long pg_manageGotoSceneCommand(const char *text)
{
  // *goto_scene scene [label] [arg1 arg2...]
  char args[4096];
  char *tokens[34];
  pg_getCommandArgs(text, args, sizeof(args));
  int count = pg_tokenizeArgs(args, tokens, 34);
  if (count == 0) {
    logWarning("goto_scene without scene name");
    return ftell(pg_game_status.fp);
  }

  pg_setParams(tokens + 2, count > 2 ? count - 2 : 0);

  if (pg_game_status.gosubCount > 0) {
    logWarning("*goto_scene after *gosub_scene without *return");
    pg_clearGosubStack();
  }
  pg_clearChoiceContexts(0);

  if (!pg_openScene(tokens[0], 1)) {
    pg_freeTokens(tokens, count);
    return ftell(pg_game_status.fp);
  }
  pg_clearTempVars();

  long position = pg_game_status.fpPosition;
  if (count > 1) {
    label *lbl = pg_get_label(tokens[1]);
    if (lbl == NULL)
      logWarning("Label %s not found in scene %s", tokens[1], tokens[0]);
    else
      position = lbl->position;
  }
  pg_freeTokens(tokens, count);

  fseek(pg_game_status.fp, position, SEEK_SET);
  return position;
}

void pg_manageParamsCommand(const char *text)
{
  // *params name1 name2...: name the arguments of gosub, they are also available as param_1, param_2...
  char args[4096];
  char *tokens[34];
  char name[32];
  pg_getCommandArgs(text, args, sizeof(args));
  int count = pg_tokenizeArgs(args, tokens, 34);

  int i;
  for (i = 0; i < count; i++) {
    if (i >= pg_game_status.paramsCount) {
      logWarning("No parameter passed for %s", tokens[i]);
      break;
    }
    pg_setTempVar(tokens[i], pg_game_status.params[i]);
    snprintf(name, sizeof(name), "param_%i", i + 1);
    pg_setTempVar(name, pg_game_status.params[i]);
  }
  for (; i < pg_game_status.paramsCount; i++) {
    snprintf(name, sizeof(name), "param_%i", i + 1);
    pg_setTempVar(name, pg_game_status.params[i]);
  }
  snprintf(name, sizeof(name), "%i", pg_game_status.paramsCount);
  pg_setTempVar("param_count", name);
  pg_freeTokens(tokens, count);
}

void pg_manageTempCommand(const char *text)
{
  // *temp name [value]
  char args[4096];
  pg_getCommandArgs(text, args, sizeof(args));
  int nameEnd = 0;
  while (args[nameEnd] && !isspace((unsigned char)args[nameEnd]))
    nameEnd++;
  if (nameEnd == 0) {
    logWarning("*temp without variable name");
    return;
  }

  char *name = pg_strndup(args, nameEnd);
  char *rawValue = args + nameEnd;
  while (isspace((unsigned char)*rawValue))
    rawValue++;

  char value[4096] = "";
  if (*rawValue)
    pg_evaluateExpression(rawValue, value);
  pg_setTempVar(name, value);

  // A quoted value is always a string ("123")
  if (rawValue[0] == '"') {
    variable *var = pg_getVar(name);
    if (var != NULL)
      var->type = string;
  }
  free(name);
}

/// @brief Add an option to the current choice
/// @param line The option line (trimmed)
/// @param start The position of the '#'
static void pg_addChoiceOption(const char *line, int start, int indentation, int reusable, int disableReuse, int selectable)
{
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
      return;
    }
  }

  choice *choice = &pg_game_status.choices[pg_game_status.choicesCount - 1];

  start++;
  choice->reusable = reusable;
  choice->disable_reuse = disableReuse;
  choice->selectable = selectable;
  choice->indentation = indentation;
  choice->text = malloc(strlen(line) - start + 1);
  choice->position = ftell(pg_game_status.fp);
  snprintf(choice->id, sizeof(choice->id), "%.40s_%li", pg_game_status.sceneName, choice->position);
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

static void pg_parseChoiceBlock(int blockIndentation);

/// @brief Parse the options inside an *if/*elseif/*else of a choice
/// @param parentIndentation The indentation of the *if line
static void pg_parseChoiceNestedBlock(int parentIndentation)
{
  char line[4096];
  long bodyStart = ftell(pg_game_status.fp);
  long lineStart;
  int nestedIndentation = -1;
  while (pg_readLine(line, sizeof(line), &lineStart)) {
    if (pg_isBlank(line) || isComment(line))
      continue;
    nestedIndentation = getIndentation(line);
    break;
  }
  fseek(pg_game_status.fp, bodyStart, SEEK_SET);

  if (nestedIndentation > parentIndentation)
    pg_parseChoiceBlock(nestedIndentation);
}

/// @brief Parse the options of a choice at the given indentation.
/// Lines with a greater indentation are option bodies, a line with a lower indentation ends the block.
/// On return the file is positioned at the start of the line ending the block.
static void pg_parseChoiceBlock(int blockIndentation)
{
  char line[4096];
  char command[256];
  long lineStart;
  // 0 = not in an *if chain, 1 = a branch was already used, 2 = no branch used yet
  int ifChain = 0;

  while (pg_readLine(line, sizeof(line), &lineStart)) {
    if (pg_isBlank(line) || isComment(line))
      continue;

    int lineIndentation = getIndentation(line);
    if (lineIndentation < blockIndentation)
      break;
    if (lineIndentation > blockIndentation)
      continue; // Option body

    trim(line);
    int lineLen = strlen(line);
    command[0] = '\0';
    if (line[0] == '*')
      pg_getCommand(line, command);

    if (pg_isElseCommand(command)) {
      if (ifChain == 2) {
        int res = 1;
        if (stricmp(command, "else") != 0) {
          char condition[4096];
          pg_getCommandArgs(line, condition, sizeof(condition));
          res = pg_evaluateCondition(condition, blockIndentation);
        }
        logDebug("\t%s -> %s", line, res ? "true" : "false");
        if (res) {
          ifChain = 1;
          pg_parseChoiceNestedBlock(blockIndentation);
        }
      } else if (ifChain == 0) {
        logWarning("*%s without *if in choice", command);
      }
      continue;
    }
    ifChain = 0;

    // Option modifiers: *hide_reuse, *disable_reuse, *allow_reuse, *selectable_if (cond), *if (cond)
    int start = 0;
    int reusable = runningGame->hide_reuse ? 0 : 1;
    int disableReuse = 0;
    int selectable = 1;
    int visible = 1;
    int isBlockIf = 0;
    int unsupported = 0;
    while (start < lineLen && line[start] == '*') {
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
        // Inline condition: *if (cond) #option, the condition must be enclosed in parentheses
        int condEnd = line[next] == '(' ? pg_findClosing(line, next) : -1;
        if (condEnd < 0 || indexOf(line, '#', condEnd) < 0) {
          if (stricmp(command, "if") == 0 && start == 0)
            isBlockIf = 1;
          else
            unsupported = 1;
          break;
        }

        char *condition = pg_strndup(line + next, condEnd - next + 1);
        int res = pg_evaluateCondition(condition, blockIndentation);
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
      char condition[4096];
      pg_getCommandArgs(line, condition, sizeof(condition));
      int res = pg_evaluateCondition(condition, blockIndentation);
      logDebug("\t%s -> %s", line, res ? "true" : "false");
      if (res) {
        ifChain = 1;
        pg_parseChoiceNestedBlock(blockIndentation);
      } else {
        ifChain = 2;
      }
      continue;
    }

    if (unsupported) {
      logWarning("Unsupported command in choice: %s", command);
      continue;
    }

    if (start < lineLen && line[start] == '#') {
      if (!visible) {
        logDebug("\tChoice hidden: %s", line);
        continue;
      }
      pg_addChoiceOption(line, start, blockIndentation, reusable, disableReuse, selectable);
    }
  }

  fseek(pg_game_status.fp, lineStart, SEEK_SET);
}

long pg_manageChoiceCommand()
{
  char line[4096];
  long lineStart;
  long start = ftell(pg_game_status.fp);
  int choiceIndentation = -1;

  pg_game_status.choicesCount = 0;
  // The options indentation is the indentation of the first line
  while (pg_readLine(line, sizeof(line), &lineStart)) {
    if (pg_isBlank(line) || isComment(line))
      continue;
    choiceIndentation = getIndentation(line);
    break;
  }
  fseek(pg_game_status.fp, start, SEEK_SET);

  if (choiceIndentation >= 0)
    pg_parseChoiceBlock(choiceIndentation);

  if (pg_game_status.choicesCount == 0) {
    logWarning("Empty choice");
  }
  pg_game_status.choiceEndPosition = ftell(pg_game_status.fp);
  return pg_game_status.choiceEndPosition;
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

/// @brief Format a number as ChoiceScript does (no decimals for integer values)
static void pg_formatNumber(double number, char *value)
{
  if (fabs(number - round(number)) < 1e-9)
    sprintf(value, "%li", (long)round(number));
  else
    sprintf(value, "%g", number);
}

/// @brief Assign an already evaluated value to a variable
static void pg_assignVar(variable *var, const char *value)
{
  char *tmp = pg_strndup(value, strlen(value));
  free(var->value);
  var->value = tmp;
  logDebug("Variable set %s=%s", var->name, var->value);
}

int pg_setVarValue(const char *varName, const char *value)
{
  char refName[1024];
  int nameLen = strlen(varName);
  if (nameLen > 1 && varName[0] == '{' && varName[nameLen - 1] == '}') {
    // *set {expr} value: the variable name is the value of expr
    char *content = pg_strndup(varName + 1, nameLen - 2);
    pg_evaluateReferenceName(content, refName, sizeof(refName));
    free(content);
    varName = refName;
  }

  variable *var = pg_getVar(varName);
  if (var == NULL)
    return 0;

  char expression[4096];
  snprintf(expression, sizeof(expression), "%s", value);
  trim(expression);
  int len = strlen(expression);
  char evaluated[4096];

  // Fairmath: *set var %+10, *set var %-10
  if (len > 2 && expression[0] == '%' && (expression[1] == '+' || expression[1] == '-')) {
    char *rest = expression + 2;
    double current = atof(var->value ? var->value : "0");
    double percent;
    char rightValue[1024];
    pg_evaluateExpression(rest, rightValue);
    percent = atof(rightValue);
    // Same as ChoiceScript (fairAdd): increase rounded down (max 99), decrease rounded up (min 1)
    double delta = expression[1] == '+' ? percent : -percent;
    double result;
    if (delta > 0) {
      result = floor(current + delta * (100 - current) / 100);
      if (result > 99)
        result = 99;
    } else {
      result = ceil(current - (-delta) * current / 100);
      if (result < 1)
        result = 1;
    }
    pg_formatNumber(result, evaluated);
    pg_assignVar(var, evaluated);
    return 1;
  }

  // Relative change: *set var +1, *set var - 2, *set var *(a + b), *set var &"text"
  if (len > 0 && strchr("+-*/&", expression[0]) != NULL) {
    char op = expression[0];
    char *rest = expression + 1;
    while (isspace((unsigned char)*rest))
      rest++;

    if (op == '&') {
      // String concatenation
      char rightValue[4096];
      pg_evaluateExpression(rest, rightValue);
      snprintf(evaluated, sizeof(evaluated), "%s%s", var->value ? var->value : "", rightValue);
      pg_assignVar(var, evaluated);
      return 1;
    }

    char rightValue[1024];
    pg_evaluateExpression(rest, rightValue);
    char *end;
    double right = strtod(rightValue, &end);
    if (end == rightValue) {
      logWarning("*set %s %s: invalid number %s", varName, value, rightValue);
      return 0;
    }
    double current = atof(var->value ? var->value : "0");
    double result;
    switch (op) {
      case '+': result = current + right; break;
      case '-': result = current - right; break;
      case '*': result = current * right; break;
      default:
        if (right == 0) {
          logWarning("*set %s %s: division by zero", varName, value);
          return 0;
        }
        result = current / right;
        break;
    }
    pg_formatNumber(result, evaluated);
    pg_assignVar(var, evaluated);
    return 1;
  }

  // Absolute value
  if (var->type == boolean && stricmp(expression, "true") != 0 && stricmp(expression, "false") != 0) {
    variable *other = pg_getVar(expression);
    if (other != NULL && other->value != NULL) {
      pg_assignVar(var, other->value);
    } else {
      // Boolean expression: *set var (a > b), *set var not(a)
      pg_assignVar(var, pg_evaluateBoolean(expression) ? "true" : "false");
    }
    return 1;
  }

  if (!pg_evaluateExpression(expression, evaluated)) {
    logWarning("*set %s %s: value not evaluated", varName, value);
    return 0;
  }
  pg_assignVar(var, evaluated);
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

static void pg_clearChoiceContexts(int gosubCount)
{
  while (pg_game_status.choiceContextsCount > 0 &&
         pg_game_status.choiceContexts[pg_game_status.choiceContextsCount - 1].gosubCount >= gosubCount) {
    pg_game_status.choiceContextsCount--;
  }
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
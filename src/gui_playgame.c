#include "CoGSDL.h"
#include "gui_playgame.h"

void pg_init();
void pg_dispose();
/// @brief Save the state at the start of the page (used by pg_saveGame)
static void pg_takeSnapshot();
static void pg_freeSnapshot();
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
/// @brief Manage the achieve command: mark the achievement as earned and show it
void pg_manageAchieveCommand(const char *text);
/// @brief Manage the rand command: *rand variable min max
void pg_manageRandCommand(const char *text);
/// @brief Random number in [0, 1)
static double pg_random();
/// @brief Initialize the random number generator (if needed)
static void pg_randomSeed();
static void pg_formatNumber(double number, char *value);
static void pg_assignVar(variable *var, const char *value);
static void pg_copyString(char *dst, size_t size, const char *src);
/// @brief Count a jump to a label (*looplimit)
/// @return 0 if the label was visited too many times in this page
static int pg_checkLoopLimit(long position);
void pg_appendText(const char *text, int newLine);
/// @brief Append a line that must be rendered on its own line (images, stat bars)
void pg_appendBlockLine(const char *line);
/// @brief Append the separator between two lines of the same paragraph (a space)
void pg_appendLineSeparator();
/// @brief Start a new paragraph (a single empty line)
void pg_appendParagraphBreak();
/// @brief Manage the stat_chart command (text, percent, opposed_pair rows)
void pg_manageStatChartCommand(int indentation);
/// @brief Draw a stat bar (*stat_bar value|label[|opposed label])
/// @return The height of the bar
static int pg_drawStatBar(SDL_Surface *surface, const char *line, int y);
static int pg_openScene(const char *sceneName, int closeCurrent);
static int pg_readLine(char *line, int size, long *lineStart);
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

  if (_Globals.input[_Keys.KeyStart] == 1) {
    _Globals.input[_Keys.KeyStart] = 0;
    pg_dispose();

    _Globals.status = GameSelect;
    return;
  }

  // Initialize
  if (pg_game_status.fp == NULL) {
    pg_init();
    pg_exec();
  }

  pg_drawPage();
}

void pg_drawPage()
{
  int scrollY = pg_game_status.scrollY;
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

  if (scrollY != pg_game_status.scrollY) {
    SDL_FreeSurface(text_surface);
    text_surface = NULL;
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

/// @brief The game state saved while the stats screen is open
static game_status pg_savedGameStatus;
static int pg_statsActive = 0;

int pg_isStatsActive()
{
  return pg_statsActive;
}

int pg_statsOpen()
{
  if (pg_statsActive)
    return 1;
  if (runningGame == NULL || pg_game_status.fp == NULL)
    return 0;

  char filePath[512];
  snprintf(filePath, sizeof(filePath), "%s/scenes/choicescript_stats.txt", runningGame->path);
  if (!fileExists(filePath)) {
    logInfo("No stats scene: %s", filePath);
    return 0;
  }

  // Save the game state: the stats scene has its own file, text, choices and temporary variables
  pg_savedGameStatus = pg_game_status;
  memset(&pg_game_status, 0, sizeof(pg_game_status));
  pg_game_status.labelsCount = -1;
  pg_game_status.selectedChoiceIndex = -1;
  pg_game_status.sceneIndex = pg_savedGameStatus.sceneIndex;
  if (text_surface) {
    SDL_FreeSurface(text_surface);
    text_surface = NULL;
  }
  pg_statsActive = 1;

  if (!pg_openScene("choicescript_stats", 0)) {
    pg_game_status = pg_savedGameStatus;
    pg_statsActive = 0;
    return 0;
  }
  logInfo("Stats screen opened");
  pg_exec();
  return 1;
}

void pg_statsClose()
{
  if (!pg_statsActive)
    return;

  // Free the stats state
  pg_clearGosubStack();
  pg_clearParams();
  pg_clearTempVars();
  pg_clearChoices();
  pg_clear_labels();
  free(pg_game_status.text);
  pg_game_status.text = NULL;
  if (pg_game_status.fp)
    fclose(pg_game_status.fp);
  if (text_surface) {
    SDL_FreeSurface(text_surface);
    text_surface = NULL;
  }
  // The images of the game page are loaded again when needed
  pg_clear_image_cache();

  pg_game_status = pg_savedGameStatus;
  memset(&pg_savedGameStatus, 0, sizeof(pg_savedGameStatus));
  pg_statsActive = 0;
  logInfo("Stats screen closed");
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
  // *hide_reuse in the startup header is read when the game is loaded
  pg_game_status.choiceReuse = runningGame->hide_reuse ? reuse_hide : reuse_allow;
  pg_game_status.loopLimit = 1000;
  pg_game_status.labelVisitsCount = 0;
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
  pg_freeSnapshot();
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
    if (startsWithWord("*stat_bar", line)) {
      int barHeight = 2 * line_height;
      if (y + barHeight > height) {
        pg_game_status.textFullyRendered = 0;
        free(line);
        break;
      }
      y += pg_drawStatBar(surface, line, y);
      x = TEXT_MARGIN;
      free(line);
      continue;
    }

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
    if (pg_game_status.text == NULL && !pg_statsActive)
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
  // Same as ChoiceScript: *looplimit counts the visits to a label in a page
  pg_game_status.labelVisitsCount = 0;

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

      if (stricmp(command, "hide_reuse") == 0 || stricmp(command, "disable_reuse") == 0 || stricmp(command, "allow_reuse") == 0) {
        // Default reuse of the options of the choices in this scene
        pg_game_status.choiceReuse = stricmp(command, "hide_reuse") == 0 ? reuse_hide :
                                     stricmp(command, "disable_reuse") == 0 ? reuse_disable : reuse_allow;
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "looplimit") == 0) {
        // looplimit N: maximum number of visits to a label in a page
        char args[256];
        char value[1024];
        pg_getCommandArgs(line, args, sizeof(args));
        pg_evaluateExpression(args, value);
        int limit;
        if (strlen(value) > 0 && is_number(value, &limit) && limit >= 0)
          pg_game_status.loopLimit = limit;
        else
          logWarning("Invalid looplimit: %s", args);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "rand") == 0) {
        // rand variable min max
        pg_manageRandCommand(line);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "achieve") == 0) {
        // achieve name
        pg_manageAchieveCommand(line);
        textStarted = 1;
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "feedback") == 0 || stricmp(command, "subscribe") == 0) {
        // Online features (rating request, newsletter subscription): nothing to do
        logDebug("*%s ignored", command);
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
        if (!pg_checkLoopLimit(newPosition))
          break;

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
        // Same as ChoiceScript: a page break on an empty screen is ignored
        if (pg_isBlank(pg_game_status.text ? pg_game_status.text : "")) {
          logDebug("page_break ignored: empty screen");
          pg_game_status.fpPosition = ftell(pg_game_status.fp);
          continue;
        }
        // page_break [button text]
        char message[sizeof(pg_game_status.pageBreakMessage)];
        pg_getCommandArgs(line, message, sizeof(message));
        pg_game_status.showPageBreak = 1;
        snprintf(pg_game_status.pageBreakMessage, sizeof(pg_game_status.pageBreakMessage), "%s", strlen(message) ? message : "Next");
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        break;
      }

      if (stricmp(command, "stat_chart") == 0) {
        pg_manageStatChartCommand(lineIndentation);
        textStarted = 1;
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "line_break") == 0) {
        pg_appendText("\n", 0);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "text_image") == 0 || stricmp(command, "image") == 0) {
        // Add the command to the text as it should be rendered with it (on its own line)
        pg_appendBlockLine(line);
        textStarted = 1;
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "finish") == 0 && pg_statsActive) {
        // End of the stats scene
        pg_game_status.gameEnded = 1;
        break;
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
        if (!pg_checkLoopLimit(newPosition))
          break;

        continue;
      }

      if (stricmp(command, "gosub") == 0) {
        //gosub
        long newPosition = pg_manageGosubCommand(line);
        logDebug("%s -> %i", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        if (!pg_checkLoopLimit(newPosition))
          break;

        continue;
      }

      if (stricmp(command, "gosub_scene") == 0) {
        //gosub_scene
        long newPosition = pg_manageGosubSceneCommand(line);
        logDebug("%s -> %i", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        if (!pg_checkLoopLimit(newPosition))
          break;

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
        if (strlen(textLine) > 0) {
          if (pg_needsNewLine(textLine) && !endsWith("\n", pg_game_status.text))
            pg_appendText("\n", 0);
          else
            pg_appendLineSeparator();
          pg_appendText(textLine, 0);
          textStarted = 1;
        }
        free(expanded);
      } else if(textStarted) {
        // New paragraph
        pg_appendParagraphBreak();
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

  // Nothing to show at the end of the file: go to the next scene
  // (a choice or a page break at the end of the file must be shown)
  if (pg_game_status.text == NULL && pg_game_status.choicesCount == 0 && !pg_game_status.showPageBreak &&
      !pg_game_status.gameEnded && pg_game_status.fpPosition == pg_game_status.fpSize) {
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
  // The saves contain the state at the start of the page
  if (!pg_statsActive)
    pg_takeSnapshot();
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

void pg_appendLineSeparator()
{
  // Same as ChoiceScript: the lines of a paragraph are joined with a space,
  // unless the previous line ends with a hyphen or a dash
  const char *text = pg_game_status.text;
  if (text == NULL)
    return;
  int len = strlen(text);
  if (len == 0 || text[len - 1] == '\n' || text[len - 1] == ' ' || text[len - 1] == '-')
    return;
  // U+2011 - U+2014 (non-breaking hyphen, figure dash, en dash, em dash)
  if (len >= 3 && (unsigned char)text[len - 3] == 0xE2 && (unsigned char)text[len - 2] == 0x80 &&
      (unsigned char)text[len - 1] >= 0x91 && (unsigned char)text[len - 1] <= 0x94)
    return;
  pg_appendText(" ", 0);
}

void pg_appendParagraphBreak()
{
  // A paragraph break is a single empty line, even after several empty lines
  const char *text = pg_game_status.text;
  if (text == NULL)
    return;
  int len = strlen(text);
  int newLines = 0;
  while (newLines < len && newLines < 2 && text[len - 1 - newLines] == '\n')
    newLines++;
  if (len == 0 || newLines >= 2)
    return;
  pg_appendText(newLines == 1 ? "\n" : "\n\n", 0);
}

void pg_appendBlockLine(const char *line)
{
  if (pg_game_status.text != NULL && strlen(pg_game_status.text) > 0 && !endsWith("\n", pg_game_status.text))
    pg_appendText("\n", 0);
  pg_appendText(line, 1);
}

/// @brief Mix two colors
static SDL_Color pg_mixColor(SDL_Color c1, SDL_Color c2, int percent2)
{
  SDL_Color res = {
    (c1.r * (100 - percent2) + c2.r * percent2) / 100,
    (c1.g * (100 - percent2) + c2.g * percent2) / 100,
    (c1.b * (100 - percent2) + c2.b * percent2) / 100,
    255
  };
  return res;
}

static void pg_drawStatText(SDL_Surface *surface, const char *text, int x, int y)
{
  if (!text || !strlen(text))
    return;
  SDL_Surface *textSurface = TTF_RenderUTF8_Blended(_Globals.textFont, text, _theme->textColor);
  if (textSurface == NULL)
    return;
  SDL_Rect dest = { x, y, textSurface->w, textSurface->h };
  SDL_BlitSurface(textSurface, NULL, surface, &dest);
  SDL_FreeSurface(textSurface);
}

static int pg_drawStatBar(SDL_Surface *surface, const char *line, int y)
{
  // *stat_bar value|label[|opposed label]
  char data[1024];
  snprintf(data, sizeof(data), "%s", line + strlen("*stat_bar "));
  char *label = strchr(data, '|');
  char *opposedLabel = NULL;
  if (label != NULL) {
    *label++ = '\0';
    opposedLabel = strchr(label, '|');
    if (opposedLabel != NULL)
      *opposedLabel++ = '\0';
  }
  double value = atof(data);
  if (value < 0)
    value = 0;
  if (value > 100)
    value = 100;

  int barX = TEXT_MARGIN;
  int barWidth = surface->w - 2 * TEXT_MARGIN;

  // Labels on the first line, the opposed label is right aligned
  pg_drawStatText(surface, label, barX, y);
  if (opposedLabel != NULL && strlen(opposedLabel)) {
    int width = 0;
    TTF_MeasureUTF8(_Globals.textFont, opposedLabel, surface->w, &width, NULL);
    pg_drawStatText(surface, opposedLabel, barX + barWidth - width, y);
  }

  // Bar on the second line
  int barHeight = line_height / 2;
  SDL_Rect bar = { barX, y + line_height + (line_height - barHeight) / 2, barWidth, barHeight };
  SDL_Color emptyColor = pg_mixColor(_theme->backgroundColor, _theme->textColor, 20);
  SDL_FillRect(surface, &bar, SDL_MapRGBA(surface->format, emptyColor.r, emptyColor.g, emptyColor.b, 255));
  bar.w = (int)(barWidth * value / 100);
  if (bar.w > 0)
    SDL_FillRect(surface, &bar, SDL_MapRGBA(surface->format, _theme->selectedTextColor.r, _theme->selectedTextColor.g, _theme->selectedTextColor.b, 255));

  return 2 * line_height;
}

/// @brief Evaluate a stat value for *stat_chart (number formatted without decimals)
static void pg_statValue(const char *variable, char *value)
{
  pg_evaluateExpression(variable, value);
  trim(value);
}

void pg_manageStatChartCommand(int indentation)
{
  char line[4096];
  char type[32];
  long lineStart;
  int rowIndentation = -1;

  // Same as ChoiceScript: the chart starts a new paragraph
  if (pg_game_status.text != NULL && strlen(pg_game_status.text) > 0 && !endsWith("\n\n", pg_game_status.text))
    pg_appendText(endsWith("\n", pg_game_status.text) ? "\n" : "\n\n", 0);

  while (pg_readLine(line, sizeof(line), &lineStart)) {
    if (pg_isBlank(line) || isComment(line))
      continue;

    int lineIndentation = getIndentation(line);
    if (lineIndentation <= indentation)
      break;
    if (rowIndentation < 0)
      rowIndentation = lineIndentation;
    if (lineIndentation > rowIndentation)
      continue; // Definition of the previous row

    trim(line);
    int typeLen = 0;
    while (line[typeLen] && !isspace((unsigned char)line[typeLen]) && typeLen < (int)sizeof(type) - 1) {
      type[typeLen] = line[typeLen];
      typeLen++;
    }
    type[typeLen] = '\0';
    char *data = line + typeLen;
    while (isspace((unsigned char)*data))
      data++;

    char variable[1024];
    char label[1024];
    char value[1024];
    char row[4096];
    if (stricmp(type, "opposed_pair") == 0) {
      snprintf(variable, sizeof(variable), "%s", data);
      // Labels: indented lines, their definitions are indented more
      char labels[2][1024];
      int labelsCount = 0;
      int labelIndentation = -1;
      char subLine[4096];
      long subStart;
      while (pg_readLine(subLine, sizeof(subLine), &subStart)) {
        if (pg_isBlank(subLine) || isComment(subLine))
          continue;
        int subIndentation = getIndentation(subLine);
        if (subIndentation <= rowIndentation) {
          fseek(pg_game_status.fp, subStart, SEEK_SET);
          break;
        }
        if (labelIndentation < 0)
          labelIndentation = subIndentation;
        if (subIndentation > labelIndentation)
          continue; // Definition
        trim(subLine);
        if (labelsCount < 2) {
          // Labels longer than the buffer are truncated
          subLine[sizeof(labels[0]) - 1] = '\0';
          strcpy(labels[labelsCount++], subLine);
        }
      }

      const char *left = labelsCount == 2 ? labels[0] : variable;
      const char *right = labelsCount == 2 ? labels[1] : labelsCount == 1 ? labels[0] : "";
      char *leftExpanded = pg_expandVariables(left);
      char *rightExpanded = pg_expandVariables(right);
      pg_statValue(variable, value);
      double number = atof(value);
      snprintf(row, sizeof(row), "*stat_bar %s|%s: %s%%|%s: %g%%", value, leftExpanded, value, rightExpanded, 100 - number);
      free(leftExpanded);
      free(rightExpanded);
      pg_appendBlockLine(row);
      continue;
    }

    if (stricmp(type, "text") != 0 && stricmp(type, "percent") != 0) {
      logWarning("*stat_chart: invalid row %s", line);
      continue;
    }

    // variable [label], the variable can be an expression in parentheses
    int variableEnd;
    if (data[0] == '(') {
      variableEnd = pg_findClosing(data, 0);
      variableEnd = variableEnd < 0 ? (int)strlen(data) : variableEnd + 1;
    } else {
      variableEnd = 0;
      while (data[variableEnd] && !isspace((unsigned char)data[variableEnd]))
        variableEnd++;
    }
    snprintf(variable, sizeof(variable), "%.*s", variableEnd, data);
    const char *labelStart = data + variableEnd;
    while (isspace((unsigned char)*labelStart))
      labelStart++;
    snprintf(label, sizeof(label), "%s", strlen(labelStart) ? labelStart : variable);
    char *labelExpanded = pg_expandVariables(label);
    pg_statValue(variable, value);

    if (stricmp(type, "text") == 0) {
      snprintf(row, sizeof(row), "%s: %s", labelExpanded, value);
      pg_appendBlockLine(row);
    } else {
      snprintf(row, sizeof(row), "*stat_bar %s|%s: %s%%", value, labelExpanded, value);
      pg_appendBlockLine(row);
    }
    free(labelExpanded);
  }
  if (feof(pg_game_status.fp))
    clearerr(pg_game_status.fp);
  else
    fseek(pg_game_status.fp, lineStart, SEEK_SET);
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

  // Same as ChoiceScript: the command name contains only letters, digits and underscores,
  // so "*if(condition)" is the command "if" followed by "(condition)"
  int idx = 0;
  if (line[0] == '*') {
    for (int i = 1; line[i] && idx < 255; i++) {
      if (!isalnum((unsigned char)line[i]) && line[i] != '_')
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
  // Command name: letters, digits and underscores (see pg_getCommand)
  while (line[i] && (isalnum((unsigned char)line[i]) || line[i] == '_'))
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
  // Same as ChoiceScript: these settings are temporary values of the scene
  pg_game_status.choiceReuse = reuse_allow;
  pg_game_status.loopLimit = 1000;
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
  if (pg_statsActive) {
    // End of the stats scene
    pg_game_status.gameEnded = 1;
    return 0;
  }

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
    frame->choiceReuse = pg_game_status.choiceReuse;
    frame->loopLimit = pg_game_status.loopLimit;
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
    pg_game_status.choiceReuse = frame->choiceReuse;
    pg_game_status.loopLimit = frame->loopLimit;
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

static int pg_checkLoopLimit(long position)
{
  unsigned int hash = 5381;
  for (const char *c = pg_game_status.sceneName; *c; c++)
    hash = hash * 33 + (unsigned char)tolower((unsigned char)*c);

  label_visit *visit = NULL;
  for (int i = 0; i < pg_game_status.labelVisitsCount; i++) {
    if (pg_game_status.labelVisits[i].position == position && pg_game_status.labelVisits[i].sceneHash == hash) {
      visit = &pg_game_status.labelVisits[i];
      break;
    }
  }
  if (visit == NULL) {
    if (pg_game_status.labelVisitsCount >= (int)(sizeof(pg_game_status.labelVisits) / sizeof(label_visit)))
      return 1; // Table full: stop tracking new labels
    visit = &pg_game_status.labelVisits[pg_game_status.labelVisitsCount++];
    visit->sceneHash = hash;
    visit->position = position;
    visit->count = 0;
  }

  visit->count++;
  // Same as ChoiceScript: 0 means no limit
  if (pg_game_status.loopLimit > 0 && visit->count > pg_game_status.loopLimit) {
    logError("Infinite loop: position %li of scene %s visited too many times (%i)", position, pg_game_status.sceneName, pg_game_status.loopLimit);
    char message[512];
    snprintf(message, sizeof(message), "\n\n[b]Error:[/b] infinite loop in scene %s (a label was visited more than %i times). The game cannot continue.",
             pg_game_status.sceneName, pg_game_status.loopLimit);
    pg_appendText(message, 0);
    pg_game_status.gameEnded = 1;
    pg_game_status.showPageBreak = 0;
    return 0;
  }
  return 1;
}

void pg_manageAchieveCommand(const char *text)
{
  char name[256];
  pg_getCommandArgs(text, name, sizeof(name));

  achievement *ach = NULL;
  for (int i = 0; i < runningGame->achievementsCount; i++) {
    if (stricmp(runningGame->achievements[i].name, name) == 0) {
      ach = &runningGame->achievements[i];
      break;
    }
  }
  if (ach == NULL) {
    logWarning("Achievement %s not declared in startup", name);
    return;
  }
  if (ach->achieved)
    return;

  ach->achieved = 1;
  const char *description = ach->postEarnedDescription ? ach->postEarnedDescription : ach->preEarnedDescription;
  logInfo("Achievement earned: %s (%s)", ach->name, ach->title);

  // Show the achievement in the text of the page
  char message[1024];
  snprintf(message, sizeof(message), "%s[b]Achievement: %s[/b]%s%s\n\n",
           pg_game_status.text && !endsWith("\n", pg_game_status.text) ? "\n\n" : "",
           ach->title ? ach->title : ach->name,
           description ? " - " : "",
           description ? description : "");
  pg_appendText(message, 0);
}

/// @brief State of the random number generator (xorshift64*), saved with the game
static uint64_t pg_randomState = 0;

static void pg_randomSeed()
{
  if (pg_randomState == 0) {
    pg_randomState = ((uint64_t)time(NULL) << 32) ^ (uint64_t)clock() ^ 0x9E3779B97F4A7C15ULL;
    if (pg_randomState == 0)
      pg_randomState = 1;
  }
}

static double pg_random()
{
  pg_randomSeed();
  pg_randomState ^= pg_randomState >> 12;
  pg_randomState ^= pg_randomState << 25;
  pg_randomState ^= pg_randomState >> 27;
  uint64_t value = pg_randomState * 0x2545F4914F6CDD1DULL;
  // 53 random bits -> [0, 1)
  return (value >> 11) * (1.0 / 9007199254740992.0);
}

void pg_manageRandCommand(const char *text)
{
  char args[4096];
  char *tokens[8];
  pg_getCommandArgs(text, args, sizeof(args));
  int count = pg_tokenizeArgs(args, tokens, 8);
  if (count != 3) {
    logWarning("Invalid rand statement, expected three args: varname min max (%s)", text);
    pg_freeTokens(tokens, count);
    return;
  }

  // The variable can be a reference: *rand {name} 1 6
  char name[1024];
  int nameLen = strlen(tokens[0]);
  if (nameLen > 1 && tokens[0][0] == '{' && tokens[0][nameLen - 1] == '}') {
    char *content = pg_strndup(tokens[0] + 1, nameLen - 2);
    pg_evaluateReferenceName(content, name, sizeof(name));
    free(content);
  } else {
    pg_copyString(name, sizeof(name), tokens[0]);
  }
  variable *var = pg_getVar(name);

  char minValue[1024];
  char maxValue[1024];
  pg_evaluateExpression(tokens[1], minValue);
  pg_evaluateExpression(tokens[2], maxValue);
  trim(minValue);
  trim(maxValue);
  pg_freeTokens(tokens, count);
  if (var == NULL) {
    logWarning("*rand: non-existent variable %s", name);
    return;
  }

  char *end1, *end2;
  double minimum = strtod(minValue, &end1);
  double maximum = strtod(maxValue, &end2);
  if (end1 == minValue || end2 == maxValue || *end1 || *end2) {
    logWarning("*rand: min and max must be numbers (%s, %s)", minValue, maxValue);
    return;
  }
  if (maximum < minimum) {
    logWarning("*rand: min must be less than max: %s > %s", minValue, maxValue);
    return;
  }

  // Same as ChoiceScript: an integer between min and max (included) if both are integers
  double result;
  int intValue;
  if (maximum == minimum)
    result = minimum;
  else if (is_number(minValue, &intValue) && is_number(maxValue, &intValue) && !strchr(minValue, '.') && !strchr(maxValue, '.'))
    result = minimum + floor(pg_random() * (maximum - minimum + 1));
  else
    result = minimum + pg_random() * (maximum - minimum);

  char value[64];
  pg_formatNumber(result, value);
  pg_assignVar(var, value);
  var->type = number;
  logDebug("*rand %s %s %s -> %s", name, minValue, maxValue, value);
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
    // Default of the scene, an option modifier overrides it
    int reusable = pg_game_status.choiceReuse == reuse_hide ? 0 : 1;
    int disableReuse = pg_game_status.choiceReuse == reuse_disable ? 1 : 0;
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

  // Absolute value. Variables are dynamically typed (as in ChoiceScript): the type follows the value
  char opr[3];
  int isBooleanExpression = expression[0] != '"' &&
                            (pg_get_operator(expression, opr) >= 0 ||
                             pg_strnicmp(expression, "not", 3) == 0 ||
                             strstr(expression, " and ") != NULL || strstr(expression, " or ") != NULL ||
                             strstr(expression, ")and") != NULL || strstr(expression, ")or") != NULL);
  if (isBooleanExpression) {
    // Boolean expression: *set var (a > b), *set var not(a), *set var a and b
    pg_assignVar(var, pg_evaluateBoolean(expression) ? "true" : "false");
    var->type = boolean;
    return 1;
  }

  if (!pg_evaluateExpression(expression, evaluated)) {
    logWarning("*set %s %s: value not evaluated", varName, value);
    return 0;
  }
  pg_assignVar(var, evaluated);

  int intValue;
  if (expression[0] == '"')
    var->type = string;
  else if (stricmp(evaluated, "true") == 0 || stricmp(evaluated, "false") == 0)
    var->type = boolean;
  else if (strlen(evaluated) > 0 && is_number(evaluated, &intValue))
    var->type = number;
  else
    var->type = string;
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

/* ---------------------------------------------------------------------------
 * Save / load
 *
 * As in ChoiceScript, the saved state is the state at the start of the current
 * page: loading a game executes the page again, showing the same text and
 * choices without applying twice the commands of the page (*set...).
 *
 * The saves are text files: ./saves/<game folder>/slot<N>.sav
 * ------------------------------------------------------------------------- */

#define SAVE_HEADER "CoGSDL save 1"

/// @brief Copy a string, truncated to the buffer size
static void pg_copyString(char *dst, size_t size, const char *src)
{
  size_t len = strlen(src);
  if (len >= size)
    len = size - 1;
  memcpy(dst, src, len);
  dst[len] = '\0';
}

/// @brief Append a string, truncated to the buffer size
static void pg_appendString(char *dst, size_t size, const char *src)
{
  size_t len = strlen(dst);
  if (len < size)
    pg_copyString(dst + len, size - len, src);
}

/// @brief The state at the start of the current page (serialized)
static char *pg_pageSnapshot = NULL;
/// @brief The scene at the start of the current page (used for the name of the save)
static char pg_snapshotScene[256];

static void pg_sbPrintf(pg_strbuf *sb, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int len = vsnprintf(NULL, 0, format, args);
  va_end(args);
  if (len <= 0)
    return;

  char *tmp = malloc(len + 1);
  if (tmp == NULL)
    return;
  va_start(args, format);
  vsnprintf(tmp, len + 1, format, args);
  va_end(args);
  pg_sb_append(sb, tmp, len);
  free(tmp);
}

/// @brief Append a value escaping backslashes and new lines
static void pg_sbEscaped(pg_strbuf *sb, const char *value)
{
  if (value == NULL)
    return;
  for (const char *c = value; *c; c++) {
    if (*c == '\\')
      pg_sb_append(sb, "\\\\", 2);
    else if (*c == '\n')
      pg_sb_append(sb, "\\n", 2);
    else if (*c == '\r')
      pg_sb_append(sb, "\\r", 2);
    else
      pg_sb_append(sb, c, 1);
  }
}

static void pg_unescape(char *value)
{
  char *out = value;
  for (char *c = value; *c; c++) {
    if (*c == '\\' && c[1]) {
      c++;
      *out++ = *c == 'n' ? '\n' : *c == 'r' ? '\r' : *c;
    } else {
      *out++ = *c;
    }
  }
  *out = '\0';
}

static void pg_sbVariable(pg_strbuf *sb, const char *prefix, variable *var)
{
  if (var->name == NULL)
    return;
  pg_sbPrintf(sb, "%s %i %s ", prefix, (int)var->type, var->name);
  pg_sbEscaped(sb, var->value ? var->value : "");
  pg_sb_append(sb, "\n", 1);
}

static void pg_takeSnapshot()
{
  if (runningGame == NULL || pg_game_status.fp == NULL)
    return;

  pg_strbuf sb = { NULL, 0, 0 };
  pg_sbPrintf(&sb, "scene %s\n", pg_game_status.sceneName);
  pg_sbPrintf(&sb, "sceneindex %i\n", pg_game_status.sceneIndex);
  // After *finish the scene is ended (fpPosition = fpSize) even if the file is not at its end
  long position = pg_game_status.fpPosition == pg_game_status.fpSize ? pg_game_status.fpSize : ftell(pg_game_status.fp);
  pg_sbPrintf(&sb, "position %li\n", position);
  pg_sbPrintf(&sb, "reuse %i\n", (int)pg_game_status.choiceReuse);
  pg_sbPrintf(&sb, "looplimit %i\n", pg_game_status.loopLimit);
  pg_sbPrintf(&sb, "ended %i\n", pg_game_status.gameEnded);
  // The random generator is saved too: loading shows the same results of *rand
  pg_randomSeed();
  pg_sbPrintf(&sb, "random %llu\n", (unsigned long long)pg_randomState);

  for (int i = 0; i < runningGame->variablesCount; i++)
    pg_sbVariable(&sb, "var", &runningGame->variables[i]);
  for (int i = 0; i < pg_game_status.tempVariablesCount; i++)
    pg_sbVariable(&sb, "temp", &pg_game_status.tempVariables[i]);
  for (int i = 0; i < pg_game_status.paramsCount; i++) {
    pg_sb_append(&sb, "param ", 6);
    pg_sbEscaped(&sb, pg_game_status.params[i]);
    pg_sb_append(&sb, "\n", 1);
  }
  for (int i = 0; i < pg_game_status.used_choices_count; i++)
    pg_sbPrintf(&sb, "used %s\n", pg_game_status.used_choices[i]);
  for (int i = 0; i < runningGame->achievementsCount; i++) {
    if (runningGame->achievements[i].achieved)
      pg_sbPrintf(&sb, "achieved %s\n", runningGame->achievements[i].name);
  }
  for (int i = 0; i < pg_game_status.choiceContextsCount; i++) {
    choice_context *ctx = &pg_game_status.choiceContexts[i];
    pg_sbPrintf(&sb, "context %li %i %i\n", ctx->endPosition, ctx->indentation, ctx->gosubCount);
  }
  for (int i = 1; i <= pg_game_status.gosubCount; i++) {
    gosub_info *frame = &pg_game_status.gosub[i];
    pg_sbPrintf(&sb, "frame %i %i %li %i %i %i %s\n", i, frame->fp != NULL, frame->position, frame->sceneIndex,
                (int)frame->choiceReuse, frame->loopLimit, frame->fp != NULL ? frame->sceneName : "-");
    for (int j = 0; j < frame->tempVariablesCount; j++) {
      char prefix[32];
      snprintf(prefix, sizeof(prefix), "frametemp %i", i);
      pg_sbVariable(&sb, prefix, &frame->tempVariables[j]);
    }
  }

  free(pg_pageSnapshot);
  pg_pageSnapshot = sb.buf;
  pg_copyString(pg_snapshotScene, sizeof(pg_snapshotScene), pg_game_status.sceneName);
}

static void pg_freeSnapshot()
{
  free(pg_pageSnapshot);
  pg_pageSnapshot = NULL;
}

/// @brief Get the folder of the saves of the running game (./saves/<game folder>)
static void pg_getSaveFolder(char *folder, int size)
{
  const char *name = runningGame->path;
  for (const char *c = runningGame->path; *c; c++) {
    if ((*c == '/' || *c == '\\') && c[1])
      name = c + 1;
  }
  snprintf(folder, size, "./saves/%s", name);
  // Remove a trailing separator
  int len = strlen(folder);
  if (len > 0 && (folder[len - 1] == '/' || folder[len - 1] == '\\'))
    folder[len - 1] = '\0';
}

static void pg_getSavePath(int slot, char *path, int size)
{
  char folder[512];
  pg_getSaveFolder(folder, sizeof(folder));
  snprintf(path, size, "%s/slot%i.sav", folder, slot + 1);
}

static int pg_makeFolder(const char *path)
{
  if (fileExists(path))
    return 1;
#ifdef _WIN32
  return mkdir(path) == 0;
#else
  return mkdir(path, 0755) == 0;
#endif
}

/// @brief Build the name of a save from the position in the game: scene name and beginning of the page
static void pg_buildSaveName(char *name, int size)
{
  // Text of the page without commands (images, stat bars) and formatting
  pg_strbuf sb = { NULL, 0, 0 };
  const char *text = pg_game_status.text;
  if ((text == NULL || pg_isBlank(text)) && pg_game_status.choicesCount > 0)
    text = pg_game_status.choices[0].text;
  if (text != NULL) {
    int lineStart = 1;
    int lastSpace = 1;
    for (const char *c = text; *c; c++) {
      if (lineStart && *c == '*') {
        // Skip the command line
        while (*c && *c != '\n')
          c++;
        if (!*c)
          break;
        continue;
      }
      lineStart = *c == '\n';
      if (*c == '[') {
        const char *close = strchr(c, ']');
        if (close != NULL && close - c <= 3) {
          c = close;
          continue;
        }
      }
      if (isspace((unsigned char)*c)) {
        if (!lastSpace)
          pg_sb_append(&sb, " ", 1);
        lastSpace = 1;
      } else {
        pg_sb_append(&sb, c, 1);
        lastSpace = 0;
      }
    }
  }

  char excerpt[128] = "";
  if (sb.buf != NULL) {
    trim(sb.buf);
    const int maxLen = 45;
    int len = strlen(sb.buf);
    if (len <= maxLen) {
      snprintf(excerpt, sizeof(excerpt), "%s", sb.buf);
    } else {
      // Cut at a word boundary (or at a UTF-8 character boundary)
      int cut = maxLen;
      while (cut > maxLen / 2 && sb.buf[cut] != ' ')
        cut--;
      if (sb.buf[cut] != ' ') {
        cut = maxLen;
        while (cut > 0 && !isutf(sb.buf[cut]))
          cut--;
      }
      snprintf(excerpt, sizeof(excerpt), "%.*s...", cut, sb.buf);
    }
    free(sb.buf);
  }

  pg_copyString(name, size, pg_snapshotScene);
  if (strlen(excerpt)) {
    pg_appendString(name, size, ": ");
    pg_appendString(name, size, excerpt);
  }
}

int pg_getSaveInfo(int slot, save_info *info)
{
  memset(info, 0, sizeof(save_info));
  if (runningGame == NULL || slot < 0 || slot >= SAVE_SLOTS)
    return 0;

  char path[600];
  pg_getSavePath(slot, path, sizeof(path));
  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return 0;

  char line[1024];
  int valid = 0;
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (!valid) {
      valid = strcmp(line, SAVE_HEADER) == 0;
      if (!valid)
        break;
    } else if (startsWith("name ", line)) {
      pg_copyString(info->name, sizeof(info->name), line + 5);
      pg_unescape(info->name);
    } else if (startsWith("time ", line)) {
      info->time = (time_t)atoll(line + 5);
      break;
    }
  }
  fclose(fp);

  info->used = valid;
  return valid;
}

int pg_saveGame(int slot)
{
  if (runningGame == NULL || pg_pageSnapshot == NULL || slot < 0 || slot >= SAVE_SLOTS) {
    logWarning("Nothing to save");
    return 0;
  }

  char folder[512];
  pg_getSaveFolder(folder, sizeof(folder));
  if (!pg_makeFolder("./saves") || !pg_makeFolder(folder)) {
    logError("Cannot create the folder %s", folder);
    return 0;
  }

  char name[256];
  pg_buildSaveName(name, sizeof(name));

  char path[600];
  char tmpPath[610];
  pg_getSavePath(slot, path, sizeof(path));
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);

  FILE *fp = fopen(tmpPath, "wb");
  if (fp == NULL) {
    logError("Cannot write %s", tmpPath);
    return 0;
  }

  pg_strbuf sb = { NULL, 0, 0 };
  const char *gameName = folder + strlen("./saves/");
  pg_sbPrintf(&sb, "%s\n", SAVE_HEADER);
  pg_sbPrintf(&sb, "game %s\n", gameName);
  pg_sb_append(&sb, "name ", 5);
  pg_sbEscaped(&sb, name);
  pg_sb_append(&sb, "\n", 1);
  pg_sbPrintf(&sb, "time %lld\n", (long long)time(NULL));
  pg_sb_append(&sb, pg_pageSnapshot, strlen(pg_pageSnapshot));
  pg_sbPrintf(&sb, "end\n");

  int ok = fwrite(sb.buf, 1, sb.len, fp) == sb.len;
  ok = (fclose(fp) == 0) && ok;
  free(sb.buf);
  if (!ok) {
    logError("Cannot write %s", tmpPath);
    remove(tmpPath);
    return 0;
  }

  // Replace the old save only when the new one is complete
  remove(path);
  if (rename(tmpPath, path) != 0) {
    logError("Cannot rename %s to %s", tmpPath, path);
    return 0;
  }
  logInfo("Game saved in %s (%s)", path, name);
  return 1;
}

static int pg_sceneExists(const char *sceneName)
{
  char path[512];
  snprintf(path, sizeof(path), "%s/scenes/%s.txt", runningGame->path, sceneName);
  return fileExists(path);
}

/// @brief Parse "<type> <name> <value>" of a saved variable
/// @return 1 on success (name and value point inside the line)
static int pg_parseSavedVariable(char *data, int *type, char **name, char **value)
{
  char *space = strchr(data, ' ');
  if (space == NULL)
    return 0;
  *space = '\0';
  *type = atoi(data);
  *name = space + 1;
  space = strchr(*name, ' ');
  if (space == NULL) {
    *value = *name + strlen(*name);
  } else {
    *space = '\0';
    *value = space + 1;
  }
  pg_unescape(*value);
  return strlen(*name) > 0;
}

int pg_loadGame(int slot)
{
  if (runningGame == NULL || slot < 0 || slot >= SAVE_SLOTS)
    return 0;

  char path[600];
  pg_getSavePath(slot, path, sizeof(path));
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    logWarning("Save not found: %s", path);
    return 0;
  }
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *content = malloc(size + 1);
  if (content == NULL || fread(content, 1, size, fp) != (size_t)size) {
    fclose(fp);
    free(content);
    return 0;
  }
  fclose(fp);
  content[size] = '\0';

  // Split the lines
  int linesCount = 0;
  for (long i = 0; i < size; i++) {
    if (content[i] == '\n')
      linesCount++;
  }
  char **lines = malloc(sizeof(char *) * (linesCount + 1));
  linesCount = 0;
  char *start = content;
  for (char *c = content; ; c++) {
    if (*c == '\n' || *c == '\0') {
      int last = *c == '\0';
      *c = '\0';
      if (c > start && c[-1] == '\r')
        c[-1] = '\0';
      if (*start)
        lines[linesCount++] = start;
      if (last)
        break;
      start = c + 1;
    }
  }

  // Validate before changing the state of the game
  int valid = linesCount > 0 && strcmp(lines[0], SAVE_HEADER) == 0 && strcmp(lines[linesCount - 1], "end") == 0;
  const char *sceneName = NULL;
  for (int i = 1; valid && i < linesCount; i++) {
    if (startsWith("scene ", lines[i])) {
      sceneName = lines[i] + 6;
      valid = pg_sceneExists(sceneName);
    } else if (startsWith("frame ", lines[i])) {
      int index, isScene, sceneIndex, reuse, loopLimit;
      long position;
      int nameStart = 0;
      if (sscanf(lines[i] + 6, "%i %i %li %i %i %i %n", &index, &isScene, &position, &sceneIndex, &reuse, &loopLimit, &nameStart) < 6 ||
          index < 1 || index >= (int)(sizeof(pg_game_status.gosub) / sizeof(gosub_info)) ||
          (isScene && !pg_sceneExists(lines[i] + 6 + nameStart)))
        valid = 0;
    }
  }
  if (!valid || sceneName == NULL) {
    logError("Invalid save: %s", path);
    free(lines);
    free(content);
    return 0;
  }

  // Reset the state of the game
  pg_statsClose();
  pg_clearGosubStack();
  pg_clearParams();
  pg_clearTempVars();
  pg_clearChoices();
  pg_game_status.choiceContextsCount = 0;
  pg_game_status.used_choices_count = 0;
  pg_game_status.labelVisitsCount = 0;
  pg_game_status.gameEnded = 0;
  pg_game_status.showPageBreak = 0;
  pg_game_status.scrollY = 0;
  pg_game_status.selectedChoiceIndex = -1;
  free(pg_game_status.text);
  pg_game_status.text = NULL;
  if (text_surface) {
    SDL_FreeSurface(text_surface);
    text_surface = NULL;
  }
  for (int i = 0; i < runningGame->achievementsCount; i++)
    runningGame->achievements[i].achieved = 0;

  // The variables created after the save (e.g. by *check_purchase) are removed
  char *loadedVars = calloc(sizeof(runningGame->variables) / sizeof(variable), 1);

  long position = 0;
  for (int i = 1; i < linesCount - 1; i++) {
    char *line = lines[i];
    char *data = strchr(line, ' ');
    if (data == NULL)
      continue;
    *data++ = '\0';

    int type;
    char *name, *value;
    if (strcmp(line, "scene") == 0) {
      pg_openScene(data, 1);
    } else if (strcmp(line, "sceneindex") == 0) {
      pg_game_status.sceneIndex = atoi(data);
    } else if (strcmp(line, "position") == 0) {
      position = atol(data);
    } else if (strcmp(line, "reuse") == 0) {
      pg_game_status.choiceReuse = (enum choice_reuse)atoi(data);
    } else if (strcmp(line, "looplimit") == 0) {
      pg_game_status.loopLimit = atoi(data);
    } else if (strcmp(line, "ended") == 0) {
      pg_game_status.gameEnded = atoi(data);
    } else if (strcmp(line, "random") == 0) {
      pg_randomState = (uint64_t)strtoull(data, NULL, 10);
    } else if (strcmp(line, "var") == 0) {
      if (pg_parseSavedVariable(data, &type, &name, &value)) {
        variable *var = NULL;
        for (int j = 0; j < runningGame->variablesCount; j++) {
          if (stricmp(runningGame->variables[j].name, name) == 0) {
            var = &runningGame->variables[j];
            break;
          }
        }
        if (var == NULL) {
          addGameVar(runningGame, (enum var_types)type, name, value);
          var = getGameVar(name, runningGame);
        } else {
          free(var->value);
          var->value = pg_strndup(value, strlen(value));
          var->type = (enum var_types)type;
        }
        if (var != NULL && loadedVars != NULL)
          loadedVars[var - runningGame->variables] = 1;
      }
    } else if (strcmp(line, "temp") == 0) {
      if (pg_parseSavedVariable(data, &type, &name, &value)) {
        pg_setTempVar(name, value);
        pg_game_status.tempVariables[pg_game_status.tempVariablesCount - 1].type = (enum var_types)type;
      }
    } else if (strcmp(line, "param") == 0) {
      if (pg_game_status.paramsCount < (int)(sizeof(pg_game_status.params) / sizeof(char *))) {
        pg_unescape(data);
        pg_game_status.params[pg_game_status.paramsCount++] = pg_strndup(data, strlen(data));
      }
    } else if (strcmp(line, "used") == 0) {
      if (pg_game_status.used_choices_count < (int)(sizeof(pg_game_status.used_choices) / sizeof(pg_game_status.used_choices[0])))
        snprintf(pg_game_status.used_choices[pg_game_status.used_choices_count++], sizeof(pg_game_status.used_choices[0]), "%s", data);
    } else if (strcmp(line, "achieved") == 0) {
      for (int j = 0; j < runningGame->achievementsCount; j++) {
        if (stricmp(runningGame->achievements[j].name, data) == 0)
          runningGame->achievements[j].achieved = 1;
      }
    } else if (strcmp(line, "context") == 0) {
      if (pg_game_status.choiceContextsCount < (int)(sizeof(pg_game_status.choiceContexts) / sizeof(choice_context))) {
        choice_context *ctx = &pg_game_status.choiceContexts[pg_game_status.choiceContextsCount++];
        sscanf(data, "%li %i %i", &ctx->endPosition, &ctx->indentation, &ctx->gosubCount);
      }
    } else if (strcmp(line, "frame") == 0) {
      int index, isScene, sceneIndex, reuse, loopLimit;
      long framePosition;
      int nameStart = 0;
      sscanf(data, "%i %i %li %i %i %i %n", &index, &isScene, &framePosition, &sceneIndex, &reuse, &loopLimit, &nameStart);
      gosub_info *frame = &pg_game_status.gosub[index];
      memset(frame, 0, sizeof(gosub_info));
      frame->position = framePosition;
      frame->sceneIndex = sceneIndex;
      frame->choiceReuse = (enum choice_reuse)reuse;
      frame->loopLimit = loopLimit;
      if (isScene) {
        pg_copyString(frame->sceneName, sizeof(frame->sceneName), data + nameStart);
        char scenePath[600];
        snprintf(scenePath, sizeof(scenePath), "%s/scenes/%s.txt", runningGame->path, frame->sceneName);
        frame->fp = fopen(scenePath, "rb");
        if (frame->fp != NULL) {
          fseek(frame->fp, 0, SEEK_END);
          frame->fpSize = ftell(frame->fp);
          fseek(frame->fp, frame->position, SEEK_SET);
        }
      }
      if (index > pg_game_status.gosubCount)
        pg_game_status.gosubCount = index;
    } else if (strcmp(line, "frametemp") == 0) {
      char *space = strchr(data, ' ');
      if (space == NULL)
        continue;
      *space = '\0';
      int index = atoi(data);
      if (index < 1 || index > pg_game_status.gosubCount)
        continue;
      if (pg_parseSavedVariable(space + 1, &type, &name, &value)) {
        gosub_info *frame = &pg_game_status.gosub[index];
        variable *tmp = realloc(frame->tempVariables, sizeof(variable) * (frame->tempVariablesCount + 1));
        if (tmp == NULL)
          continue;
        frame->tempVariables = tmp;
        variable *var = &frame->tempVariables[frame->tempVariablesCount++];
        var->name = pg_strndup(name, strlen(name));
        var->value = pg_strndup(value, strlen(value));
        var->type = (enum var_types)type;
      }
    }
  }
  free(lines);
  free(content);

  if (loadedVars != NULL) {
    int count = 0;
    for (int i = 0; i < runningGame->variablesCount; i++) {
      if (loadedVars[i]) {
        runningGame->variables[count++] = runningGame->variables[i];
      } else {
        logDebug("Variable %s removed (not in the save)", runningGame->variables[i].name);
        free(runningGame->variables[i].name);
        free(runningGame->variables[i].value);
      }
    }
    runningGame->variablesCount = count;
    free(loadedVars);
  }

  // Execute the saved page again
  fseek(pg_game_status.fp, position, SEEK_SET);
  pg_game_status.fpPosition = position;
  logInfo("Game loaded from %s", path);
  pg_exec();
  return 1;
}

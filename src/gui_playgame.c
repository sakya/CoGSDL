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
variable* pg_getVar(const char *varName);
int pg_setVarValue(const char *varName, const char *value);
void pg_clearTempVars();
void pg_clearChoices();
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
/// @return The new file position where continue execution
long pg_manageIfCommand(const char *text, int indentation);
/// @brief Evaluate an if condition. The condition does not contains parentheses or AND/OR
/// @param condition The condition
/// @return 1 if true, 0 if false
int pg_evaluateCondition(const char *condition);
int pg_evaluateExpression(const char *expression, char *value);
/// @brief Manage the goto command
/// @param text The text of the line with the goto command
/// @return The new file position
long pg_manageGotoCommand(const char *text);
long pg_manageGosubCommand(const char *text);
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

int pg_get_operator(const char *condition, char *operator);
void pg_clear_image_cache();
image_cache_item* pg_get_image_from_cache(const char *name);
image_cache_item* pg_add_image_to_cache(const char *name);

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

  fseek(pg_game_status.fp, 0L, SEEK_END);
  pg_game_status.fpSize = ftell(pg_game_status.fp);
  fseek(pg_game_status.fp, 0L, SEEK_SET);

  pg_clearChoices();
  pg_game_status.showPageBreak = 0;
  pg_game_status.tempVariables = NULL;
  pg_game_status.labels = runningGame->labels;
  pg_game_status.labelsCount = runningGame->labelsCount;
  pg_game_status.text = NULL;
  pg_game_status.sceneIndex = 0;
  pg_game_status.fpPosition = runningGame->startPosition;
  pg_game_status.scrollY = 0;
  pg_game_status.textFullyRendered = 0;
  pg_game_status.lastRenderedY = 0;
  pg_game_status.selectedChoiceIndex = -1;
  pg_game_status.gosubCount = 0;

  logDebug("Position set to %li", pg_game_status.fpPosition);
  fseek(pg_game_status.fp, pg_game_status.fpPosition, SEEK_SET);

  SDL_Surface *surface = TTF_RenderUTF8_Blended(_Globals.textFont, "H", _theme->textColor);
  line_height = surface->h;
  logDebug("Line height %li", line_height);
  SDL_FreeSurface(surface);

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
  if (text_surface) {
    SDL_FreeSurface(text_surface);
    text_surface = NULL;
  }

  pg_game_status.scrollY = 0;
  pg_game_status.textFullyRendered = 0;
  pg_game_status.lastRenderedY = 0;
  pg_game_status.gosubCount = 0;

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

void pg_expandVariables(char* text)
{
  if (!text)
    return;

  int idx = indexOf(text, '$', 0);
  while (idx >= 0) {
    idx = indexOf(text, '$', ++idx);
  }
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
int pg_exec_primitive(FILE *fp, long file_size, long *final_position)
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

  if (pg_game_status.fpPosition == file_size) {
    logDebug("Reached EOF (file size=%i), going to the next scene", file_size);
    pg_goToNextScene();

    return 0;
  }

  int textStarted = 0;
  while (fgets(line, sizeof(line), fp)) {
    int lineIndentation = isEmpty(line)
      ? indentation
      : getIndentation(line);
    if (indentation < 0)
      indentation = lineIndentation;

    if (lineIndentation != indentation) {
      // Indentation changed, we don't need to read further
      break;
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
      if (strcmp(command, "page_break") == 0) {
        pg_game_status.showPageBreak = 1;
        strcpy(pg_game_status.pageBreakMessage, "Next");
        pg_game_status.fpPosition = ftell(fp);
        break;
      }

      if (strcmp(command, "line_break") == 0) {
        pg_appendText("\n", 0);
        continue;
      }

      if (strcmp(command, "text_image") == 0 || strcmp(command, "image") == 0) {
        // Add the command to the text as it should be rendered with it
        pg_appendText(line, 1);
        continue;
      }

      if (strcmp(command, "finish") == 0) {
        pg_manageFinishCommand(line);
        pg_game_status.showPageBreak = 1;
        pg_game_status.fpPosition = file_size;
        break;
      }

      if (strcmp(command, "goto") == 0) {
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

      if (strcmp(command, "gosub") == 0) {
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

      if (strcmp(command, "return") == 0) {
        if (pg_game_status.gosubCount == 0) {
          logWarning("No previous position found");
          continue;
        }

        long newPosition = pg_game_status.gosub[pg_game_status.gosubCount].position;
        pg_game_status.fpPosition = newPosition;
        pg_game_status.gosubCount--;
        indentation = -1;
        logDebug("%s -> %i", line, newPosition);
        continue;
      }

      if (strcmp(command, "set") == 0) {
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

      if (strcmp(command, "if") == 0 || strcmp(command, "elseif") == 0) {
        // if
        long newPosition = pg_manageIfCommand(line, lineIndentation);
        logDebug("%s -> %i", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        fseek(pg_game_status.fp, pg_game_status.fpPosition, SEEK_SET);
        indentation = -1;
        continue;
      }

      if (strcmp(command, "temp") == 0) {
        // temp varName value
        pg_manageTempCommand(line);
        continue;
      }

      if (strcmp(command, "choice") == 0 || strcmp(command, "fake_choice") == 0) {
        // choice
        long newPosition = pg_manageChoiceCommand();
        logDebug("Choice -> %i", newPosition);
        pg_game_status.fpPosition = newPosition;
        break;
      }

      if (strcmp(command, "label") == 0) {
        // We can just skip labels
        continue;
      }

      logWarning("Unknown command: %s", command);
    } else {
      // Text
      if (lineLen) {
        if (pg_needsNewLine(line) && !endsWith("\n", pg_game_status.text))
          pg_appendText("\n", 0);
        pg_appendText(line, 0);
        textStarted = 1;
      } else if(textStarted) {
        // New paragraph
        pg_appendText("\n\n", 0);
      }
    }
  }
  if (ftell(fp) == file_size) {
    pg_game_status.fpPosition = file_size;
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

  if (pg_game_status.text == NULL && pg_game_status.fpPosition == file_size) {
    logDebug("Reached EOF (file size=%i), going to the next scene", file_size);
    pg_goToNextScene();

    return 0;
  } else {
    logDebug("Text to render:\n%s", pg_game_status.text);
  }

  return 1;
}

void pg_exec()
{
  long position;
  while (!pg_exec_primitive(pg_game_status.fp, pg_game_status.fpSize, &position));
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
  pg_game_status.labels = NULL;
  pg_game_status.labelsCount = 0;
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

  fseek(pg_game_status.fp, 0L, SEEK_END);
  pg_game_status.fpSize = ftell(pg_game_status.fp);
  fseek(pg_game_status.fp, 0L, SEEK_SET);

  // Get labels
  char line[4096];
  int lineStartPosition = 0;
  long nextLineStartPosition = 0;
  while (fgets(line, sizeof(line), pg_game_status.fp)) {
    lineStartPosition = nextLineStartPosition;
    nextLineStartPosition = ftell(pg_game_status.fp);

    line[strcspn(line, "\r")] = '\0';
    line[strcspn(line, "\n")] = '\0';
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
      if (strcmp(alignment, "center") == 0) {
        destRect.x += deltaX / 2;
      } else if (strcmp(alignment, "right") == 0) {
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

long pg_manageIfCommand(const char *text, int indentation)
{
  char condition[4096];
  trim(condition);
  int textLen = strlen(text);
  int start = 0;
  if (startsWith("*if", text)) {
    start = 3;
  } else if (startsWith("*elseif", text)) {
    start = 7;
  }
  subString(text, condition, start, textLen - start);
  trim(condition);
  textLen = strlen(text);

  logDebug("if condition=%s", condition);

  // First check parentheses
  parentheses par[20];
  int parIdx = 0;
  int idx = 0;
  while (idx < textLen) {
    if (condition[idx] == '(') {
      par[parIdx++].open = idx;
    } else if (condition[idx] == ')') {
      for (int pIdx = parIdx - 1; pIdx >= 0; pIdx--) {
        if (par[pIdx].close == 0) {
          par[pIdx].close = idx;
          break;
        }
      }
    }
    idx++;
  }

  if (parIdx == 0) {
    // Single condition
    if (pg_evaluateCondition(condition)) {
      return ftell(pg_game_status.fp);
    } else {
      // Search for the else or end
      char line[4096];
      int lineStartPosition = 0;
      long nextLineStartPosition = 0;

      while (fgets(line, sizeof(line), pg_game_status.fp)) {
        lineStartPosition = nextLineStartPosition;
        nextLineStartPosition = ftell(pg_game_status.fp);
        line[strcspn(line, "\r")] = '\0';
        line[strcspn(line, "\n")] = '\0';

        int lineIndentation = getIndentation(line);
        trim(line);
        if (lineIndentation == indentation) {
          if (strcmp(line, "*else") == 0) {
            return ftell(pg_game_status.fp);
          } else if (startsWith("*elseif", line)) {
            return lineStartPosition;
          }

          return lineStartPosition;
        }
      }
    }
  }

  logWarning("If not managed: %s", text);
  return 0;
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

int pg_evaluateCondition(const char *condition)
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
    res = var->value != NULL && strcmp(var->value, "true") == 0;
    logDebug("\tCondition (%s) -> %s", condition, res ? "true" : "false");
    return res;
  }

  // expr OPT expr
  char left_expression[1024];
  subString(condition, left_expression, 0, opIdx);
  trim(left_expression);
  char right_expression[1024];
  subString(condition, right_expression, opIdx + 1, strlen(condition) - opIdx - 1);
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
  if (strcmp(operator, "=") == 0) {
    res = strcmp(left_value, right_value) == 0 ? 1 : 0;
  } else if (strcmp(operator, "!=") == 0) {
    res = strcmp(left_value, right_value) != 0 ? 1 : 0;
  } else if (strcmp(operator, ">") == 0) {
    res = strcmp(left_value, right_value) > 0 ? 1 : 0;
  } else if (strcmp(operator, ">=") == 0) {
    res = strcmp(left_value, right_value) >= 0 ? 1 : 0;
  } else if (strcmp(operator, "<") == 0) {
    res = strcmp(left_value, right_value) < 0 ? 1 : 0;
  } else if (strcmp(operator, "<=") == 0) {
    res = strcmp(left_value, right_value) <= 0 ? 1 : 0;
  } else {
    logWarning("\tUnknown operator: %s", operator);
    return 0;
  }

  logDebug("\tCondition (%s) -> %s", condition, res ? "true" : "false");
  return res;
}

int pg_evaluateExpression(const char *expression, char *value)
{
  value[0] = '\0';
  if (!expression)
    return 0;

  int expLen = strlen(expression);
  if (expLen > 1 && expression[0] == '"' && expression[expLen - 1] == '"') {
    // String constant
    subString(expression, value, 1, expLen - 2);

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
  if (strcmp(expression, "true") == 0 || strcmp(expression, "false") == 0) {
    strcpy(value, expression);
    return 1;
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

  char labelName[256];
  subString(text, labelName, start + 1, strlen(text) - start - 1);

  label *lbl = pg_get_label(labelName);
  if (lbl == NULL) {
    logWarning("Label %s not found", labelName);
    return pg_game_status.fpPosition;
  }

  // TODO: manage arguments
  pg_game_status.gosubCount++;
  gosub_info *gosub = &pg_game_status.gosub[pg_game_status.gosubCount];
  gosub->fp = NULL;
  gosub->position = pg_game_status.fpPosition;
  fseek(pg_game_status.fp, lbl->position, SEEK_SET);

  return lbl->position;
}

void pg_manageTempCommand(const char *text)
{
  int idx = indexOfWordBreak(text, 6);

  pg_game_status.tempVariablesCount++;
  if (pg_game_status.tempVariables == NULL) {
    pg_game_status.tempVariables = malloc(sizeof(variable) * pg_game_status.tempVariablesCount);
  } else {
    variable *tmp = realloc(pg_game_status.tempVariables, sizeof(variable) * pg_game_status.tempVariablesCount);
    if (tmp != NULL) {
      pg_game_status.tempVariables = tmp;
      tmp = NULL;
    } else {
      logWarning("Realloc failed");
      return;
    }
  }

  variable *var = &pg_game_status.tempVariables[pg_game_status.tempVariablesCount - 1];
  var->name = malloc(idx - 6 + 1);
  subString(text, var->name, 6, idx - 6);
  var->value = NULL;

  char *tempValue = malloc(strlen(text) - idx + 1);
  subString(text, tempValue, idx + 1, strlen(text) - idx);
  trim(tempValue);
  int valueLen = strlen(tempValue);
  if (valueLen > 1 && tempValue[0] == '"' && tempValue[valueLen - 1] == '"') {
    var->type = string;
  } else if (strcmp(tempValue, "true") == 0 || strcmp(tempValue, "false") == 0) {
    var->type = boolean;
  } else {
    var->type = number;
  }
  setVarValue(var, tempValue);
  free(tempValue);

  logDebug("\tTEMPVAR %s=%s (type %s)", var->name, var->value, var->type == string ? "string" : var->type == boolean ? "boolean" : "number");
}

long pg_manageChoiceCommand()
{
  char line[4096];
  int choiceIndentation = -1;
  int lineStartPosition = 0;
  long nextLineStartPosition = 0;
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
    if (lineIndentation == choiceIndentation) {
      int start = indexOf(line, '#', 0);
      if (start >= 0) {
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
            continue;
          }
        }

        choice *choice = &pg_game_status.choices[pg_game_status.choicesCount - 1];

        start++;
        choice->used = 0;
        choice->reusable = startsWithWord("*hide_reuse", line) ? 0 : 1;
        choice->text = malloc(strlen(line) - start + 1);
        // Get the position of the line to execute when the choice is selected
        long pos = ftell(pg_game_status.fp);
        char temp_line[4096];
        while (fgets(temp_line, sizeof(temp_line), pg_game_status.fp)) {
          int tempIndentation = getIndentation(temp_line);
          logDebug("temp_line=%s, tempIndentation=%i", temp_line, tempIndentation);
          if (tempIndentation > choiceIndentation) {
            choice->position = pos;
            break;
          }
        }
        choice->position = ftell(pg_game_status.fp);
        subString(line, choice->text, start, strlen(line) - start);
        logDebug("\tChoice: %s (pos=%li, reusable=%i)", choice->text, choice->position, choice->reusable);
      }
    }
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
    if (strcmp(image_cache[i].name, name) == 0)
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
  int i = 0;
  while (i < pg_game_status.labelsCount) {
    if (strcmp(pg_game_status.labels[i].name, labelName) == 0)
      return &pg_game_status.labels[i];
    i++;
  }
  return NULL;
}

variable* pg_getVar(const char *varName)
{
  int i = 0;
  for (i = 0; i < pg_game_status.tempVariablesCount; i++) {
    if (strcmp(pg_game_status.tempVariables[i].name, varName) == 0) {
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
  if (!textLen || *startPosition == textLen - 1)
    return NULL;

  char *res = NULL;
  char test[5];
  int width = 0;
  int endOfLinePosition = *startPosition;
  int endOfPreviousWordPosition = 0;
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
        continue;
      } else if (i + 3 < textLen && text[i + 1] == '/' && text[i + 2] == 'i' && text[i + 3] == ']') {
        font = _Globals.textFont;
        i += 4;
        continue;
      }

      // Bold
      if (i + 2 < textLen && text[i + 1] == 'b' && text[i + 2] == ']') {
        font = _Globals.textBoldFont;
        i += 3;
        continue;
      } else if (i + 3 < textLen && text[i + 1] == '/' && text[i + 2] == 'b' && text[i + 3] == ']') {
        font = _Globals.textFont;
        i += 4;
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
      endOfLinePosition = endOfPreviousWordPosition;
      if (wordWrap != NULL)
        *wordWrap = 1;
      if (last_x != NULL)
        *last_x = previousWordWidth;
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
  memset(res, 0, endOfLinePosition - *startPosition + 2);
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
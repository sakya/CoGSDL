#include "CoGSDL.h"
#include "gui_playgame.h"

void pg_init();
void pg_dispose();
void pg_exec();
void pg_renderText(const char* text, int x, int y, int maxWidth, int scrollY);
int pg_renderPageBreak();
int pg_indexOfWordEnd(const char *string, const int startFrom);
void pg_getCommand(const char *line, char *command) ;
void pg_goToNextScene();
void pg_appendText(const char *text, int newLine);
int pg_needsNewLine(const char* text);
label* pg_get_label(const char *labelName);
char* pg_getVarValue(const char *varName);
int pg_setVarValue(const char *varName, const char *value);
void pg_clearTempVars();
void pg_clearChoices();
/// @brief Get the next line from text
/// @param text The string
/// @param position The starting position
/// @param maxWidth The maximum width in pixels of the rendered line
/// @return A line of text or NULL. The position is updated with the position of the next line
char* pg_getNextLine(const char *text, int* position, int maxWidth);

/// @brief Manage the image and text_image command
/// @param text The text of the line with
/// @param y The y value where the image have to be drawn
/// @return The height of the image
int pg_manageImageCommand(const char *text, int y);
/// @brief Manage an if
/// @param text The text of the line with the if command
/// @return The new file position where continue execution
int pg_manageIfCommand(const char *text);
/// @brief Evaluate an if condition. The condition does not contains parentheses or AND/OR
/// @param condition The condition
/// @return 1 if true, 0 if false
int pg_evaluateCondition(const char *condition);
/// @brief Manage the goto command
/// @param text The text of the line with the goto command
/// @return The new file position
int pg_manageGotoCommand(const char *text);
/// @brief Manage the temp command
/// @param text The text of the line with the temp command
/// @return The new file position
void pg_manageTempCommand(const char *text);
/// @brief Manage the choice command
/// @param text The text of the line with the temp command
/// @param start
/// @return The new file position
int pg_manageChoiceCommand(const char *text);

void pg_clear_image_cache();
image_cache_item* pg_get_image_from_cache(const char *name);
image_cache_item* pg_add_image_to_cache(const char *name);

static game_status pg_game_status;
static int lineHeight = 0;
static image_cache_item image_cache[32];
int image_cache_count = 0;

void drawGame()
{
  if (pg_game_status.canScrollDown) {
    if (_Globals.input[_Keys.KeyDown] == 1) {
      _Globals.input[_Keys.KeyDown]= 0;
      pg_game_status.scrollY += lineHeight;
    }
  }

  if (_Globals.input[_Keys.KeyUp] == 1) {
    _Globals.input[_Keys.KeyUp]= 0;
    pg_game_status.scrollY -= lineHeight;
    if (pg_game_status.scrollY < 0)
      pg_game_status.scrollY = 0;
  }

  if (_Globals.input[_Keys.KeyA] == 1) {
    //Next
    if (!pg_game_status.canScrollDown && pg_game_status.showPageBreak) {
      _Globals.input[_Keys.KeyA] = 0;
      pg_exec();
    }
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

  if (pg_game_status.text != NULL) {
    pg_renderText(pg_game_status.text, TEXT_MARGIN, HEADER_HEIGHT + TEXT_MARGIN, _Globals.windowWidth - 2 * TEXT_MARGIN, pg_game_status.scrollY);
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
    for (int i = 0; i < pg_game_status.choicesCount; i++) {
      pg_game_status.lastRenderedY += lineHeight;
      //choice *choice = &pg_game_status.choices[i];
      //pg_renderText(choice->text, TEXT_MARGIN * 2, pg_game_status.lastRenderedY, _Globals.windowWidth - 4 * TEXT_MARGIN, pg_game_status.scrollY);
    }
  }
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}

void pg_init()
{
  pg_clear_image_cache();
  char filePath[256];
  strcpy(filePath, runningGame->path);
  strcat(filePath, "/scenes/startup.txt");

  pg_game_status.fp = fopen(filePath, "rb");
  if (!pg_game_status.fp) {
    logError("Failed to open file %s", filePath);
    return;
  }
  logDebug("Opened file %s\n", filePath);

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

  logDebug("Position set to %li\n", pg_game_status.fpPosition);
  fseek(pg_game_status.fp, pg_game_status.fpPosition, SEEK_SET);

  SDL_Surface *surface = TTF_RenderUTF8_Blended(_Globals.textFont, "H", _theme->textColor);
  lineHeight = surface->h;
  logDebug("Line height %li\n", lineHeight);
  SDL_FreeSurface(surface);
}

void pg_dispose()
{
  pg_clear_image_cache();
  pg_clearTempVars();
  pg_clearChoices();

  pg_game_status.scrollY = 0;
  pg_game_status.textFullyRendered = 0;
  pg_game_status.lastRenderedY = 0;

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

void pg_renderText(const char* text, int x, int y, int maxWidth, int scrollY)
{
  if (text == NULL || strlen(text) == 0)
    return;

  char test[5];
  TTF_Font *font = _Globals.textFont;

  pg_game_status.textFullyRendered = 1;
  pg_game_status.lastRenderedY = 0;

  char *line = NULL;
  int linePos = 0;
  while ((line = pg_getNextLine(pg_game_status.text, &linePos, maxWidth)) != NULL) {
    if (startsWithWord("*text_image", line) || startsWithWord("*image", line)) {
      // Draw image
      // text_image filename.png alignment optional text
      // image filename.png alignment optional text
      y += pg_manageImageCommand(line, y - scrollY);
      x = TEXT_MARGIN;
      free(line);
      continue;
    }

    if (y < scrollY + HEADER_HEIGHT + TEXT_MARGIN) {
      free(line);
      y += lineHeight;
      continue;
    }

    if (y - scrollY + lineHeight > _Globals.windowHeight - HEADER_HEIGHT) {
      pg_game_status.textFullyRendered = 0;
      free(line);
      break;
    }

    int lineLen = strlen(line);
    if (lineLen > 0) {
      int i = 0;
      do {
        // Check font change:
        if (line[i] == '[') {
          // Italic
          if (i + 2 < lineLen && line[i + 1] == 'i' && line[i + 2] == ']') {
            font = _Globals.textItalicFont;
            i += 2;
            continue;
          } else if (i + 3 < lineLen && line[i + 1] == '/' && line[i + 2] == 'i' && line[i + 3] == ']') {
            font = _Globals.textFont;
            i += 3;
            continue;
          }

          // Bold
          if (i + 2 < lineLen && line[i + 1] == 'b' && line[i + 2] == ']') {
            font = _Globals.textBoldFont;
            i += 2;
            continue;
          } else if (i + 3 < lineLen && line[i + 1] == '/' && line[i + 2] == 'b' && line[i + 3] == ']') {
            font = _Globals.textFont;
            i += 3;
            continue;
          }
        }

        int charBytes = u8seqlen(&line[i]);
        memset(&test, 0, sizeof(test));
        strncpy(test, &line[i], charBytes);
        int charWidth;
        TTF_MeasureUTF8(font, test, _Globals.windowWidth, &charWidth, NULL);
        sdlWriteText(font, _theme->textColor, test, x, y - scrollY, 0, 0);
        pg_game_status.lastRenderedY = y - scrollY + lineHeight;
        x += charWidth;
      } while (u8nextchar(line, &i));
    }

    free(line);
    y += lineHeight;
    x = TEXT_MARGIN;
    if (y - scrollY + lineHeight > _Globals.windowHeight - HEADER_HEIGHT) {
      pg_game_status.textFullyRendered = 0;
      break;
    }
  }
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

    char next[] = "Next";
    int width;
    TTF_MeasureUTF8(_Globals.defaultFont, next, _Globals.windowWidth, &width, NULL);
    sdlWriteText(_Globals.defaultFont, _theme->textColor, next, _Globals.windowWidth - 35 - width - 5, _Globals.windowHeight - 28, 0, 0);

    return 1;
  }

  return 0;
}

void pg_exec()
{
  char line[4096];
  int indentation = -1;

  if(pg_game_status.text != NULL) {
    free(pg_game_status.text);
    pg_game_status.text = NULL;
  }
  pg_game_status.showPageBreak = 0;
  pg_game_status.scrollY = 0;
  pg_clearChoices();

  if (pg_game_status.fpPosition == pg_game_status.fpSize) {
    logDebug("Reached EOF (file size=%i), going to the next scene\n", pg_game_status.fpSize);
    pg_goToNextScene();
  }

  int textStarted = 0;
  while (fgets(line, sizeof(line), pg_game_status.fp)) {
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
      logDebug("Command=%s\n", command);
      if (strcmp(command, "page_break") == 0) {
        pg_game_status.showPageBreak = 1;
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
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

      if (strcmp(command, "goto") == 0) {
        // goto
        int newPosition = pg_manageGotoCommand(line);
        logDebug("%s -> %i\n", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        indentation = -1;
        continue;
      }

      if (strcmp(command, "set") == 0) {
        // set varName value
        int idx = 5;
        int wordEnd = pg_indexOfWordEnd(line, idx);
        if (wordEnd < 0) {
          logWarning("Missing variable name\n");
          continue;
        }
        char varName[256];
        char value[256];
        subString(line, varName, idx, wordEnd - idx);
        idx += strlen(varName);
        subString(line, value, idx + 1, lineLen - idx);
        pg_setVarValue(varName, value);
        continue;
      }

      if (strcmp(command, "if") == 0) {
        // if
        int newPosition = pg_manageIfCommand(line);
        logDebug("%s -> %i\n", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        indentation = -1;
        continue;
      }

      if (strcmp(command, "temp") == 0) {
        // temp varName value
        pg_manageTempCommand(line);
        continue;
      }

      if (strcmp(command, "choice") == 0) {
        // choice
        int newPosition = pg_manageChoiceCommand(line);
        logDebug("Choice -> %i\n", newPosition);
        pg_game_status.fpPosition = newPosition;
        break;
      }

      if (strcmp(command, "label") == 0) {
        // We can just skip labels
        continue;
      }

      logWarning("Unknown command: %s\n", command);
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
  logDebug("Text to render:\n%s\n", pg_game_status.text);
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
      logWarning("Realloc failed\n");
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
  logDebug("Opened file %s\n", filePath);

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
          logWarning("Realloc failed\n");
          break;
        }
      }

      pg_game_status.labels[pg_game_status.labelsCount - 1].name = malloc(strlen(line) - 7 + 1);
      subString(line, pg_game_status.labels[pg_game_status.labelsCount - 1].name, 7, strlen(line) - 7);
      pg_game_status.labels[pg_game_status.labelsCount - 1].position = lineStartPosition;

      logDebug("\tLabel name=%s, position=%li\n", pg_game_status.labels[pg_game_status.labelsCount - 1].name, pg_game_status.labels[pg_game_status.labelsCount - 1].position);
    }
  }
  fseek(pg_game_status.fp, 0L, SEEK_SET);
}

int pg_manageImageCommand(const char *text, int y)
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
  if (image && image->texture) {
    SDL_Rect destRect;
    destRect.x = TEXT_MARGIN;
    destRect.y = y;
    destRect.w = image->w;
    destRect.h = image->h;

    float ratio = (float)destRect.w / (float)destRect.h;
    if (destRect.w > _Globals.windowWidth - 2 * TEXT_MARGIN) {
      destRect.w = _Globals.windowWidth - 2 * TEXT_MARGIN;
      destRect.h = destRect.w / ratio;
    }
    if (destRect.h > _Globals.windowHeight - 2 * HEADER_HEIGHT - 2 * TEXT_MARGIN) {
      destRect.h = _Globals.windowHeight - 2 * HEADER_HEIGHT - 2 * TEXT_MARGIN;
      destRect.w = destRect.h * ratio;
    }

    SDL_Rect srcRect;
    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.w = image->w;
    srcRect.h = image->h;

    // Always return multiple of lineHeight to not break text scrolling
    int res = destRect.h + TEXT_MARGIN;
    int diff = res % lineHeight;
    if (diff)
      res += lineHeight - diff;

    if (y < HEADER_HEIGHT - TEXT_MARGIN) {
      int deltaY = HEADER_HEIGHT - TEXT_MARGIN - y;
      if (deltaY >= destRect.h) {
        return res;
      }

      int srcDeltaY = srcRect.h * deltaY / destRect.h;
      srcRect.y += srcDeltaY;
      srcRect.h -= srcDeltaY;

      destRect.y = HEADER_HEIGHT + TEXT_MARGIN;
      destRect.h -= deltaY;
    }

    if (y + destRect.h > _Globals.windowHeight - HEADER_HEIGHT - TEXT_MARGIN) {
      int deltaY = y + destRect.h - (_Globals.windowHeight - HEADER_HEIGHT - TEXT_MARGIN);
      int srcDeltaY = srcRect.h * deltaY / destRect.h;
      srcRect.h -= srcDeltaY;
      destRect.h -= deltaY;
    }

    int deltaX = _Globals.windowWidth - destRect.w - 2 * TEXT_MARGIN;
    if (deltaX > 0) {
      if (strcmp(alignment, "center") == 0) {
        destRect.x += deltaX / 2;
      } else if (strcmp(alignment, "right") == 0) {
        destRect.x = deltaX;
      }
    }

    if (destRect.h > 0) {
      SDL_RenderCopyEx(_Globals.renderer, image->texture, &srcRect, &destRect, 0, NULL, SDL_FLIP_NONE);
    }
    return res;
  }
  return 0;
}

int pg_manageIfCommand(const char *text)
{
  char condition[4096];
  int textLen = strlen(text);
  subString(text, condition, 3, textLen - 3);
  trim(condition);

  logDebug("if condition=%s\n", condition);
  // Condition on boolean
  if (indexOf(condition, ' ', 0) < 0) {
    if (pg_evaluateCondition(condition)) {
      return ftell(pg_game_status.fp);
    }
  }

  // First check parentheses
  parentheses par[20];
  int parIdx = 0;
  int idx = 0;
  while (idx < textLen) {
    if (text[idx] == '(') {
      par[parIdx++].open = idx;
    } else if (text[idx] == ')') {
      for (int pIdx = parIdx - 1; pIdx >= 0; pIdx--) {
        if (par[pIdx].close == 0) {
          par[pIdx].close = idx;
          break;
        }
      }
    }
    idx++;
  }

  logWarning("If not managed: %s\n", text);
  return 0;
}

int pg_evaluateCondition(const char *condition)
{
  if (indexOf(condition, ' ', 0) < 0) {
    // just a variable name (boolean condition)
    char* value = getVarValue(condition, runningGame);
    return value != NULL && strcmp(value, "true") == 0;
  }

  int idx;
  if ((idx = indexOf(condition, '=', 0)) >= 0) {
    // var = value

  }
  return 0;
}

int pg_manageGotoCommand(const char *text)
{
  int start = indexOfWordBreak(text, 0);
  if (start < 0) {
    logWarning("goto without label name\n");
    return ftell(pg_game_status.fp);
  }

  char labelName[256];
  subString(text, labelName, start + 1, strlen(text) - start - 1);

  label *lbl = pg_get_label(labelName);
  if (lbl == NULL) {
    logWarning("Label %s not found\n", labelName);
    return ftell(pg_game_status.fp);
  }
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
      logWarning("Realloc failed\n");
      return;
    }

  }

  variable *var = &pg_game_status.tempVariables[pg_game_status.tempVariablesCount - 1];
  var->name = malloc(idx - 6 + 1);
  subString(text, var->name, 6, idx - 6);
  var->value = malloc(strlen(text) - idx + 1);
  subString(text, var->value, idx + 1, strlen(text) - idx);

  if (startsWith("\"", var->value)) {
    var->type = string;
  } else if (strcmp(var->value, "true") == 0 || strcmp(var->value, "false") == 0) {
    var->type = boolean;
  } else {
    var->type = number;
  }

  logDebug("\tTEMPVAR %s=%s (type %s)\n", var->name, var->value, var->type == string ? "string" : var->type == boolean ? "boolean" : "number");
}

int pg_manageChoiceCommand(const char *text)
{
  char line[4096];
  int choiceIndentaion = -1;
  int lineStartPosition = 0;
  long nextLineStartPosition = 0;
  while (fgets(line, sizeof(line), pg_game_status.fp)) {
    lineStartPosition = nextLineStartPosition;
    nextLineStartPosition = ftell(pg_game_status.fp);
    line[strcspn(line, "\r")] = '\0';
    line[strcspn(line, "\n")] = '\0';

    if (choiceIndentaion < 0) {
      choiceIndentaion = getIndentation(line);
    }

    int lineIndentation = getIndentation(line);
    if (strlen(line) > 0 && lineIndentation < choiceIndentaion) {
      break;
    }

    if (lineIndentation == choiceIndentaion) {
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
            logWarning("Realloc failed\n");
            continue;
          }
        }

        choice *choice = &pg_game_status.choices[pg_game_status.choicesCount - 1];

        start++;
        choice->reusable = 1;
        choice->text = malloc(strlen(line) - start + 1);
        subString(line, choice->text, start, strlen(line) - start);
        logDebug("\tChoice: %s\n", choice->text);
      }
    }
  }
  return lineStartPosition;
}

void pg_clear_image_cache()
{
  for (int i = 0; i < image_cache_count; i++) {
    free(image_cache[i].name);
    image_cache[i].name = NULL;
    SDL_DestroyTexture(image_cache[i].texture);
    image_cache[i].texture = NULL;
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
  image_cache[image_cache_count].texture = SDL_CreateTextureFromSurface(_Globals.renderer, surface);
  SDL_FreeSurface(surface);
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

char* pg_getVarValue(const char *varName)
{
  int i = 0;
  for (i = 0; i < pg_game_status.tempVariablesCount; i++) {
    if (strcmp(pg_game_status.tempVariables[i].name, varName) == 0) {
      return pg_game_status.tempVariables[i].value;
    }
  }

  return getVarValue(varName, runningGame);
}

int pg_setVarValue(const char *varName, const char *value)
{
  int i = 0;

  for (i = 0; i < pg_game_status.tempVariablesCount; i++) {
    if (strcmp(pg_game_status.tempVariables[i].name, varName) == 0) {
      strcpy(pg_game_status.tempVariables[i].value, value);
      logDebug("Temp variable set %s=%s\n", varName, value);
      return 1;
    }
  }

  return setVarValue(varName, value, runningGame);
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

char* pg_getNextLine(const char *text, int* startPosition, int maxWidth)
{
  if (!text || maxWidth <= 0)
    return NULL;

  int textLen = strlen(text);
  if (*startPosition == textLen - 1)
    return NULL;

  char *res = NULL;
  char test[5];
  int width = 0;
  int endOfLinePosition = *startPosition;
  int endOfPreviousWordPosition = 0;
  int i = *startPosition;

  TTF_Font *font = _Globals.textFont;
  do {
    endOfLinePosition = i;
    if (text[i] == '\n' || i == textLen) {
      break;
    }

    // Check font change:
    if (text[i] == '[')  {
      // Italic
      if (i + 2 < textLen && text[i + 1] == 'i' && text[i + 2] == ']') {
        font = _Globals.textItalicFont;
        i += 2;
        continue;
      } else if (i + 3 < textLen && text[i + 1] == '/' && text[i + 2] == 'i' && text[i + 3] == ']') {
        font = _Globals.textFont;
        i += 3;
        continue;
      }

      // Bold
      if (i + 2 < textLen && text[i + 1] == 'b' && text[i + 2] == ']') {
        font = _Globals.textBoldFont;
        i += 2;
        continue;
      } else if (i + 3 < textLen && text[i + 1] == '/' && text[i + 2] == 'b' && text[i + 3] == ']') {
        font = _Globals.textFont;
        i += 3;
        continue;
      }
    }

    int charBytes = u8seqlen(&text[i]);
    memset(&test, 0, sizeof(test));
    strncpy(test, &text[i], charBytes);
    int charWidth;
    TTF_MeasureUTF8(font, test, _Globals.windowWidth, &charWidth, NULL);
    if (width + charWidth > maxWidth) {
      endOfLinePosition = endOfPreviousWordPosition;
      break;
    }
    width += charWidth;
    if (text[i] == ' ' || text[i] == '\t') {
      endOfPreviousWordPosition = i;
    }
  } while (u8nextchar(text, &i));

  res = malloc(endOfLinePosition - *startPosition + 1);
  int idx = 0;
  for (int j = *startPosition; j <= endOfLinePosition; j++) {
    if (text[j] != '\r' && text[j] != '\n' && (idx > 0 || text[j] != ' '))
      res[idx++] = text[j];
  }
  res[idx] = '\0';

  // Remove trailing spaces
  while (res[--idx] == ' ') {
    res[idx] = '\0';
  }

  if (text[endOfLinePosition] == '\n') {
    *startPosition = endOfLinePosition + 1;
  } else {
    *startPosition = endOfLinePosition;
  }
  return res;
}
#include "CoGSDL.h"
#include "playgame.h"

void pg_init();
void pg_dispose();
void pg_exec();
void pg_renderText(char* text);
int pg_renderPageBreak();
int pg_indexOfWordEnd(const char *string, const int startFrom);
void pg_getCommand(const char *line, char *command) ;
void pg_goToNextScene();
void pg_appendText(const char *text, int newLine);

static game_status pg_game_status;
static int lineHeight = 0;

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
    if (pg_game_status.status == play) {
      //Next
      if (!pg_game_status.canScrollDown && pg_game_status.showPageBreak) {
        _Globals.input[_Keys.KeyA] = 0;
        pg_exec();
      }
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
    pg_renderText(pg_game_status.text);
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
}

void pg_init()
{
  char filePath[255];
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

  pg_game_status.status = play;
  pg_game_status.text = NULL;
  pg_game_status.sceneIndex = 0;
  pg_game_status.fpPosition = runningGame->startPosition;
  pg_game_status.scrollY = 0;
  pg_game_status.textFullyRendered = 0;
  pg_game_status.lastRenderedY = 0;

  logDebug("Position set to %li\n", pg_game_status.fpPosition);
  fseek(pg_game_status.fp, pg_game_status.fpPosition, SEEK_SET);

  SDL_Surface* surface = TTF_RenderUTF8_Blended(_Globals.defaultFont, "H", colorText);
  lineHeight = surface->h;
  logDebug("Line height %li\n", lineHeight);
  SDL_FreeSurface(surface);
}

void pg_dispose()
{
  pg_game_status.scrollY = 0;
  pg_game_status.textFullyRendered = 0;
  pg_game_status.lastRenderedY = 0;

  free(pg_game_status.text);
  pg_game_status.text = NULL;

  fclose(pg_game_status.fp);
  pg_game_status.fp = NULL;

  freeGame(runningGame);
  runningGame = NULL;

  SDL_FreeSurface(runningGameIcon);
  runningGameIcon = NULL;
}

void pg_renderText(char* text)
{
  if (text == NULL || strlen(text) == 0)
    return;

  int end = strlen(text);
  int idx = 0;
  int x = TEXT_MARGIN;
  int y = HEADER_HEIGHT + TEXT_MARGIN;
  int wordWidth;
  char test[5];
  char word[256];
  TTF_Font *font = _Globals.defaultFont;
  pg_game_status.textFullyRendered = 0;
  pg_game_status.lastRenderedY = 0;
  while(1) {
    if (y - pg_game_status.scrollY + lineHeight > _Globals.windowHeight - HEADER_HEIGHT) {
      break;
    }

    // Find the next word to print
    int wordEnd = pg_indexOfWordEnd(text, idx);
    if (wordEnd < 0) {
      pg_game_status.textFullyRendered = 1;
      break;
    }

    if (wordEnd - idx + 1 > sizeof(word)) {
      logWarning("Word too long: %i chars", wordEnd - idx);
      break;
    }
    subString(text, word, idx, wordEnd - idx + 1);
    if (startsWithWord("*text_image", word) || startsWithWord("*image", word)) {
      // Draw image
      // *text_image filename.png alignment optional text
      // *image filename.png alignment optional text
    }

    TTF_MeasureUTF8(font, word, _Globals.windowWidth, &wordWidth, NULL);
    int i = 0;
    if (x + wordWidth + TEXT_MARGIN * 2 >= _Globals.windowWidth) {
      int needsNewline = 1;
      if (word[strlen(word) - 1] == ' ') {
        word[strlen(word) - 1] = '\0';
        TTF_MeasureUTF8(font, word, _Globals.windowWidth, &wordWidth, NULL);
        if (x + wordWidth + TEXT_MARGIN * 2 < _Globals.windowWidth) {
          needsNewline = 0;
        } else {
          word[strlen(word)] = ' ';
        }
      }

      if (needsNewline) {
        y += lineHeight;
        x = TEXT_MARGIN;
        if (y - pg_game_status.scrollY + lineHeight > _Globals.windowHeight - HEADER_HEIGHT) {
          break;
        }
        // Avoid whitespace at the beginning of the line
        if (word[i] == ' ')
          i++;
      }
    }

    int wordLen = strlen(word);
    do {
      if (word[i] == '\n') {
        y += lineHeight;
        x = TEXT_MARGIN;
        if (y - pg_game_status.scrollY + lineHeight > _Globals.windowHeight - HEADER_HEIGHT) {
          break;
        }
        continue;
      }

      // Check font change:
      if (word[i] == '[')  {
        // Italic
        if (i + 2 < wordLen && word[i + 1] == 'i' && word[i + 2] == ']') {
          font = _Globals.italicFont;
          i += 2;
          continue;
        } else if (i + 3 < wordLen && word[i + 1] == '/' && word[i + 2] == 'i' && word[i + 3] == ']') {
          font = _Globals.defaultFont;
          i += 3;
          continue;
        }

        // Bold
        if (i + 2 < wordLen && word[i + 1] == 'b' && word[i + 2] == ']') {
          font = _Globals.boldFont;
          i += 2;
          continue;
        } else if (i + 3 < wordLen && word[i + 1] == '/' && word[i + 2] == 'b' && word[i + 3] == ']') {
          font = _Globals.defaultFont;
          i += 3;
          continue;
        }
      }

      int charBytes = u8seqlen(&word[i]);
      memset(&test, 0, sizeof(test));
      strncpy(test, &word[i], charBytes);
      int charWidth;
      TTF_MeasureUTF8(font, test, _Globals.windowWidth, &charWidth, NULL);
      if (y >= pg_game_status.scrollY + HEADER_HEIGHT + TEXT_MARGIN) {
        sdlWriteText(font, colorText, test, x, y - pg_game_status.scrollY, 0, 0);
        pg_game_status.lastRenderedY = y - pg_game_status.scrollY + lineHeight;
      }
      x += charWidth;
    } while (u8nextchar(word, &i));

    idx += wordLen;
    if (idx >= end) {
      pg_game_status.textFullyRendered = 1;
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
    sdlWriteText(_Globals.defaultFont, colorText, next, _Globals.windowWidth - 35 - width - 5, _Globals.windowHeight - 30, 0, 0);

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

  if (pg_game_status.fpPosition == pg_game_status.fpSize) {
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

      if (strcmp(command, "set") == 0) {
        // *set varName value
        int idx = 5;
        int wordEnd = pg_indexOfWordEnd(line, idx);
        if (wordEnd < 0) {
          logWarning("Missing variable name\n");
          continue;
        }
        char varName[255];
        char value[255];
        subString(line, varName, idx, wordEnd - idx);
        idx += strlen(varName);
        subString(line, value, idx + 1, lineLen - idx);
        setVarValue(varName, value, runningGame);
        continue;
      }

      if (strcmp(command, "temp") == 0) {
        // *temp varName value
        continue;
      }

      if (strcmp(command, "choice") == 0) {
        // Choice
        continue;
      }
    } else {
      // Read text
      if (lineLen) {
        pg_appendText(line, 1);
        textStarted = 1;
      } else if(textStarted) {
        pg_appendText("\n", 0);
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

void pg_goToNextScene()
{
  if (pg_game_status.sceneIndex + 1 >= runningGame->scenesCount) {
    logWarning("No next scene");
    return;
  }

  pg_game_status.sceneIndex++;
  pg_game_status.fpPosition = 0;

  char filePath[255];
  strcpy(filePath, runningGame->path);
  strcat(filePath, "/scenes/");
  strcat(filePath, runningGame->scenes[pg_game_status.sceneIndex]);
  strcat(filePath, ".txt");

  if (pg_game_status.fp) {
    fclose(pg_game_status.fp);
    pg_game_status.fp = NULL;
  }
  pg_game_status.fp = fopen(filePath, "rb");
  if (!pg_game_status.fp) {
    logError("Failed to open file %s", filePath);
    return;
  }
  logDebug("Opened file %s\n", filePath);

  fseek(pg_game_status.fp, 0L, SEEK_END);
  pg_game_status.fpSize = ftell(pg_game_status.fp);
  fseek(pg_game_status.fp, 0L, SEEK_SET);
}
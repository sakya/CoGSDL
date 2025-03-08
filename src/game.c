#include "CoGSDL.h"

int compareByTitle(const void *a, const void *b)
{
  return stricmp(((struct game*)a)->title, ((struct game*)b)->title);
}

int isComment(const char *line)
{
  if (line == NULL)
    return 0;

  int end = strlen(line);
  char* comment = "*comment";
  int idx = 0;
  for (int i = 0; i < end; i++) {
    if (idx == 0 && isspace(line[i]))
      continue;

    if (line[i] != comment[idx++])
      return 0;

    if (idx == strlen(comment))
      break;
  }

  return idx == strlen(comment) ? 1 : 0;
}

int isCommand(const char *line, const char command)
{
  if (line == NULL)
    return 0;

  int end = strlen(line);
  const char *c = &command;
  int idx = 0;
  for (int i = 0; i < end; i++) {
    if (idx == 0 && isspace(line[i]))
      continue;

    if (line[i] != c[idx++])
      return 0;

    if (idx == strlen(c))
      break;
  }

  return idx == strlen(c) ? 1 : 0;
}

int loadGame(const char* path, game* game, int forListing)
{
  if (game == NULL)
    return -1;

  logInfo("Loading game from: %s", path);
  strcpy(game->path, path);

  char iconPath[256];
  strcpy(iconPath, path);
  strcat(iconPath, "/icon.png");
  if (fileExists(iconPath)) {
    strcpy(game->icon, iconPath);
  }

  strcpy(iconPath, path);
  strcat(iconPath, "/icon.jpg");
  if (fileExists(iconPath)) {
    strcpy(game->background, iconPath);
  }

  logInfo("\tIcon: %s", game->icon);
  logInfo("\tBackground: %s", game->background);
  char filePath[256];
  strcpy(filePath, path);
  strcat(filePath, "/scenes/startup.txt");
  FILE *fp;
  char line[4096];

  fp = fopen(filePath, "rb");
  if (fp == NULL) {
    logWarning("\tFile %s not found", filePath);
    return -1;
  }

  // Skip the UTF8 BOM
  char bomBuffer[3];
  if (fread(bomBuffer, sizeof(char), sizeof(bomBuffer), fp) == sizeof(bomBuffer)) {
    if (bomBuffer[0] != -17 || bomBuffer[1] != -69 || bomBuffer[2] != -65) {
      fseek(fp, 0, SEEK_SET);
    } else {
      logDebug("\tSkipped BOM bytes");
    }
  }

  achievement* ach = NULL;
  long lineStartPosition = 0;
  long nextLineStartPosition = ftell(fp);
  int sceneIdx = -1;
  game->title = NULL;
  game->author = NULL;
  game->ifid = NULL;
  game->scenesCount = 0;
  game->variablesCount = 0;
  game->achievementsCount = 0;
  game->startPosition = 0;
  game->hide_reuse = 0;
  while (fgets(line, sizeof(line), fp)) {
    lineStartPosition = nextLineStartPosition;
    nextLineStartPosition = ftell(fp);

    if (forListing && game->title != NULL && game->author != NULL) {
      break;
    }

    line[strcspn(line, "\r")] = '\0';
    line[strcspn(line, "\n")] = '\0';
    if (isComment(line)) {
      // Skip comments
      continue;
    }
    if (isEmpty(line)) {
      // Skip empty lines
      sceneIdx = -1;
      ach = NULL;
      continue;
    }

    if (stricmp(line, "*scene_list") == 0) {
      sceneIdx = 0;
    } else if (stricmp(line, "*hide_reuse") == 0) {
      game->hide_reuse = 1;
    } else if (startsWithWord("*title", line)) {
      game->title = malloc(strlen(line) - 7 + 1);
      subString(line, game->title, 7, strlen(line) - 7);
      logInfo("\tTitle: %s", game->title);
    } else if (startsWithWord("*author", line)) {
      game->author = malloc(strlen(line) - 8 + 1);
      subString(line, game->author, 8, strlen(line) - 8);
      logInfo("\tAuthor: %s", game->author);
    } else if (startsWithWord("*ifid", line)) {
      game->ifid = malloc(strlen(line) - 8 + 1);
      subString(line, game->ifid, 8, strlen(line) - 8);
      logInfo("\tIfid: %s", game->ifid);
    } else if (startsWithWord("*create", line)) {
      if (game->variablesCount == sizeof(game->variables) / sizeof(variable)) {
        logWarning("Reached maximum number of variables (%i)", game->variablesCount);
        continue;
      }

      // Variable definition
      int idx = indexOfWordBreak(line, 8);
      variable *var = &game->variables[game->variablesCount];
      var->name = malloc(idx - 8 + 1);
      subString(line, var->name, 8, idx - 8);
      var->value = NULL;

      char *tempValue = malloc(strlen(line) - idx + 1);
      subString(line, tempValue, idx + 1, strlen(line) - idx);
      trim(tempValue);
      var->type = get_var_type(tempValue);
      setVarValue(&game->variables[game->variablesCount], tempValue);
      free(tempValue);

      logDebug("\tVAR %s=%s (type %s)", var->name, var->value, var->type == string ? "string" : var->type == boolean ? "boolean" : "number");
      game->variablesCount++;
    } else if (startsWithWord("*achievement", line)) {
      if (game->achievementsCount == sizeof(game->achievements) / sizeof(achievement)) {
        logWarning("Reached maximum number of achievements (%i)", game->achievementsCount);
        continue;
      }

      // Achievement definition
      int idx = indexOfWordBreak(line, 13);
      game->achievements[game->achievementsCount].name = malloc(idx - 13 + 1);
      subString(line, game->achievements[game->achievementsCount].name, 13, idx - 13);

      game->achievements[game->achievementsCount].visible = line[idx + 1] == 'v' ? 1 : 0;

      idx = indexOfWordBreak(line, idx + 1);
      int nextIdx = indexOfWordBreak(line, idx + 1);
      char points[20] = "0";
      if (nextIdx - idx - 1 < sizeof(points)) {
        subString(line, points, idx + 1, nextIdx - idx - 1);
        game->achievements[game->achievementsCount].points = atoi(points);
      } else {
        logWarning("\tPoints string too long (idx=%i, nextIdx=%i)", idx, nextIdx);
      }
      game->achievements[game->achievementsCount].title = malloc(strlen(line) - nextIdx + 1);
      subString(line, game->achievements[game->achievementsCount].title, nextIdx + 1, strlen(line) - nextIdx);

      game->achievements[game->achievementsCount].preEarnedDescription = NULL;
      game->achievements[game->achievementsCount].postEarnedDescription = NULL;
      ach = &game->achievements[game->achievementsCount];
      logDebug("\tACH name=%s, title=%s, points=%i, visible=%i", game->achievements[game->achievementsCount].name, game->achievements[game->achievementsCount].title, game->achievements[game->achievementsCount].points, game->achievements[game->achievementsCount].visible);
      game->achievementsCount++;
    } else if (startsWithWord("*label", line)) {
      continue;
    } else if (startsWithWord("*product", line)) {
      // Don't know what's this
    } else {
      if (ach != NULL) {
        trim(line);
        if (ach->preEarnedDescription == NULL) {
          ach->preEarnedDescription = malloc(strlen(line) + 1);
          strncpy(ach->preEarnedDescription, line, strlen(line));
        } else if (ach->postEarnedDescription == NULL) {
          ach->postEarnedDescription = malloc(strlen(line) + 1);
          strncpy(ach->postEarnedDescription, line, strlen(line));
          ach = NULL;
        } else {
          ach = NULL;
        }
      } else if (sceneIdx != -1) {
        trim(line);
        if (startsWithWord("$", line)) {
          subString(line, game->scenes[sceneIdx], 2, strlen(line) - 2);
        } else {
          strcpy(game->scenes[sceneIdx], line);
        }
        logDebug("\tscene: %s", game->scenes[sceneIdx]);
        sceneIdx++;
        game->scenesCount++;
      } else {
        if (game->startPosition == 0) {
          logDebug("\tFirst line to interpret=%s", line);
          game->startPosition = lineStartPosition;
        }
      }
    }
  }
  fclose(fp);

  if (!forListing) {
    addGameVar(game, boolean, "choice_randomtest", "false");
    addGameVar(game, boolean, "choice_is_omnibus_app", "true");
    addGameVar(game, boolean, "skip_description", "true");

    logInfo("\tScenes      : %i", game->scenesCount);
    logInfo("\tVariables   : %i", game->variablesCount);
    logInfo("\tAchievements: %i", game->achievementsCount);
    logInfo("\tStart       : %i", game->startPosition);
  }

  return 0;
}

void freeGame(game *game)
{
  if (game == NULL)
    return;

  free(game->title);
  game->title = NULL;
  free(game->author);
  game->author = NULL;
  free(game->ifid);
  game->ifid = NULL;

  int i = 0;
  for (i = 0; i < game->variablesCount; i++) {
    free(game->variables[i].name);
    game->variables[i].name = NULL;

    free(game->variables[i].value);
    game->variables[i].value = NULL;
  }
  game->variablesCount = 0;

  for (i = 0; i < game->achievementsCount; i++) {
    free(game->achievements[i].name);
    game->achievements[i].name = NULL;

    free(game->achievements[i].title);
    game->achievements[i].title = NULL;

    free(game->achievements[i].preEarnedDescription);
    game->achievements[i].preEarnedDescription = NULL;

    free(game->achievements[i].postEarnedDescription);
    game->achievements[i].postEarnedDescription = NULL;
  }
  game->achievementsCount = 0;

  free(game);
}

int listGames(const char* path, game *games)
{
  struct dirent *entry;
  DIR *dir = opendir(path);
  if (dir == NULL) {
    logWarning("Directory %s not found", path);
    return 0;
  }

  int count = 0;
  while ((entry = readdir(dir)) != NULL) {
    if (stricmp(entry->d_name, ".") == 0 || stricmp(entry->d_name, "..") == 0)
      continue;

    char subDirPath[256];
    strcpy(subDirPath, path);
    strcat(subDirPath, entry->d_name);

    DIR *subDir = opendir(subDirPath);
    if (subDir == NULL)
      continue;
    closedir(subDir);

    game g;
    if (!loadGame(subDirPath, &g, 1)) {
      games[count] = g;
      count++;
    }
  }

  closedir(dir);

  qsort(games, count, sizeof(struct game), compareByTitle);
  logInfo("Games found in %s: %i", path, count);
  return count;
}

variable* getGameVarPrimitive(const char *varName, game* game)
{
  int i = 0;
  for (i = 0; i < game->variablesCount; i++) {
    if (stricmp(game->variables[i].name, varName) == 0)
      return &game->variables[i];
  }

  return NULL;
}

variable* getGameVar(const char *varName, game* game)
{
  variable* res = getGameVarPrimitive(varName, game);
  if (res == NULL)
    logWarning("Variable %s not found", varName);
  return res;
}

void addGameVar(game* game, enum var_types type, const char* name, const char* value)
{
  variable *var = getGameVarPrimitive(name, game);
  if (var == NULL) {
    var = &game->variables[game->variablesCount];
    var->type = type;
    var->name = malloc(strlen(name) + 1);
    strcpy(var->name, name);
    game->variablesCount++;
  } else {
    if (var->type != type) {
      logWarning("addGameVar different type: %i != %i", var->type, type);
    }
    if (var->value != NULL) {
      free(var->value);
      var->value = NULL;
    }
  }
  var->value = malloc(strlen(value) + 1);
  strcpy(var->value, value);
}

char* getGameVarValue(const char *varName, game* game)
{
  variable* var = getGameVar(varName, game);
  if (var == NULL) {
    return NULL;
  }
  return var->value;
}

int setGameVarValue(const char *varName, const char *value, game *game)
{
  variable* var = getGameVar(varName, game);
  if (var == NULL) {
    return 0;
  }

  setVarValue(var, value);
  return 1;
}

void setVarValue(variable *var, const char *value)
{
  if (var->value == NULL) {
    var->value = malloc(1);
    var->value[0] = '\0';
  }

  char *tempValue = malloc(strlen(value) + 1);
  strcpy(tempValue, value);
  trim(tempValue);
  int valueLen = strlen(tempValue);

  switch (var->type)
  {
    case string:
      if (valueLen > 1 && tempValue[0] == '"' && tempValue[valueLen - 1] == '"') {
        subString(tempValue, tempValue, 1, valueLen - 2);
      }
      break;
    case number:
      int intValue;
      int isFairMath = valueLen > 2 && tempValue[0] == '%' && (tempValue[1] == '+' || tempValue[1] == '-');
      if (isFairMath) {
        int varValue = atoi(var->value);
        int deltaValue = atoi(tempValue + 2);
        if (tempValue[1] == '+') {
          intValue = (varValue + (100 - varValue) * (deltaValue / 100));
          sprintf(tempValue, "%i", intValue);
        } else if (tempValue[1] == '-') {
          intValue = (varValue - varValue * (deltaValue / 100));
          sprintf(tempValue, "%i", intValue);
        }
      } else {
        if (!is_number(tempValue, &intValue)) {
          logWarning("Invalid value for number variable %s: %s", var->name, tempValue);
          strcpy(tempValue, "0");
        }

        if (tempValue[0] == '+' || tempValue[0] == '-') {
          intValue += atoi(var->value);
          sprintf(tempValue, "%i", intValue);
        }
      }
      break;
    case boolean:
      if (stricmp(tempValue, "true") != 0 && stricmp(tempValue, "false") != 0) {
        logWarning("Invalid value for boolean variable %s: %s", var->name, tempValue);
        strcpy(tempValue, "false");
      }
      break;
  }

  if (var->value != NULL) {
    free(var->value);
    var->value = NULL;
  }
  var->value = malloc(strlen(tempValue) + 1);
  strcpy(var->value, tempValue);
  logDebug("Variable set %s=%s (%s)", var->name, tempValue, var->type == string ? "string" : var->type == boolean ? "boolean" : "number");
  free(tempValue);
}

enum var_types get_var_type(const char *value)
{
  int valueLen = strlen(value);
  if (valueLen > 1 && value[0] == '"' && value[valueLen - 1] == '"') {
    return string;
  } else if (stricmp(value, "true") == 0 || stricmp(value, "false") == 0) {
    return boolean;
  }

  return number;
}
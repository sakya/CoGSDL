#include "CoGSDL.h"

int compareByTitle(const void *a, const void *b)
{
  return strcmp(((struct game*)a)->title, ((struct game*)b)->title);
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

  logInfo("Loading game from: %s\n", path);
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

  logInfo("\tIcon: %s\n", game->icon);
  logInfo("\tBackground: %s\n", game->background);
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
      logDebug("\tSkipped BOM bytes\n");
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
  game->labelsCount = 0;
  game->startPosition = 0;
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

    if (strcmp(line, "*scene_list") == 0) {
      sceneIdx = 0;
    } else if (startsWithWord("*title", line)) {
      game->title = malloc(strlen(line) - 7 + 1);
      subString(line, game->title, 7, strlen(line) - 7);
      logInfo("\tTitle: %s\n", game->title);
    } else if (startsWithWord("*author", line)) {
      game->author = malloc(strlen(line) - 8 + 1);
      subString(line, game->author, 8, strlen(line) - 8);
      logInfo("\tAuthor: %s\n", game->author);
    } else if (startsWithWord("*ifid", line)) {
      game->ifid = malloc(strlen(line) - 8 + 1);
      subString(line, game->ifid, 8, strlen(line) - 8);
      logInfo("\tIfid: %s\n", game->ifid);
    } else if (startsWithWord("*create", line)) {
      if (game->variablesCount == sizeof(game->variables) / sizeof(variable)) {
        logWarning("Reached maximum number of variables (%i)\n", game->variablesCount);
        continue;
      }

      // Variable definition
      int idx = indexOfWordBreak(line, 8);
      game->variables[game->variablesCount].name = malloc(idx - 8 + 1);
      subString(line, game->variables[game->variablesCount].name, 8, idx - 8);
      game->variables[game->variablesCount].value = malloc(strlen(line) - idx + 1);
      subString(line, game->variables[game->variablesCount].value, idx + 1, strlen(line) - idx);

      if (startsWith("\"", game->variables[game->variablesCount].value)) {
        game->variables[game->variablesCount].type = string;
      } else if (strcmp(game->variables[game->variablesCount].value, "true") == 0 || strcmp(game->variables[game->variablesCount].value, "false") == 0) {
        game->variables[game->variablesCount].type = boolean;
      } else {
        game->variables[game->variablesCount].type = number;
      }

      logDebug("\tVAR %s=%s (type %s)\n", game->variables[game->variablesCount].name, game->variables[game->variablesCount].value,
        game->variables[game->variablesCount].type == string ? "string" : game->variables[game->variablesCount].type == boolean ? "boolean" : "number");
      game->variablesCount++;
    } else if (startsWithWord("*achievement", line)) {
      if (game->achievementsCount == sizeof(game->achievements) / sizeof(achievement)) {
        logWarning("Reached maximum number of achievements (%i)\n", game->achievementsCount);
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
        logWarning("\tPoints string too long (idx=%i, nextIdx=%i)\n", idx, nextIdx);
      }
      game->achievements[game->achievementsCount].title = malloc(strlen(line) - nextIdx + 1);
      subString(line, game->achievements[game->achievementsCount].title, nextIdx + 1, strlen(line) - nextIdx);

      game->achievements[game->achievementsCount].preEarnedDescription = NULL;
      game->achievements[game->achievementsCount].postEarnedDescription = NULL;
      ach = &game->achievements[game->achievementsCount];
      logDebug("\tACH name=%s, title=%s, points=%i, visible=%i\n", game->achievements[game->achievementsCount].name, game->achievements[game->achievementsCount].title, game->achievements[game->achievementsCount].points, game->achievements[game->achievementsCount].visible);
      game->achievementsCount++;
    } else if (startsWithWord("*label", line)) {
      if (game->labelsCount == sizeof(game->labels) / sizeof(label)) {
        logWarning("Reached maximum number of labels (%i)\n", game->labelsCount);
        continue;
      }

      game->labels[game->labelsCount].name = malloc(strlen(line) - 7 + 1);
      subString(line, game->labels[game->labelsCount].name, 7, strlen(line) - 7);

      game->labels[game->labelsCount].position = lineStartPosition;
      logDebug("\tLabel name=%s, position=%li\n", game->labels[game->labelsCount].name, game->labels[game->labelsCount].position);
      game->labelsCount++;
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
        logDebug("\tscene: %s\n", game->scenes[sceneIdx]);
        sceneIdx++;
        game->scenesCount++;
      } else {
        if (game->startPosition == 0) {
          logDebug("\tFirst line to interpret=%s\n", line);
          game->startPosition = lineStartPosition;
        }
      }
    }
  }
  fclose(fp);

  if (!forListing) {
    // Add choice_is_omnibus_app var
    game->variables[game->variablesCount].name = malloc(64);
    strcpy(game->variables[game->variablesCount].name, "choice_is_omnibus_app");
    game->variables[game->variablesCount].value = malloc(64);
    strcpy(game->variables[game->variablesCount].value, "true");
    game->variablesCount++;
    // Add skip_description var
    game->variables[game->variablesCount].name = malloc(64);
    strcpy(game->variables[game->variablesCount].name, "skip_description");
    game->variables[game->variablesCount].value = malloc(64);
    strcpy(game->variables[game->variablesCount].value, "true");
    game->variablesCount++;

    logInfo("\tScenes      : %i\n", game->scenesCount);
    logInfo("\tVariables   : %i\n", game->variablesCount);
    logInfo("\tAchievements: %i\n", game->achievementsCount);
    logInfo("\tLabels      : %i\n", game->labelsCount);
    logInfo("\tStart       : %i\n", game->startPosition);
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

  for (i = 0; i < game->labelsCount; i++) {
    free(game->labels[i].name);
    game->labels[i].name = NULL;
  }

  free(game);
}

int loadScene(const char* path, scene* scene)
{
  if (scene == NULL)
    return -1;

  return 0;
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
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
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
  logInfo("Games found in %s: %i\n", path, count);
  return count;
}

char* getVarValue(const char *varName, game* game)
{
  int i = 0;
  for (i = 0; i < sizeof(game->variables) / sizeof(variable); i++) {
    if (strcmp(game->variables[i].name, varName) == 0)
      return game->variables[i].value;
  }
  return NULL;
}

int setVarValue(const char *varName, const char *value, game *game)
{
  int i = 0;
  for (i = 0; i < sizeof(game->variables) / sizeof(variable); i++) {
    if (strcmp(game->variables[i].name, varName) == 0) {
      strcpy(game->variables[i].value, value);
      logDebug("Variable %s=%s\n", varName, value);
      return 1;
    }
  }
  return 0;
}
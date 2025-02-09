#include "CoGSDL.h"

int loadGame(const char* path, game* game)
{
  if (game == NULL)
    return -1;

  logInfo("Loading game from: %s\n", path);
  strcpy(game->path, path);

  char iconPath[255];
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
  char filePath[255];
  strcpy(filePath, path);
  strcat(filePath, "/scenes/startup.txt");
  FILE * fp;
  char line[4096];

  fp = fopen(filePath, "r");
  if (fp == NULL) {
    logWarning("\tFile %s not found", filePath);
    return -1;
  }

  achievement* ach = NULL;

  int sceneIdx = -1;
  game->scenesCount = 0;
  game->variablesCount = 0;
  game->achievementsCount = 0;
  game->labelsCount = 0;
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\n")] = '\0';
    if (startsWith("*comment ", line)) {
      // Skip comments
      continue;
    }
    if (isEmpty(line)) {
      // Skip empty lines
      sceneIdx = -1;
      continue;
    }

    //logInfo("%s\n", line);
    if (strcmp(line, "*scene_list") == 0) {
      sceneIdx = 0;
    } else if (startsWith("*title ", line)) {
      game->title = malloc(strlen(line) - 7 + 1);
      subString(line, game->title, 7, strlen(line) - 7);
      logInfo("\tTitle: %s\n", game->title);
    } else if (startsWith("*author ", line)) {
      game->author = malloc(strlen(line) - 8 + 1);
      subString(line, game->author, 8, strlen(line) - 8);
      logInfo("\tAuthor: %s\n", game->author);
    } else if (startsWith("*create ", line)) {
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

      logDebug("\tVAR %s=%s\n", game->variables[game->variablesCount].name, game->variables[game->variablesCount].value);
      game->variablesCount++;
    } else if (startsWith("*achievement ", line)) {
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
      char points[20];
      if (nextIdx - idx - 1 < sizeof(points)) {
        subString(line, points, idx + 1, nextIdx - idx - 1);
        game->achievements[game->achievementsCount].points = atoi(points);
      }
      game->achievements[game->achievementsCount].title = malloc(strlen(line) - nextIdx + 1);
      subString(line, game->achievements[game->achievementsCount].title, nextIdx + 1, strlen(line) - nextIdx);

      ach = &game->achievements[game->achievementsCount];
      logDebug("\tACH name=%s, title=%s, points=%i, visible=%i\n", game->achievements[game->achievementsCount].name, game->achievements[game->achievementsCount].title, game->achievements[game->achievementsCount].points, game->achievements[game->achievementsCount].visible);
      game->achievementsCount++;
    } else if (startsWith("*label ", line)) {
      if (game->labelsCount == sizeof(game->labels) / sizeof(label)) {
        logWarning("Reached maximum number of labels (%i)\n", game->labelsCount);
        continue;
      }

      game->labels[game->labelsCount].name = malloc(strlen(line) - 7 + 1);
      subString(line, game->labels[game->labelsCount].name, 7, strlen(line) - 7);

      game->labels[game->labelsCount].position = ftell(fp);
      logDebug("\tLabel name=%s, position=%li\n", game->labels[game->labelsCount].name, game->labels[game->labelsCount].position);
      game->labelsCount++;
    } else {
      if (ach != NULL) {
        trim(line);
        if (ach->preEarnedDescription == NULL) {
          ach->preEarnedDescription = line;
        } else {
          ach->postEarnedDescription = line;
          ach = NULL;
        }
      } else if (sceneIdx != -1) {
        trim(line);
        if (startsWith("$ ", line)) {
          subString(line, game->scenes[sceneIdx], 2, strlen(line) - 2);
        } else {
          strcpy(game->scenes[sceneIdx], line);
        }
        logDebug("\tscene: %s\n", game->scenes[sceneIdx]);
        sceneIdx++;
        game->scenesCount++;
      }
    }
  }

  // Add choice_is_omnibus_app var
  game->variables[game->variablesCount].name = "choice_is_omnibus_app";
  game->variables[game->variablesCount].value = "true";
  game->variablesCount++;

  // Add skip_description var
  game->variables[game->variablesCount].name = "skip_description";
  game->variables[game->variablesCount].value = "true";
  game->variablesCount++;

  fclose(fp);

  logInfo("\tScenes      : %i\n", game->scenesCount);
  logInfo("\tVariables   : %i\n", game->variablesCount);
  logInfo("\tAchievements: %i\n", game->achievementsCount);
  logInfo("\tLabels      : %i\n", game->labelsCount);

  return 0;
}

int loadSection(const char* path, section* section)
{
  if (section == NULL)
    return -1;

  return 0;
}

int listGames(const char* path, game *games)
{
  struct dirent *entry;
  DIR *dir = opendir(path);
  if (dir == NULL)
      return 0;

  int count = 0;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char subDirPath[255];
    strcpy(subDirPath, path);
    strcat(subDirPath, entry->d_name);

    DIR *subDir = opendir(subDirPath);
    if (subDir == NULL)
        continue;
    closedir(subDir);

    game g;
    if (!loadGame(subDirPath, &g)) {
      games[count] = g;
      count++;
    }
  }

  closedir(dir);
  logInfo("Games found: %i", count);
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
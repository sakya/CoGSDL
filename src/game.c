#include "CoGSDL.h"

int loadGame(char* path, game* game)
{
  if (game == NULL)
    return -1;

  printf("Loading game from: %s\n", path);
  strcpy(game->path, path);

  char iconPath[255];
  strcpy(iconPath, path);
  strcat(iconPath, "/icon.png");
  if (fileExists(iconPath)) {
    strcpy(game->icon, iconPath);
  } else {
    strcpy(iconPath, path);
    strcat(iconPath, "/icon.jpg");
    if (fileExists(iconPath)) {
      strcpy(game->icon, iconPath);
    }
  }
  char filePath[255];
  strcpy(filePath, path);
  strcat(filePath, "/scenes/startup.txt");
  FILE * fp;
  char line[4096];

  fp = fopen(filePath, "r");
  if (fp == NULL)
      return -1;

  achievement* achievement = NULL;

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
    if (strlen(line) == 0) {
      // Skip empty lines
      sceneIdx = -1;
      continue;
    }

    if (strcmp(line, "*scene_list") == 0) {
      sceneIdx = 0;
    } else if (startsWith("*title ", line)) {
      game->title = (char*)malloc(strlen(line) - 7 + 1);
      subString(line, game->title, 7, strlen(line) - 7);
      printf("Title: %s\n", game->title);
    } else if (startsWith("*author ", line)) {
      game->author = malloc(strlen(line) - 8 + 1);
      subString(line, game->author, 8, strlen(line) - 8);
      printf("Author: %s\n", game->author);
    } else if (startsWith("*create ", line)) {
      // Variable definition
      int idx = indexOfWordBreak(line, 8);
      game->variables[game->variablesCount].name = (char*)malloc(idx - 8 + 1);
      subString(line, game->variables[game->variablesCount].name, 8, idx - 8);
      game->variables[game->variablesCount].value = (char*)malloc(strlen(line) - idx + 1);
      subString(line, game->variables[game->variablesCount].value, idx + 1, strlen(line) - idx);

      printf("VAR %s=%s\n", game->variables[game->variablesCount].name, game->variables[game->variablesCount].value);
      game->variablesCount++;
    } else if (startsWith("*achievement ", line)) {
      // Achievement definition
      int idx = indexOfWordBreak(line, 13);
      printf("achievementsCount=%i\n", game->achievementsCount);
      game->achievements[game->achievementsCount].name = (char*)malloc(idx - 13 + 1);
      subString(line, game->achievements[game->achievementsCount].name, 13, idx - 13);

      game->achievements[game->achievementsCount].visible = line[idx + 1] == 'v' ? 1 : 0;

      idx = indexOfWordBreak(line, idx + 1);
      int nextIdx = indexOfWordBreak(line, idx + 1);
      char points[10];
      subString(line, points, idx, nextIdx - idx);
      game->achievements[game->achievementsCount].points = atoi(points);

      game->achievements[game->achievementsCount].title = (char*)malloc(strlen(line) - nextIdx + 1);
      subString(line, game->achievements[game->achievementsCount].title, nextIdx + 1, strlen(line) - nextIdx);

      achievement = &game->achievements[game->achievementsCount];
      printf("ACH name=%s, title=%s, points=%i, visible=%i\n", game->achievements[game->achievementsCount].name, game->achievements[game->achievementsCount].title, game->achievements[game->achievementsCount].points, game->achievements[game->achievementsCount].visible);
      game->achievementsCount++;
    } else if (startsWith("*label ", line)) {
      game->labels[game->labelsCount].name = (char*)malloc(strlen(line) - 7);
      subString(line, game->labels[game->labelsCount].name, 7, strlen(line) - 7);

      game->labels[game->labelsCount].position = ftell(fp);
      printf("Label name=%s, position=%li\n", game->labels[game->labelsCount].name, game->labels[game->labelsCount].position);
      game->labelsCount++;
    } else {
      if (achievement != NULL) {
        trim(line);
        if (achievement->preEarnedDescription == NULL) {
          achievement->preEarnedDescription = line;
        } else {
          achievement->postEarnedDescription = line;
          achievement = NULL;
        }
      } else if (sceneIdx != -1) {
        trim(line);
        strcpy(game->scenes[sceneIdx], line);
        printf("scene: %s\n", game->scenes[sceneIdx]);
        sceneIdx++;
        game->scenesCount++;
      }
    }
  }

  // Add choice_is_omnibus_app var
  game->variables[game->variablesCount].name = "choice_is_omnibus_app";
  game->variables[game->variablesCount].value = "true";
  game->variablesCount++;

  fclose(fp);
  return 0;
}

int loadSection(char* path, section* section)
{
  if (section == NULL)
    return -1;

  return 0;
}

int listGames(char* path, game (*games)[0])
{
  struct dirent *entry;
  DIR *dir = opendir(path);
  if (dir == NULL)
      return 0;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    int count = 0;
    char subDirPath[255];
    strcpy(subDirPath, path);
    strcat(subDirPath, entry->d_name);

    DIR *subDir = opendir(subDirPath);
    if (subDir == NULL)
        continue;
    closedir(subDir);

    game game;
    if (loadGame(subDirPath, &game)) {
      games = realloc(games, count + 1);
      *games[count] = game;
      count++;
    }
  }

  closedir(dir);
  return 0;
}

char* getVarValue(char *varName, game* game)
{
  int i = 0;
  for (i = 0; i < sizeof(game->variables) / sizeof(variable); i++) {
    if (strcmp(game->variables[i].name, varName) == 0)
      return game->variables[i].value;
  }
  return NULL;
}
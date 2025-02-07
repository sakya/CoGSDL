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
  int varIdx = 0;
  int achIdx = 0;
  int lblIdx = 0;
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
      int idx = indexOf(line, ' ', 8);
      game->variables[varIdx].name = (char*)malloc(idx - 8 + 1);
      subString(line, game->variables[varIdx].name, 8, idx - 8);
      game->variables[varIdx].value = (char*)malloc(strlen(line) - idx + 1);
      subString(line, game->variables[varIdx].value, idx + 1, strlen(line) - idx);

      printf("VAR %s=%s\n", game->variables[varIdx].name, game->variables[varIdx].value);
      varIdx++;
    } else if (startsWith("*achievement ", line)) {
      // Achievement definition
      int idx = indexOf(line, ' ', 13);
      game->achievements[achIdx].name = (char*)malloc(idx - 13 + 1);
      subString(line, game->achievements[achIdx].name, 13, idx - 13);

      game->achievements[achIdx].visible = line[idx + 1] == 'v' ? 1 : 0;

      idx = indexOf(line, ' ', idx + 1);
      int nextIdx = indexOf(line, ' ', idx + 1);
      char points[10];
      subString(line, points, idx, nextIdx - idx);
      game->achievements[achIdx].points = atoi(points);

      game->achievements[achIdx].title = (char*)malloc(strlen(line) - nextIdx + 1);
      subString(line, game->achievements[achIdx].title, nextIdx + 1, strlen(line) - nextIdx);

      achievement = &game->achievements[achIdx];
      printf("ACH name=%s, title=%s, points=%i, visible=%i\n", game->achievements[achIdx].name, game->achievements[achIdx].title, game->achievements[achIdx].points, game->achievements[achIdx].visible);
      achIdx++;
    } else if (startsWith("*label ", line)) {
      game->labels[lblIdx].name = (char*)malloc(strlen(line) - 7);
      subString(line, game->labels[lblIdx].name, 7, strlen(line) - 7);

      game->labels[lblIdx].position = ftell(fp);
      printf("Label name=%s, position=%li\n", game->labels[lblIdx].name, game->labels[lblIdx].position);
      lblIdx++;
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
        strcpy(game->scene_list[sceneIdx], line);
        printf("scene: %s\n", game->scene_list[sceneIdx]);
        sceneIdx++;
      }
    }
  }

  fclose(fp);
  return 0;
}

int loadSection(char* path, section* section)
{
  if (section == NULL)
    return -1;

  return 0;
}
#include "CoGSDL.h"

int loadGame(char* path, Game* game)
{
  if (game == NULL)
    return -1;

  printf("Loading game from: %s\n", path);
  strcpy(game->path, path);
  char filePath[255];
  strcpy(filePath, path);
  strcat(filePath, "/scenes/startup.txt");
  FILE * fp;
  char line[4096];

  fp = fopen(filePath, "r");
  if (fp == NULL)
      return -1;

  achievement* achievement = NULL;
  int varIdx = 0;
  int achIdx = 0;
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\n")] = '\0';
    if (startsWith("*comment ", line) || strlen(line) == 0) {
      // Skip comments and empty lines
      continue;
    }

    if (startsWith("*title ", line)) {
      game->title = (char*)malloc(strlen(line) - 7 + 1);
      subString(line, game->title, 7, strlen(line) - 7);
      printf("\tTitle: %s\n", game->title);
    } else if (startsWith("*author ", line)) {
      game->author = malloc(strlen(line) - 8 + 1);
      subString(line, game->author, 8, strlen(line) - 8);
      printf("\tAuthor: %s\n", game->author);
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
    } else {
      if (achievement != NULL) {
        trim(line);
        if (achievement->preEarnedDescription == NULL) {
          achievement->preEarnedDescription = line;
        } else {
          achievement->postEarnedDescription = line;
          achievement = NULL;
        }
      }
    }
  }

  fclose(fp);
  return 0;
}
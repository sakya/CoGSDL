#include "CoGSDL.h"

int loadScene(char* path, Scene* scene)
{
  printf("Loading scene from: %s\n", path);
  FILE * fp;
  char line[4096];

  fp = fopen(path, "r");
  if (fp == NULL)
      return -1;

  while (fgets(line, sizeof(line), fp)) {
    if (startsWith("*comment ", line) || strlen(line) == 0) {
      // Skip comments
      continue;
    }

    if (startsWith("*title ", line)) {
      scene->title = malloc(strlen(line) - 7);
      subString(line, scene->title, 7, strlen(line) - 7);
      printf("\tTitle: %s", scene->title);
    } else if (startsWith("*author ", line)) {
      scene->author = malloc(strlen(line) - 8);
      subString(line, scene->author, 8, strlen(line) - 8);
      printf("\tAuthor: %s", scene->author);
    } else if (startsWith("*create ", line)) {
      // Variable definition
      int idx = indexOf(line, ' ');
      printf("idx=%i\n", idx);
    } else if (startsWith("*achievement ", line)) {
      // Achievement definition
    }
    //printf("%s", line);
  }

  fclose(fp);
  return 0;
}
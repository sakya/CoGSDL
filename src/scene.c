#include "CoGSDL.h"

int loadScene(char* path, Scene* scene)
{
  FILE * fp;
  char line[4096];

  fp = fopen(path, "r");
  if (fp == NULL)
      return -1;

  while (fgets(line, sizeof(line), fp)) {
      printf("%s", line);
  }

  fclose(fp);
  return 0;
}
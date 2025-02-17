#ifndef GAME_H__
#define GAME_H__

enum var_types
{
  string = 0,
  number = 1,
  boolean = 2,
};

struct variable
{
  char* name;
  char* value;
  enum var_types type;
} typedef variable;

struct label
{
  char* name;
  long position;
} typedef label;

struct achievement
{
  char* name;
  int visible;
  int points;
  char* title;
  char* preEarnedDescription;
  char* postEarnedDescription;
} typedef achievement;

struct game
{
  char path[256];
  char icon[256];
  char background[256];
  char *title;
  char *author;
  char *ifid;

  int scenesCount;
  char scenes[1024][256];

  int variablesCount;
  variable variables[4096];

  int achievementsCount;
  achievement achievements[4096];

  int labelsCount;
  label labels[4096];

  long startPosition;
} typedef game;

struct scene
{
  char path[256];

  label labels[4096];
} typedef scene;

#endif
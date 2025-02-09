#ifndef GAME_H__
#define GAME_H__

struct variable
{
  char* name;
  char* value;
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
  char path[255];
  char icon[255];
  char background[255];
  char* title;
  char* author;

  int scenesCount;
  char scenes[1024][256];

  int variablesCount;
  variable variables[4096];

  int achievementsCount;
  achievement achievements[4096];

  int labelsCount;
  label labels[4096];
} typedef game;

struct section
{
  char path[255];

  label labels[4096];
} typedef section;

#endif
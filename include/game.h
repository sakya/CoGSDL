struct variable
{
  char* name;
  char* value;

} typedef variable;

struct achievement
{
  char* name;
  int visible;
  int points;
  char* title;
  char* preEarnedDescription;
  char* postEarnedDescription;

} typedef achievement;

struct Game
{
  char path[255];
  char* title;
  char* author;

  char scene_list[1024][1024];
  variable variables[1024];
  achievement achievements[1024];
} typedef Game;

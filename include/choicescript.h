#ifndef CHOICESCRIPT_H
#define CHOICESCRIPT_H

// ChoiceScript interpreter: execution of the scenes, commands, variables, expressions

#include "CoGSDL.h"
#include "gui_playgame.h"

/// @brief A growing string
struct cs_strbuf
{
  char *buf;
  size_t len;
  size_t cap;
} typedef cs_strbuf;

void cs_clearParams();
void cs_exec();
int cs_indexOfWordEnd(const char *string, const int startFrom);
void cs_getCommand(const char *line, char *command) ;
/// @brief Go to the next scene of the scene list
/// @return 0 if there is no next scene (the game is ended)
int cs_goToNextScene();
void cs_clearGosubStack();
void cs_getCommandArgs(const char *line, char *args, int size);
long cs_manageGotoSceneCommand(const char *text);
void cs_manageParamsCommand(const char *text);
/// @brief Manage the rand command: *rand variable min max
void cs_manageRandCommand(const char *text);
/// @brief Manage the config command: *config variable value
void cs_manageConfigCommand(const char *text);
void cs_setTempVar(const char *name, const char *value);
/// @brief Initialize the random number generator (if needed)
void cs_randomSeed();
/// @brief Store the text typed for *input_text
void cs_setInputText(const char *text);
/// @brief Check the number typed for *input_number
const char *cs_validateInputNumber(const char *text);
void cs_copyString(char *dst, size_t size, const char *src);
void cs_appendText(const char *text, int newLine);
/// @brief Append a line that must be rendered on its own line (images, stat bars)
void cs_appendBlockLine(const char *line);
/// @brief Append the separator between two lines of the same paragraph (a space)
void cs_appendLineSeparator();
/// @brief Start a new paragraph (a single empty line)
void cs_appendParagraphBreak();
/// @brief Manage the stat_chart command (text, percent, opposed_pair rows)
void cs_manageStatChartCommand(int indentation);
int cs_openScene(const char *sceneName, int closeCurrent);
int cs_readLine(char *line, int size, long *lineStart);
int cs_needsNewLine(const char* text);
label* cs_get_label(const char *labelName);
/// @brief Get a variable (temp or global)
/// @param varName The variable name
/// @return The varible or NULL
variable* cs_getVar(const char *varName);
/// @brief Set a variable (temp or global) value
/// @param varName The variable name
/// @param value The value
/// @return 1 on success
int cs_setVarValue(const char *varName, const char *value);
variable* cs_add_temp_var();
int cs_delete_temp_var(int index);
void cs_clearTempVars();
void cs_clearChoices();
/// @brief Continue the execution with the body of a choice option
/// @param index The option index
void cs_selectChoice(int index);
int cs_isBlank(const char *line);
/// @brief Remove the choice contexts of the current gosub level (or deeper)
void cs_clearChoiceContexts(int gosubCount);
/// @brief Get the end of the choice of an option line (see choice_end)
/// @return The position after the choice or -1
long cs_findChoiceEnd(long position);
/// @brief Manage an if (and the following *elseif/*else)
/// @param text The text of the line with the if command
/// @param indentation The line indentation
/// @return The new file position where continue execution (the file is positioned there)
long cs_manageIfCommand(const char *text, int indentation);
/// @brief Skip the *elseif/*else branches following an executed branch
/// @param indentation The *elseif/*else indentation
/// @return The new file position where continue execution (the file is positioned there)
long cs_skipElseChain(int indentation);
/// @brief Evaluate an if condition. The condition does not contains parentheses or AND/OR
/// @param condition The condition
/// @return 1 if true, 0 if false
int cs_evaluateSimpleCondition(const char *condition);
int cs_evaluateCondition(const char *condition, int indentation);
int cs_evaluateExpression(const char *expression, char *value);
int cs_evaluateBoolean(const char *condition);
int cs_evaluateArithmetic(const char *expression, double *result);
/// @brief Replace ${}, $!{}, $!!{} and @{} in a text
/// @param text The text
/// @return The expanded text (must be freed) or NULL
char *cs_expandVariables(const char *text);
/// @brief Manage the goto command
/// @param text The text of the line with the goto command
/// @return The new file position
long cs_manageGotoCommand(const char *text);
long cs_manageGosubCommand(const char *text);
long cs_manageGosubSceneCommand(const char *text);
/// @brief Manage the temp command
/// @param text The text of the line with the temp command
/// @return The new file position
void cs_manageTempCommand(const char *text);
/// @brief Manage the choice command
/// @return The new file position
long cs_manageChoiceCommand();
/// @brief Manage the finish command
/// @param text
void cs_manageFinishCommand(const char *text);
void cs_manageCheckPurchaseCommand(const char *text);

int cs_get_operator(const char *condition, char *operator);

void cs_read_labels();
void cs_clear_labels();
extern uint64_t cs_randomState;
void cs_sb_append(cs_strbuf *sb, const char *s, size_t n);
char *cs_strndup(const char *s, size_t n);

#endif

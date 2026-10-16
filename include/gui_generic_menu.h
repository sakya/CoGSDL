#ifndef GUI_GENERIC_MENU_H
#define GUI_GENERIC_MENU_H

#include "CoGSDL.h"

/// @brief Kind of a menu item
typedef enum {
  /// @brief Simple item: text (and optional sub text)
  gme_item_simple = 0,
  /// @brief Item with a value chosen from a list of strings (left/right)
  gme_item_list,
  /// @brief Item with a numeric value between min and max (left/right)
  gme_item_number
} gme_item_type;

/// @brief A menu item
typedef struct {
  gme_item_type type;
  /// @brief Id for the caller (e.g. to use in a switch)
  int id;
  /// @brief Main text (defaultFont)
  const char *text;
  /// @brief Optional text below the main one (defaultSmallFont), NULL = none
  const char *subText;
  /// @brief 1: the item is shown with disabledTextColor and cannot be selected
  int disabled;
  /// @brief Extra space in pixels before the item (e.g. to separate "Quit")
  int spaceBefore;

  /// @brief gme_item_list: the values and the index of the current one
  const char *const *values;
  int valuesCount;
  int valueIdx;
  /// @brief gme_item_list: 1 = after the last value go back to the first one
  int wrapValues;

  /// @brief gme_item_number: current value, limits and step (0 = 1)
  int value;
  int minValue;
  int maxValue;
  int step;
} gme_item;

/// @brief Definition and state of a menu
typedef struct {
  gme_item *items;
  int itemsCount;

  /// @brief Top of the menu area
  int y;
  /// @brief Height of the menu area: the items that do not fit are scrolled
  int height;
  /// @brief Distance in pixels of the selection box from the left and right edges of the screen
  int margin;
  /// @brief Distance in pixels of the texts from the selection box
  int textIndent;
  /// @brief Vertical space in pixels between two items
  int spacing;
  /// @brief 1 = moving down from the last item goes to the first one (and up from the first to the last)
  int wrap;

  /// @brief State: selected item and first visible item
  int selectedIdx;
  int firstIdx;
} gme_menu;

/// @brief Result of gme_handleInput
typedef enum {
  gme_event_none = 0,
  /// @brief The selected item changed
  gme_event_moved,
  /// @brief The value of the selected item (list/number) changed
  gme_event_value_changed,
  /// @brief A pressed on the selected item
  gme_event_selected,
  /// @brief B pressed
  gme_event_back
} gme_event;

/// @brief Initialize a menu with the default layout (the whole area between header and footer)
void gme_init(gme_menu *menu, gme_item *items, int itemsCount);
/// @brief Handle the menu keys (up/down/left/right/A/B). The keys used are reset.
/// @return What happened: the selected item is menu->items[menu->selectedIdx]
gme_event gme_handleInput(gme_menu *menu);
/// @brief Draw the menu
void gme_draw(gme_menu *menu);
/// @brief The selected item (NULL if no item can be selected)
gme_item *gme_selectedItem(gme_menu *menu);
/// @brief Select an item (if it is enabled) and scroll to make it visible
void gme_select(gme_menu *menu, int idx);
/// @brief Height in pixels of an item (without spacing)
int gme_itemHeight(const gme_item *item);

#endif

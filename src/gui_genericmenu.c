#include "gui_generic_menu.h"

#define GME_DEFAULT_MARGIN 5
#define GME_DEFAULT_TEXT_INDENT 5
#define GME_DEFAULT_SPACING 2
/// @brief Space between the text and the border of the selection box
#define GME_BOX_PADDING 5

void gme_init(gme_menu *menu, gme_item *items, int itemsCount)
{
  memset(menu, 0, sizeof(*menu));
  menu->items = items;
  menu->itemsCount = itemsCount;
  menu->y = HEADER_HEIGHT + 10;
  menu->height = _Globals.windowHeight - 2 * HEADER_HEIGHT - 20;
  menu->margin = GME_DEFAULT_MARGIN;
  menu->textIndent = GME_DEFAULT_TEXT_INDENT;
  menu->spacing = GME_DEFAULT_SPACING;
  menu->selectedIdx = -1;
  gme_select(menu, 0);
}

static int gme_isSelectable(const gme_menu *menu, int idx)
{
  return idx >= 0 && idx < menu->itemsCount && !menu->items[idx].disabled;
}

int gme_itemHeight(const gme_item *item)
{
  if (item->surface != NULL)
    return item->surface->h;
  int h = _Globals.defaultFontHeight;
  if (item->subText != NULL && item->subText[0] != '\0')
    h += _Globals.defaultSmallFontHeight;
  return h + 2 * GME_BOX_PADDING;
}

/// @brief Space used by an item in the menu (spacing and spaceBefore included)
static int gme_itemSpace(const gme_menu *menu, int idx)
{
  int h = gme_itemHeight(&menu->items[idx]);
  if (idx > menu->firstIdx)
    h += menu->spacing + menu->items[idx].spaceBefore;
  return h;
}

/// @brief Index of the last item that fits in the menu area starting from firstIdx
static int gme_lastVisible(const gme_menu *menu)
{
  int used = 0;
  int last = menu->firstIdx;
  for (int i = menu->firstIdx; i < menu->itemsCount; i++) {
    used += gme_itemSpace(menu, i);
    if (used > menu->height && i > menu->firstIdx)
      break;
    last = i;
  }
  return last;
}

/// @brief Position in pixels of an item from the top of the menu (partialItems)
static int gme_itemTop(const gme_menu *menu, int idx)
{
  int y = 0;
  for (int i = 0; i < idx; i++)
    y += gme_itemHeight(&menu->items[i]) + menu->spacing + menu->items[i + 1].spaceBefore;
  return y;
}

/// @brief Height in pixels of all the items (partialItems)
static int gme_totalHeight(const gme_menu *menu)
{
  if (menu->itemsCount <= 0)
    return 0;
  return gme_itemTop(menu, menu->itemsCount - 1) + gme_itemHeight(&menu->items[menu->itemsCount - 1]);
}

/// @brief Scroll by pixels just enough to show the whole selected item (partialItems)
static void gme_ensureVisiblePartial(gme_menu *menu)
{
  if (menu->selectedIdx >= 0) {
    const int top = gme_itemTop(menu, menu->selectedIdx);
    const int bottom = top + gme_itemHeight(&menu->items[menu->selectedIdx]);
    if (top < menu->scrollY || bottom - top > menu->height)
      menu->scrollY = top;
    else if (bottom > menu->scrollY + menu->height)
      menu->scrollY = bottom - menu->height;
  }
  const int maxScroll = gme_totalHeight(menu) - menu->height;
  if (menu->scrollY > maxScroll)
    menu->scrollY = maxScroll;
  if (menu->scrollY < 0)
    menu->scrollY = 0;
}

/// @brief Scroll the menu so that the selected item is visible
static void gme_ensureVisible(gme_menu *menu)
{
  if (menu->partialItems) {
    gme_ensureVisiblePartial(menu);
    return;
  }
  if (menu->selectedIdx < 0)
    return;
  if (menu->firstIdx > menu->selectedIdx)
    menu->firstIdx = menu->selectedIdx;
  while (menu->firstIdx < menu->selectedIdx && gme_lastVisible(menu) < menu->selectedIdx)
    menu->firstIdx++;
  // Keep the disabled items just before the selected one visible when possible
  // (e.g. a disabled "Continue" at the top of the menu)
  if (menu->firstIdx == menu->selectedIdx) {
    while (menu->firstIdx > 0 && menu->items[menu->firstIdx - 1].disabled) {
      menu->firstIdx--;
      if (gme_lastVisible(menu) < menu->selectedIdx) {
        menu->firstIdx++;
        break;
      }
    }
  }
}

void gme_select(gme_menu *menu, int idx)
{
  if (menu->itemsCount <= 0) {
    menu->selectedIdx = -1;
    return;
  }
  if (idx < 0)
    idx = 0;
  if (idx >= menu->itemsCount)
    idx = menu->itemsCount - 1;
  // First enabled item from idx (forward, then backward)
  int found = -1;
  for (int i = idx; i < menu->itemsCount && found < 0; i++)
    if (gme_isSelectable(menu, i))
      found = i;
  for (int i = idx - 1; i >= 0 && found < 0; i--)
    if (gme_isSelectable(menu, i))
      found = i;
  menu->selectedIdx = found;
  gme_ensureVisible(menu);
}

gme_item *gme_selectedItem(gme_menu *menu)
{
  if (!gme_isSelectable(menu, menu->selectedIdx))
    return NULL;
  return &menu->items[menu->selectedIdx];
}

/// @brief Move the selection to the next enabled item in the direction delta (+1/-1)
static int gme_move(gme_menu *menu, int delta)
{
  int idx = menu->selectedIdx;
  for (int n = 0; n < menu->itemsCount; n++) {
    idx += delta;
    if (idx < 0 || idx >= menu->itemsCount) {
      if (!menu->wrap)
        return 0;
      idx = idx < 0 ? menu->itemsCount - 1 : 0;
    }
    if (gme_isSelectable(menu, idx)) {
      if (idx == menu->selectedIdx)
        return 0;
      menu->selectedIdx = idx;
      gme_ensureVisible(menu);
      return 1;
    }
  }
  return 0;
}

/// @brief Change the value of a list/number item
static int gme_changeValue(gme_item *item, int delta)
{
  if (item->type == gme_item_list) {
    if (item->valuesCount <= 0)
      return 0;
    int idx = item->valueIdx + delta;
    if (idx < 0 || idx >= item->valuesCount) {
      if (!item->wrapValues)
        return 0;
      idx = (idx + item->valuesCount) % item->valuesCount;
    }
    if (idx == item->valueIdx)
      return 0;
    item->valueIdx = idx;
    return 1;
  }

  if (item->type == gme_item_number) {
    int step = item->step > 0 ? item->step : 1;
    int value = item->value + delta * step;
    if (value < item->minValue)
      value = item->minValue;
    if (value > item->maxValue)
      value = item->maxValue;
    if (value == item->value)
      return 0;
    item->value = value;
    return 1;
  }
  return 0;
}

gme_event gme_handleInput(gme_menu *menu)
{
  if (menu->selectedIdx < 0 || !gme_isSelectable(menu, menu->selectedIdx))
    gme_select(menu, menu->selectedIdx < 0 ? 0 : menu->selectedIdx);

  if (_Globals.input[_Keys.KeyDown] == 1) {
    _Globals.input[_Keys.KeyDown] = 0;
    if (gme_move(menu, 1)) {
      playMenuSound();
      return gme_event_moved;
    }
  } else if (_Globals.input[_Keys.KeyUp] == 1) {
    _Globals.input[_Keys.KeyUp] = 0;
    if (gme_move(menu, -1)) {
      playMenuSound();
      return gme_event_moved;
    }
  } else if (_Globals.input[_Keys.KeyLeft] == 1 || _Globals.input[_Keys.KeyRight] == 1) {
    int delta = _Globals.input[_Keys.KeyRight] == 1 ? 1 : -1;
    _Globals.input[_Keys.KeyLeft] = 0;
    _Globals.input[_Keys.KeyRight] = 0;
    gme_item *item = gme_selectedItem(menu);
    if (item != NULL && gme_changeValue(item, delta)) {
      playMenuSound();
      return gme_event_value_changed;
    }
  } else if (_Globals.input[_Keys.KeyA] == 1) {
    _Globals.input[_Keys.KeyA] = 0;
    if (gme_selectedItem(menu) != NULL) {
      playMenuSelectSound();
      return gme_event_selected;
    }
  } else if (_Globals.input[_Keys.KeyB] == 1) {
    _Globals.input[_Keys.KeyB] = 0;
    return gme_event_back;
  }
  return gme_event_none;
}

/// @brief Text of the value of a list/number item, NULL for a simple item
static const char *gme_valueText(const gme_item *item, char *buffer, size_t size)
{
  if (item->type == gme_item_list) {
    if (item->valueIdx < 0 || item->valueIdx >= item->valuesCount)
      return NULL;
    snprintf(buffer, size, "< %s >", item->values[item->valueIdx]);
    return buffer;
  }
  if (item->type == gme_item_number) {
    snprintf(buffer, size, "< %d >", item->value);
    return buffer;
  }
  return NULL;
}

void gme_draw(gme_menu *menu)
{
  if (menu->itemsCount <= 0)
    return;
  gme_ensureVisible(menu);

  // partialItems: all the items, cut at the borders of the area; otherwise the whole visible ones
  const int first = menu->partialItems ? 0 : menu->firstIdx;
  const int last = menu->partialItems ? menu->itemsCount - 1 : gme_lastVisible(menu);
  const int mainHeight = _Globals.defaultFontHeight;
  const int textX = menu->margin + menu->textIndent;
  int y = menu->partialItems ? menu->y - menu->scrollY : menu->y;
  if (menu->partialItems) {
    SDL_Rect clip = { 0, menu->y, _Globals.windowWidth, menu->height };
    SDL_RenderSetClipRect(_Globals.renderer, &clip);
  }
  for (int i = first; i <= last; i++) {
    gme_item *item = &menu->items[i];
    if (i > first)
      y += menu->spacing + item->spaceBefore;
    const int height = gme_itemHeight(item);
    const int selected = i == menu->selectedIdx;
    if (menu->partialItems) {
      // Outside the area
      if (y >= menu->y + menu->height)
        break;
      if (y + height <= menu->y) {
        y += height;
        continue;
      }
    }

    if (selected) {
      SDL_SetRenderDrawColor(_Globals.renderer, _theme->headerBackgroundColor.r, _theme->headerBackgroundColor.g, _theme->headerBackgroundColor.b, 255);
      SDL_Rect rect = { menu->margin, y, _Globals.windowWidth - 2 * menu->margin, height };
      SDL_RenderFillRect(_Globals.renderer, &rect);
    }

    SDL_Color color = selected ? _theme->titleTextColor : _theme->textColor;
    if (item->disabled)
      color = _theme->disabledTextColor;

    if (item->surface != NULL) {
      SDL_Surface *surface = selected && item->selectedSurface != NULL ? item->selectedSurface : item->surface;
      SDL_Texture *texture = sdlGetImageTexture(surface, surface->w, surface->h);
      SDL_Rect rect = { textX, y, surface->w, surface->h };
      if (texture)
        SDL_RenderCopy(_Globals.renderer, texture, NULL, &rect);
      y += height;
      continue;
    }

    const int textY = y + GME_BOX_PADDING;
    sdlWriteText(_Globals.defaultFont, color, (char *)item->text, textX, textY, 0, 0);
    if (item->subText != NULL && item->subText[0] != '\0')
      sdlWriteText(_Globals.defaultSmallFont, color, (char *)item->subText, textX, textY + mainHeight, 0, 0);

    char buffer[128];
    const char *value = gme_valueText(item, buffer, sizeof(buffer));
    if (value != NULL) {
      int width = 0;
      TTF_SizeUTF8(_Globals.defaultFont, value, &width, NULL);
      sdlWriteText(_Globals.defaultFont, color, (char *)value, _Globals.windowWidth - menu->margin - menu->textIndent - width, textY, 0, 0);
    }

    y += height;
  }
  if (menu->partialItems)
    SDL_RenderSetClipRect(_Globals.renderer, NULL);
}

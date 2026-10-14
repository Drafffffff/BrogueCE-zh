/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef BROGUE_ZH_PANEL_H
#define BROGUE_ZH_PANEL_H
#include <SDL.h>
#include <stdint.h>
void br_inventory_restore(uint64_t category,uint64_t required,uint64_t forbidden,
                          int mode,const void *const *items,int count);
void br_inventory_remember(int index);
void br_inventory_forget(const void *item);
void br_inventory_clear(void);
void br_panel_begin(const char *title,const char *body);
void br_panel_add(const char *label,int hotkey,int enabled);
void br_panel_end(void);
int br_panel_selected(void);
void br_panel_focus(int index);
void br_panel_hint(const char *text);
void br_panel_tabs(const char *const *labels,int count,int selected);
void br_panel_object(const char *name,unsigned glyph,SDL_Color color);
void br_panel_prefer_details(int enabled);
int br_panel_overlay(void);
void br_panel_cover(void);
int br_panel_cover_active(void);
void br_panel_inventory(void);
int br_panel_inventory_active(void);
void br_panel_inventory_item(int index,const char *name,unsigned glyph,SDL_Color color,int equipped);
void br_panel_inventory_info(int index,int strength,const char *summary);
void br_panel_inventory_state(int index,int quantity,const char *state,SDL_Color ink);
int br_panel_take_detail(void);
void br_panel_scroll_bottom(void);
int br_panel_active(void);
/* May turn an explicit confirm into the existing button's text hotkey. */
int br_panel_event(SDL_Event *event);
void br_panel_draw(SDL_Renderer *renderer,int width,int height);
#endif

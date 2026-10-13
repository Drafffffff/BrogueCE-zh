/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef BROGUE_ZH_UI_H
#define BROGUE_ZH_UI_H
#include <SDL.h>
/* Panels borrow the game's own interface palette (source/src/brogue/Globals.c)
   so a handheld window reads as part of Brogue, not as a shell drawn on top.
   The panel floor is the upstream dialog background and the selected row is the
   upstream button colour, which keeps the original three-step brightness
   ladder: floor, selection, focus. */
#define BR_UI_INK ((SDL_Color){222,218,223,255})
#define BR_UI_FOCUS ((SDL_Color){225,205,135,255})
#define BR_UI_MUTED ((SDL_Color){176,165,190,255})
#define BR_UI_HINT ((SDL_Color){150,145,170,255})
#define BR_UI_DANGER ((SDL_Color){255,128,153,255})
#define BR_UI_BOX ((SDL_Color){7,6,15,255})        /* interfaceBoxColor */
#define BR_UI_SELECTED ((SDL_Color){18,15,38,255}) /* interfaceButtonColor */
#define BR_UI_RULE ((SDL_Color){42,36,54,255})
/* Health trough and the upstream red bar (colour {45,10,15}) at screen depth. */
#define BR_UI_TROUGH ((SDL_Color){21,16,28,255})
#define BR_UI_BAR_RED ((SDL_Color){115,26,38,255})
/* Nutrition uses the message palette, so the bar says what the text would:
   green while fed, item-yellow when hungry, danger once fainting. */
#define BR_UI_BAR_FED ((SDL_Color){94,128,102,255})
#define BR_UI_BAR_HUNGRY ((SDL_Color){186,163,86,255})
#define BR_UI_BAR_FAINT ((SDL_Color){196,98,118,255})
void br_ui_text(SDL_Renderer *renderer,const char *text,int x,int y,SDL_Color color,int columns);
void br_ui_fill(SDL_Renderer *renderer,SDL_Rect box,SDL_Color color);
/* One-pixel hairline between the panel and the map. */
void br_ui_rule(SDL_Renderer *renderer,int x,int y,int right);
/* Fixed ten-pixel character columns; Chinese occupies two columns. */
void br_ui_frame(SDL_Renderer *renderer,int width,int height,const char *title,int footer_rows);
#endif

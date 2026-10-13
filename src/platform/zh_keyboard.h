/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef BROGUE_ZH_KEYBOARD_H
#define BROGUE_ZH_KEYBOARD_H
#include <SDL.h>
void br_keyboard_begin(const char *prompt,int numeric);
void br_keyboard_value(const char *value,int bytes,int capacity,const char *suffix);
void br_keyboard_end(void);
int br_keyboard_active(void);
int br_keyboard_event(SDL_Event *event);
void br_keyboard_draw(SDL_Renderer *renderer,int width,int height);
#endif

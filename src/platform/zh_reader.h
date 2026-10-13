/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef BROGUE_ZH_READER_H
#define BROGUE_ZH_READER_H
#include <SDL.h>
int br_reader_history_begin(void);
void br_reader_history_add(unsigned long turn,const char *text);
void br_reader_history_end(void);
int br_reader_history_count(void);
void br_reader_set(const char *text);
void br_reader_open(const char *title, const char *text);
int br_reader_active(void);
int br_reader_event(const SDL_Event *event);
void br_reader_draw(SDL_Renderer *renderer, int width, int height);
#endif

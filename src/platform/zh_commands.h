/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef BROGUE_ZH_COMMANDS_H
#define BROGUE_ZH_COMMANDS_H
#include <SDL.h>
/* F6 / physical L2. context: 0 disabled, 1 exploration, 2 recording playback. */
int br_commands_event(SDL_Event *event,int context);
int br_commands_active(void);
void br_commands_state(int wizard,int easy,int rethrow,int swap,int graphics);
void br_commands_forget(void);
void br_commands_reload(void);
/* One-shot semantic command; never changes SDL's global modifier state. */
int br_commands_take(int *key,int *control,int *shift);
/* Preserve the originating tab across the original synchronous modal command.
   Resume only when the caller stayed in this game without taking a turn. */
int br_commands_begin(long key,int playback);
void br_commands_finish(int origin,int resume);
#endif

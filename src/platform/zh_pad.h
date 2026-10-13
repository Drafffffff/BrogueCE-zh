/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef BROGUE_ZH_PAD_H
#define BROGUE_ZH_PAD_H
#include <SDL.h>
typedef struct {
    unsigned held, direction, chord;
    Uint32 due;
    int pending, context, running;
    Uint32 repeat_due;
    int repeat_kind, menu_used;
} BrPad;
/* context 1 = exploration, 0 = UI, 2 = soft keyboard, 3 = original targeting, 4 = recording playback, 5 = map, 6 = reader, 7 = death, 8 = panel, 9 = inventory, 10 = action tabs; codes are H700 firmware codes. */
int br_pad_feed(BrPad *pad,int code,int value,Uint32 now,int context,SDL_Event *out);
int br_pad_tick(BrPad *pad,Uint32 now,int context,SDL_Event *out);
void br_pad_poll(int context);
void br_pad_close(void);
#endif

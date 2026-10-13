/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef BROGUE_ZH_MAP_H
#define BROGUE_ZH_MAP_H
#include <SDL.h>
#define BR_NEARBY_LINES 28
/* Only the labels, glyph and bars already painted by the native sidebar.
   No creature pointer or internal health value crosses this boundary. */
typedef struct {
    int x,y,atlas,player,line_count;
    SDL_Color ink;
    char name[1024],lines[BR_NEARBY_LINES][1024];
    SDL_Color tones[BR_NEARBY_LINES];
    int bars[BR_NEARBY_LINES]; /* -1 text, otherwise rounded visible percent */
} BrNearby;
void br_map_nearby_reset(void);
void br_map_nearby_add(const BrNearby *entry);
void br_game_nearby_add(const BrNearby *entry,int glyph);
int br_map_nearby_active(void);
void br_map_clear(int columns, int rows);
void br_map_record(int x, int y, int glyph, int fr, int fg, int fb, int br, int bg, int bb);
void br_map_put(int x, int y, int atlas, SDL_Color fore, SDL_Color back);
void br_map_player(int x, int y, int depth, int health, int maximum);
void br_map_exploring(int enabled);
void br_map_session(int active,int automated);
/* Input contexts may end between turns; they must not end the map session. */
void br_map_alert(const char *text, SDL_Color fore, SDL_Color back, int strength);
void br_map_acknowledging(int enabled);
int br_map_ack_active(void);
void br_map_playback(int enabled,int paused,int out_of_sync,unsigned long turn,unsigned long total);
int br_map_browsing(void);
void br_map_target(int enabled,int x,int y);
void br_map_inspect(int enabled,int x,int y);
void br_map_context_message(const char *text);
int br_map_aiming(void);
/* Core adapter copies the already rendered map and existing visible HUD. */
void br_game_target_view(int enabled,int x,int y);
void br_map_message(const char *text);
void br_map_searching(int enabled);
/* hunger: 0 fed, 1 hungry, 2 weak, 3 fainting. */
void br_map_hud(const char *metrics, const char *statuses, const char *details,
                int nutrition, int nutrition_max, int hunger);
/* Both bars share one column grid on the right — label, bar, then value — so
   row one (health) and row two (nutrition) read as a single instrument. The
   left of both rows is free for the depth, the derived stats and any active
   status effects. The HUD builder measures status text against this column so
   nothing runs under the bars. */
#define BR_HUD_LABEL_X 400
#define BR_HUD_BAR_X 448
#define BR_HUD_BAR_W 110
#define BR_HUD_VALUE_X 566
int br_map_showing(void);
int br_map_moving(void);
int br_map_event(const SDL_Event *event);
/* Returns logical map coordinates for a pointer in the passive local viewport. */
int br_map_pointer(int screen_x, int screen_y, int *map_x, int *map_y);
void br_map_draw(SDL_Renderer *renderer, int width, int height, SDL_Texture *atlas,
                 int atlas_width, int atlas_height);
#endif

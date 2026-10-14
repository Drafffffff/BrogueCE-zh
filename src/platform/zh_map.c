/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "zh_map.h"
#include "zh_glyph.h"
#include "zh_text.h"
#include "zh_reader.h"
#include "zh_ui.h"
#include <stdio.h>
#include <string.h>

/* Display-only cache: populated AFTER the game's own fog-aware painting.
   No pmap access, movement command, RNG or game cursor belongs in this module. */
typedef struct { int atlas; SDL_Color fore, back; } Cell;
static Cell cells[128][64];
static int columns, rows, px, py, depth, hp, max_hp;
static int nutrition, nutrition_max, hunger;
static struct {char name[160];int percent;SDL_Color ink;} effects[32];
static int effect_count;
void br_map_effects_reset(void) {effect_count=0;}
void br_map_effect(const char *name,int percent,SDL_Color ink) {
    if(effect_count>=32)return;
    snprintf(effects[effect_count].name,sizeof effects[effect_count].name,"%s",name);
    effects[effect_count].percent=percent;effects[effect_count++].ink=ink;
}
static int replay, replay_paused, replay_oos;
static unsigned long replay_turn,replay_total;
static int exploring, local = 1, mode, closing, cx, cy;
static int acknowledging,session,automated,searching;
static char alert[2048];
static SDL_Color alert_fore,alert_back;
static int alert_strength;
static int aiming,aim_overview,tx,ty;
static char message[20000],context_note[2048];
static char metrics[1024], statuses[1024], details[20000];
static SDL_Rect viewport;
static int origin_x, origin_y, tile_w, tile_h;
static int clamp(int value,int low,int high);
static void copy_text(char *to,size_t capacity,const char *from);
static BrNearby nearby[30];
static int nearby_count,nearby_open,nearby_selected,nearby_located;
static int nearby_page_size=3;
static int nearby_overlay=2; /* Start on the right; then off -> left -> right. */
void br_map_nearby_reset(void) {nearby_count=0;}
void br_map_nearby_add(const BrNearby *entry) {
    if(nearby_count<30)nearby[nearby_count++]=*entry;
}
int br_map_nearby_active(void) {return nearby_open;}
static void nearby_toggle(void) {
    nearby_open=!nearby_open;nearby_located=0;
    if(!nearby_open) {closing=1;return;}
    nearby_selected=0;
    if(aiming || mode) {
        int x=aiming?tx:cx,y=aiming?ty:cy;
        int found=0;
        for(int i=0;i<nearby_count;i++)if(nearby[i].x==x && nearby[i].y==y) {nearby_selected=i;found=1;break;}
        if(!found && nearby_count<30) {
            BrNearby e={0};e.x=x;e.y=y;e.atlas=cells[x][y].atlas;e.ink=cells[x][y].fore;
            br_game_map_caption(e.name,sizeof e.name,x,y);
            if(!e.name[0])copy_text(e.name,sizeof e.name,"光标位置");
            nearby_selected=nearby_count;nearby[nearby_count++]=e;
        }
    }
}
static int nearby_event(const SDL_Event *event) {
    if(event->type==SDL_KEYDOWN) {
        SDL_Keycode key=event->key.keysym.sym;
        if(key==SDLK_ESCAPE || key==SDLK_F5) {
            if(!event->key.repeat) {
                if(key==SDLK_ESCAPE && nearby_located)nearby_located=0;
                else {nearby_open=nearby_located=0;closing=1;}
            }
        } else if(key==SDLK_UP || key==SDLK_DOWN || key==SDLK_PAGEUP || key==SDLK_PAGEDOWN) {
            int step=key==SDLK_UP?-1:key==SDLK_DOWN?1:key==SDLK_PAGEUP?-nearby_page_size:nearby_page_size;
            nearby_selected=clamp(nearby_selected+step,0,nearby_count-1);nearby_located=0;
        } else if(key==SDLK_F2 || key==SDLK_HOME) {nearby_selected=0;nearby_located=0;}
        else if(key==SDLK_F8 && !event->key.repeat && nearby_count)nearby_located=1;
        else if(key==SDLK_RETURN && !event->key.repeat && nearby_count) {
            BrNearby *e=&nearby[nearby_selected];char full[32000];
            if(!e->player) {br_game_map_details(e->x,e->y);return 1;}
            if(e->player) {
                copy_text(full,sizeof full,details);
                char *tail=strstr(full,"\n附近信息\n");if(tail)*tail=0;
                if(e->name[0]) {size_t used=strlen(full);snprintf(full+used,sizeof full-used,"\n%s",e->name);}
            } else {
                snprintf(full,sizeof full,"%s\n",e->name);
                for(int n=0;n<e->line_count;n++) {
                    size_t used=strlen(full);snprintf(full+used,sizeof full-used,"\n%s",e->lines[n]);
                }
                size_t used=strlen(full);snprintf(full+used,sizeof full-used,"\n\n这是原版侧栏已经显示的已知信息。");
            }
            br_reader_open(e->player?"角色信息":e->name,full);
        }
    }
    /* Consume even unknown/page/pointer keys. No event may resize the native
       window or become a game action while this display-only panel is open. */
    return event->type!=SDL_QUIT && event->type!=SDL_WINDOWEVENT;
}

static int clamp(int value, int low, int high) {
    if (high < low) return low;
    return value < low ? low : value > high ? high : value;
}
void br_map_clear(int cols, int lines) {
    aiming=aim_overview=0;context_note[0]=0;nearby_open=nearby_located=nearby_count=0;
    alert[0]=0;alert_strength=0;
    columns = clamp(cols, 0, 128); rows = clamp(lines, 0, 64);
    memset(cells, 0, sizeof cells);
}
void br_map_put(int x, int y, int atlas, SDL_Color fore, SDL_Color back) {
    if (x < 0 || y < 0 || x >= columns || y >= rows) return;
    cells[x][y] = (Cell){atlas, fore, back};
}
void br_map_player(int x, int y, int floor, int health, int maximum) {
    px = x; py = y; depth = floor; hp = health; max_hp = maximum;
}
static void copy_text(char *to,size_t capacity,const char *from) {
    size_t used=0;
    for(BrTextToken t;(t=br_text_next(from)).bytes;from+=t.bytes) {
        if(used+t.bytes>=capacity) break;
        memcpy(to+used,from,t.bytes);used+=t.bytes;
    }
    to[used]=0;
}
void br_map_context_message(const char *value) {copy_text(context_note,sizeof context_note,value);}
void br_map_searching(int enabled) {searching=enabled;}
void br_map_hud(const char *values,const char *effects,const char *full,
                int hunger_value,int hunger_max,int hunger_level) {
    copy_text(metrics,sizeof metrics,values);copy_text(statuses,sizeof statuses,effects);
    copy_text(details,sizeof details,full);
    nutrition=hunger_value;nutrition_max=hunger_max;hunger=hunger_level;
}
void br_map_exploring(int enabled) {
    exploring = enabled;
    if (!enabled) mode = 0;
}
void br_map_session(int active,int automatic) {
    session=active;automated=active && automatic;
    if(!active) {alert[0]=0;alert_strength=0;nearby_open=nearby_located=0;}
}
void br_map_alert(const char *value,SDL_Color fore,SDL_Color back,int strength) {
    copy_text(alert,sizeof alert,value);
    alert_fore=fore;alert_back=back;alert_strength=clamp(strength,0,100);
}
void br_map_acknowledging(int enabled) {acknowledging=enabled;}
int br_map_ack_active(void) {return acknowledging;}
void br_map_message(const char *text) {
    size_t used = 0;
    for (BrTextToken token; (token = br_text_next(text)).bytes; text += token.bytes) {
        if (used + token.bytes >= sizeof message) break;
        memcpy(message + used, text, token.bytes); used += token.bytes;
    }
    while (used && message[used-1]=='\n') used--;
    message[used] = 0;
}
void br_map_target(int enabled,int x,int y) {
    if(!aiming || !enabled)aim_overview=0;
    aiming=enabled;tx=clamp(x,0,columns-1);ty=clamp(y,0,rows-1);
}
void br_map_inspect(int enabled,int x,int y) {br_map_target(enabled,x,y);if(enabled)aiming=2;}
int br_map_aiming(void) {return aiming;}
void br_map_playback(int enabled,int paused,int out_of_sync,unsigned long turn,unsigned long total) {
    replay=enabled;replay_paused=paused;replay_oos=out_of_sync;replay_turn=turn;replay_total=total;
}
int br_map_browsing(void) {return mode!=0;}
int br_map_moving(void) { return exploring && !mode && !aiming && !acknowledging && !nearby_open; }
/* exploring controls command routing only. The game session owns rendering,
   including turn animations, travel, health alerts and acknowledgment waits. */
int br_map_showing(void) { return columns && rows && (acknowledging || aiming || ((exploring || session || replay) && (local || mode))); }

int br_map_event(const SDL_Event *event) {
    if(event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_F9) {
        if(!event->key.repeat && br_map_showing())nearby_overlay=(nearby_overlay+1)%3;
        return 1;
    }
    if(nearby_open) {nearby_selected=clamp(nearby_selected,0,nearby_count-1);return nearby_event(event);}
    if(acknowledging) {
        if(event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_F5) {
            if(!event->key.repeat)nearby_toggle();
            return 1;
        }
        return 0; /* Native acknowledgment loop owns all confirm/cancel events. */
    }
    if (closing) {
        if (event->type == SDL_TEXTINPUT || event->type == SDL_KEYUP) return 1;
        if (event->type == SDL_KEYDOWN) closing = 0;
    }
    if(aiming) {
        if(event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_F3) {
            if(!event->key.repeat)aim_overview=!aim_overview;
            return 1;
        }
        if(event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_F5) {
            if(!event->key.repeat)nearby_toggle();
            return 1;
        }
        return 0; /* Every aim/confirm/cancel remains an original game event. */
    }
    if ((!exploring && !replay) || !columns || !rows) return 0;
    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_F5) {
            if(!event->key.repeat)nearby_toggle();
            return 1;
        }
        if (key == SDLK_F3) {
            if (!event->key.repeat) {
                if (mode) { mode = 0; closing = 1; }
                else { mode = 1; cx = px; cy = py; }
            }
            return 1;
        }
        if (!mode && key == SDLK_F4) {
            if (!event->key.repeat) local = !local;
            return 1;
        }
        if (!mode) return 0;
        if (key == SDLK_b || key == SDLK_ESCAPE) {
            if (mode == 2) mode = 1;
            else { mode = 0; closing = 1; }
        } else if (key == SDLK_a || key == SDLK_RETURN) {
            if(!event->key.repeat) {
                if(mode==1)mode=2;
                else br_game_map_details(cx,cy);
            }
        }
        else if (key == SDLK_HOME) { cx = px; cy = py; }
        else if (key == SDLK_LEFT) cx = clamp(cx - 1, 0, columns - 1);
        else if (key == SDLK_RIGHT) cx = clamp(cx + 1, 0, columns - 1);
        else if (key == SDLK_UP) cy = clamp(cy - 1, 0, rows - 1);
        else if (key == SDLK_DOWN) cy = clamp(cy + 1, 0, rows - 1);
        return 1;
    }
    return mode && (event->type == SDL_KEYUP || event->type == SDL_TEXTINPUT ||
        event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEBUTTONDOWN ||
        event->type == SDL_MOUSEBUTTONUP || event->type == SDL_MOUSEWHEEL);
}

int br_map_pointer(int x, int y, int *mx, int *my) {
    if (!br_map_showing() || nearby_open || (!aiming && mode) || tile_w <= 0 || tile_h <= 0 ||
        x < viewport.x || y < viewport.y || x >= viewport.x + viewport.w ||
        y >= viewport.y + viewport.h) return 0;
    *mx = origin_x + (x - viewport.x) / tile_w;
    *my = origin_y + (y - viewport.y) / tile_h;
    return *mx < columns && *my < rows;
}

static void text(SDL_Renderer *renderer, const char *value, int x, int y, SDL_Color color) {
    for (BrTextToken token; (token = br_text_next(value)).bytes; value += token.bytes) {
        if (token.is_color) {
            const unsigned char *c=(const unsigned char*)value;
            color=(SDL_Color){(c[1]-25)*255/100,(c[2]-25)*255/100,(c[3]-25)*255/100,255};continue;
        }
        br_draw_glyph(renderer, token.codepoint, (SDL_Rect){x,y,token.columns*10,20}, color);
        x += token.columns * 10;
    }
}
static void border(SDL_Renderer *r, int x, int y, SDL_Color color) {
    int dx = x - origin_x, dy = y - origin_y;
    if (dx < 0 || dy < 0 || dx >= viewport.w / tile_w || dy >= viewport.h / tile_h) return;
    SDL_Rect box = {viewport.x + dx*tile_w, viewport.y + dy*tile_h, tile_w, tile_h};
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,255); SDL_RenderDrawRect(r,&box);
}
/* Returns the columns actually drawn, so a following label can be placed
   after the text instead of on top of it. */
static int clipped_text(SDL_Renderer *r,const char *value,int x,int y,SDL_Color ink,int limit) {
    char wrapped[2400];if(br_text_wrap(wrapped,sizeof wrapped,value,limit)<0)return 0;
    char *line=strchr(wrapped,'\n');if(line)*line=0;
    text(r,wrapped,x,y,ink);
    return br_text_width(wrapped);
}
static void panel_fill(SDL_Renderer *r,SDL_Rect box,SDL_Color ink) {
    SDL_SetRenderDrawColor(r,ink.r,ink.g,ink.b,255);SDL_RenderFillRect(r,&box);
}
/* One bar grammar for the whole screen: a solid trough with a solid fill, the
   same picture the top row already draws. The neighbouring panel used to spell
   its bar out in ASCII and then lift the hue, so a single colour appeared three
   ways on one screen. */
static void draw_bar(SDL_Renderer *r,int x,int y,int width,int height,int percent,SDL_Color ink) {
    int filled=clamp(percent,0,100)*width/100;
    panel_fill(r,(SDL_Rect){x,y,width,height},BR_UI_TROUGH);
    if(filled>0)panel_fill(r,(SDL_Rect){x,y,filled,height},ink);
}
static int entry_screen_x(int x) {return viewport.x+(x-origin_x)*tile_w;}
/* Upstream's sidebar wraps a name at 17 columns. This panel is wider, so a name
   has to be wrapped again for the panel's own width — drawing only its first
   upstream break is what cut a long scroll name down to half. */
#define BR_NEARBY_ROW_H 24
static int wrapped_rows(const char *value,int limit) {
    char wrapped[2400];
    int rows=br_text_wrap(wrapped,sizeof wrapped,value,limit);
    return rows<1?1:rows;
}
static int draw_wrapped(SDL_Renderer *r,const char *value,int x,int y,int limit,SDL_Color ink) {
    char wrapped[2400];
    int rows=br_text_wrap(wrapped,sizeof wrapped,value,limit);
    if(rows<1)rows=1;
    int row=0;
    for(char *line=wrapped;;) {
        char *nl=strchr(line,'\n');
        if(nl)*nl=0;
        text(r,line,x,y+row*BR_NEARBY_ROW_H,ink);
        row++;
        if(!nl)break;
        line=nl+1;
    }
    return rows;
}
/* An entry claims only the height it uses, and the draw loop below branches on
   exactly the same cases so a row can never be half-painted. A creature carries
   a gauge and one decisive state; an item or a terrain line is nothing but its
   name, so its height is whatever that name wraps to. */
static int entry_height(const BrNearby *e,int visible,int limit,int *bar,int *state) {
    *bar=*state=-1;
    for(int n=0;n<e->line_count;n++)if(e->bars[n]>=0) {*bar=n;break;}
    int rows=wrapped_rows(e->name,limit);
    if(!visible) return rows+1;              /* the name, then the located hint */
    if(*bar>=0) {
        for(int n=e->line_count-1;n>=0;n--)if(e->bars[n]<0) {*state=n;break;}
        if(*state<0 && e->line_count>1)*state=1;
        return rows+1+(*state>=0);           /* name, gauge, one decisive state */
    }
    return rows+e->line_count;               /* the name, plus any continuation */
}
static int entry_visible(const BrNearby *e) {
    return e->x>=origin_x && e->x<origin_x+viewport.w/tile_w
        && e->y>=origin_y && e->y<origin_y+viewport.h/tile_h;
}
/* Both windows share the placement decision, but neither changes the camera. */
static int nearby_panel_left(int width) {
    if(nearby_overlay)return nearby_overlay==1?width-302:16;
    BrNearby *focus=nearby_count?&nearby[clamp(nearby_selected,0,nearby_count-1)]:NULL;
    int sx=focus?entry_screen_x(focus->x):width/2,player_x=entry_screen_x(px);
    int left_cost=(sx<302)*2+(player_x<302);
    int right_cost=(sx>=width-302)*2+(player_x>=width-302);
    return left_cost<=right_cost?16:width-302;
}
static void small_text(SDL_Renderer *r,const char *value,int x,int y,int limit,SDL_Color ink) {
    int used=0;
    for(BrTextToken t;(t=br_text_next(value)).bytes;value+=t.bytes) {
        if(t.is_color)continue;
        if(used+t.columns>limit)break;
        br_draw_glyph(r,t.codepoint,(SDL_Rect){x+used*6,y,t.columns*6,12},ink);used+=t.columns;
    }
}
static void draw_nearby_overlay(SDL_Renderer *r,int width,int height,SDL_Texture *atlas,int aw,int ah) {
    int ids[30],heights[30],count=0,total=0,used=24;
    int x=nearby_overlay==1?16:width-194,y=64;
    /* Copy the native sidebar order, including items and terrain. Only the
       player is already represented by the permanent HUD. No hidden queries. */
    for(int i=0;i<nearby_count;i++)if(!nearby[i].player)total++;
    for(int i=0;i<nearby_count;i++)if(!nearby[i].player) {
        char wrapped[2400];int rows=br_text_wrap(wrapped,sizeof wrapped,nearby[i].name,23);
        int h=(rows>0?rows:1)*14+4;
        for(int k=0;k<nearby[i].line_count;k++)if(nearby[i].bars[k]>=0){h+=16;break;}
        if(used+h+16>height-80-y)break;
        ids[count]=i;heights[count++]=h;used+=h;
    }
    SDL_BlendMode blend;SDL_GetRenderDrawBlendMode(r,&blend);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r,18,15,38,160);
    SDL_Rect background={x,y,178,used+(total>count?16:count?0:20)};
    SDL_RenderFillRect(r,&background);
    SDL_SetRenderDrawBlendMode(r,blend);
    char title[80];snprintf(title,sizeof title,"附近 %d  · R2+R1",total);
    small_text(r,title,x+10,y+6,26,BR_UI_MUTED);
    if(!count && !total)small_text(r,"暂无附近对象",x+10,y+24,26,BR_UI_MUTED);
    int row=y+24;
    for(int n=0;n<count;n++) {
        BrNearby *e=&nearby[ids[n]];int bar=-1,state=-1;
        if(e->atlas>=0 && e->atlas<384) {
            SDL_Rect src={(e->atlas%16)*aw,(e->atlas/16)*ah,aw,ah},dst={x+10,row,12,14};
            SDL_SetTextureColorMod(atlas,e->ink.r,e->ink.g,e->ink.b);SDL_RenderCopy(r,atlas,&src,&dst);
        }
        char wrapped[2400];br_text_wrap(wrapped,sizeof wrapped,e->name,23);
        int text_y=row;
        for(char *line=wrapped;;) {
            char *nl=strchr(line,'\n');if(nl)*nl=0;
            small_text(r,line,x+30,text_y,23,e->ink);text_y+=14;
            if(!nl)break;
            line=nl+1;
        }
        for(int k=0;k<e->line_count;k++) {
            if(e->bars[k]>=0 && bar<0)bar=k;
            if(e->bars[k]<0)state=k;
        }
        if(bar>=0) {
            draw_bar(r,x+30,text_y+5,60,4,e->bars[bar],BR_UI_BAR_RED);
            if(state>=0)small_text(r,e->lines[state],x+96,text_y,12,e->tones[state]);
        }
        row+=heights[n];
    }
    if(total>count) {
        char more[80];snprintf(more,sizeof more,"另 %d 项 · R1 查看",total-count);
        small_text(r,more,x+10,row,26,BR_UI_MUTED);
    }
}
static void draw_nearby(SDL_Renderer *r,int width,int height,SDL_Texture *atlas,int aw,int ah) {
    int panel_width=286,top=60,bottom=height-92;
    if(bottom-top<120)return;
    if(nearby_selected>=nearby_count)nearby_selected=nearby_count?nearby_count-1:0;
    BrNearby *focus=nearby_count?&nearby[nearby_selected]:NULL;
    int left=nearby_panel_left(width);
    panel_fill(r,(SDL_Rect){left,top,panel_width,bottom-top},BR_UI_BOX);
    text(r,"角色与附近",left+14,top+8,BR_UI_FOCUS);
    char pagetext[48];snprintf(pagetext,sizeof pagetext,"%d/%d",nearby_count?nearby_selected+1:0,nearby_count);
    text(r,pagetext,left+panel_width-14-br_text_width(pagetext)*10,top+8,BR_UI_MUTED);
    if(!focus)text(r,"暂无侧栏对象",left+14,top+42,BR_UI_MUTED);
    /* Paginate on the height each entry really needs, so a page of potions or
       floor items shows as many as fit instead of one per screen. */
    int usable=bottom-top-46,used=0,pages=0;
    int heights[30],page_first[31];
    const int name_limit=(panel_width-68)/10;
    page_first[0]=0;
    for(int i=0;i<nearby_count;i++) {
        int bar,state;
        heights[i]=entry_height(&nearby[i],entry_visible(&nearby[i]),name_limit,&bar,&state)*BR_NEARBY_ROW_H+6;
        if(used && used+heights[i]>usable) {page_first[++pages]=i;used=0;}
        used+=heights[i];
    }
    page_first[++pages]=nearby_count;
    int page=0;while(page+1<pages && nearby_selected>=page_first[page+1])page++;
    int first=page_first[page],last=page_first[page+1];
    nearby_page_size=last-first;
    if(nearby_page_size<1)nearby_page_size=1;
    int y=top+40;
    for(int i=first;i<last;i++) {
        BrNearby *e=&nearby[i];int selected=i==nearby_selected;
        int bar,state;
        int visible=entry_visible(e);
        /* entry_height returns the whole entry; only the name block starts at y,
           so the gauge and the state follow the name's own wrapped rows. */
        int name_rows=wrapped_rows(e->name,name_limit);
        entry_height(e,visible,name_limit,&bar,&state);
        int entry_h=heights[i]-6;
        if(selected)panel_fill(r,(SDL_Rect){left+8,y-2,panel_width-16,entry_h},BR_UI_SELECTED);
        if(selected)text(r,">",left+12,y,BR_UI_FOCUS);
        if(e->atlas>=0 && e->atlas<384) {
            SDL_Rect src={(e->atlas%16)*aw,(e->atlas/16)*ah,aw,ah},dst={left+28,y,18,22};
            SDL_SetTextureColorMod(atlas,e->ink.r,e->ink.g,e->ink.b);SDL_RenderCopy(r,atlas,&src,&dst);
        }
        /* The full name keeps its own colour and takes as many rows as it needs;
           nothing is trimmed to fit a fixed row. */
        draw_wrapped(r,e->name,left+52,y,name_limit,e->ink);
        int row=y+name_rows*BR_NEARBY_ROW_H;
        if(!visible) {
            text(r,"画面外 · X 定位",left+28,row,BR_UI_MUTED);
        } else if(bar>=0) {
            char label[1024];copy_text(label,sizeof label,e->lines[bar]);
            char *suffix=strstr(label," 约 ");if(suffix)*suffix=0;
            clipped_text(r,label,left+28,row,BR_UI_INK,(panel_width-142)/10);
            draw_bar(r,left+panel_width-100,row+3,80,10,e->bars[bar],BR_UI_BAR_RED);
            if(state>=0)clipped_text(r,e->lines[state],left+28,row+BR_NEARBY_ROW_H,
                e->tones[state],(panel_width-44)/10);
        } else {
            /* No gauge: these rows are the rest of the name's own continuation. */
            for(int n=0;n<e->line_count;n++)
                clipped_text(r,e->lines[n],left+52,row+n*BR_NEARBY_ROW_H,
                    e->tones[n],name_limit);
        }
        y+=heights[i];
    }
    panel_fill(r,(SDL_Rect){0,height-28,width,28},(SDL_Color){0,0,0,255});
    text(r,nearby_located?"上下选对象  A 详情  B 回原视野":"上下选对象  A 详情  X 定位  L1/R1 翻页  B 返回",18,height-24,BR_UI_MUTED);
}
void br_map_draw(SDL_Renderer *r, int width, int height, SDL_Texture *atlas, int aw, int ah) {
    if (!br_map_showing()) return;
    SDL_SetRenderDrawColor(r,0,0,0,255); SDL_RenderClear(r);
    char title[256],health[64],food[64];
    /* The right column is one instrument: label, gauge, value, stacked. The
       left column carries what describes the situation — depth and mode on the
       first row, the derived stats and active effects on the second. */
    snprintf(title,sizeof title,"@ 第 %d 层",depth);
    text(r,title,18,7,BR_UI_FOCUS);
    const char *state=acknowledging?(acknowledging==2?"[本局结束]":"[等待确认]"):
        aiming?(aiming==2?"[观察]":"[瞄准]"):mode?"[地图]":automated?"[自动行动]":searching?"[搜索中]":"";
    if(replay)state=replay_oos?"[录像不同步]":replay_paused?"[录像暂停]":"[录像播放]";
    if(*state)text(r,state,18+(br_text_width(title)+2)*10,7,BR_UI_FOCUS);
    /* Status effects ride the first row. Danger belongs where the eye lands,
       and row two is now fully occupied by the three derived stats — leaving
       them there would collapse almost every state into an anonymous +N. */
    if(statuses[0] && !replay) {
        int sx=18+(br_text_width(title)+2+br_text_width(state)+2)*10;
        if(!effect_count)clipped_text(r,statuses,sx,7,BR_UI_DANGER,(BR_HUD_LABEL_X-sx)/10);
        else {
            int shown=0;
            for(int i=0;i<effect_count && shown<1;i++) {
                int w=br_text_width(effects[i].name)*10;
                if(sx+w+12+(i+1<effect_count?30:0)>BR_HUD_LABEL_X)break;
                text(r,effects[i].name,sx,7,effects[i].ink);
                draw_bar(r,sx,28,w,3,effects[i].percent,effects[i].ink);
                sx+=w+12;shown++;
            }
            if(shown<effect_count) {
                char more[16];snprintf(more,sizeof more,"+%d",effect_count-shown);
                clipped_text(r,more,sx,7,BR_UI_MUTED,(BR_HUD_LABEL_X-sx)/10);
            }
        }
    }
    text(r,"生命",BR_HUD_LABEL_X,7,BR_UI_INK);
    draw_bar(r,BR_HUD_BAR_X,15,BR_HUD_BAR_W,12,
        max_hp>0 ? clamp(hp*100/max_hp,0,100) : 0,BR_UI_BAR_RED);
    snprintf(health,sizeof health,"%d/%d",hp,max_hp);
    text(r,health,BR_HUD_VALUE_X,7,(max_hp>0 && hp*100<max_hp*25)?BR_UI_DANGER:BR_UI_INK);
    if(metrics[0])text(r,metrics,18,34,BR_UI_MUTED);
    if(replay) {
        snprintf(title,sizeof title,"%lu/%lu",replay_turn,replay_total);
        text(r,title,BR_HUD_LABEL_X,34,BR_UI_MUTED);
    } else {
        /* Nutrition mirrors health directly beneath it, value included, so the
           two bars line up as a pair. The hue follows the hunger tier. */
        SDL_Color ink=hunger>=2?BR_UI_BAR_FAINT:hunger==1?BR_UI_BAR_HUNGRY:BR_UI_BAR_FED;
        text(r,"饱食",BR_HUD_LABEL_X,34,hunger>=2?BR_UI_DANGER:BR_UI_INK);
        draw_bar(r,BR_HUD_BAR_X,42,BR_HUD_BAR_W,12,
            nutrition_max>0?clamp(nutrition*100/nutrition_max,0,100):0,ink);
        snprintf(food,sizeof food,"%d/%d",nutrition,nutrition_max);
        text(r,food,BR_HUD_VALUE_X,34,hunger>=2?BR_UI_DANGER:BR_UI_MUTED);
    }
    /* Exactly two fixed rows: warnings, search and effects never reflow it. */
    int top=56;
    int available_w = width-32, available_h = height-top-86;
    if (available_w < 1 || available_h < 1) return;
    int visible_w, visible_h;
    if (!nearby_located && !acknowledging && ((aiming && aim_overview) || (!aiming && mode == 1))) {
        /* Overview keeps the cell's 18:22 proportions instead of forcing one
           square size for both axes. A square cell stretched every glyph 22%
           wide and left the full height of a 29-row map unused. */
        tile_w = clamp(available_w/columns,1,64);
        tile_h = tile_w*22/18;
        if(tile_h*rows>available_h) {
            tile_h=clamp(available_h/rows,1,64);
            tile_w=tile_h*18/22;
            if(tile_w<1)tile_w=1;
        }
        visible_w = columns; visible_h = rows; origin_x = origin_y = 0;
    } else {
        tile_w = 18; tile_h = 22;
        visible_w = clamp(available_w/tile_w,1,columns);
        visible_h = clamp(available_h/tile_h,1,rows);
        int fx=nearby_located && nearby_count?nearby[nearby_selected].x:aiming?tx:mode?cx:px;
        int fy=nearby_located && nearby_count?nearby[nearby_selected].y:aiming?ty:mode?cy:py;
        origin_x = clamp(fx-visible_w/2,0,columns-visible_w);
        origin_y = clamp(fy-visible_h/2,0,rows-visible_h);
    }
    viewport = (SDL_Rect){(width-visible_w*tile_w)/2,
        top+(available_h-visible_h*tile_h)/2, visible_w*tile_w, visible_h*tile_h};
    for (int y=0; y<visible_h; y++) for (int x=0; x<visible_w; x++) {
        Cell *cell = &cells[x+origin_x][y+origin_y];
        SDL_Rect dest = {viewport.x+x*tile_w,viewport.y+y*tile_h,tile_w,tile_h};
        SDL_SetRenderDrawColor(r,cell->back.r,cell->back.g,cell->back.b,255);
        SDL_RenderFillRect(r,&dest);
        if (cell->atlas < 0 || cell->atlas >= 384) continue;
        SDL_Rect src = {(cell->atlas%16)*aw,(cell->atlas/16)*ah,aw,ah};
        SDL_SetTextureColorMod(atlas,cell->fore.r,cell->fore.g,cell->fore.b);
        SDL_RenderCopy(r,atlas,&src,&dest);
    }
    border(r,px,py,(SDL_Color){95,220,155,255});          /* player: unchanged */
    if (aiming || mode) border(r,aiming?tx:cx,aiming?ty:cy,(SDL_Color){255,210,75,255});
    if(nearby_open)nearby_selected=clamp(nearby_selected,0,nearby_count-1);
    if(nearby_open && nearby_count)border(r,nearby[nearby_selected].x,nearby[nearby_selected].y,BR_UI_FOCUS);
    if(nearby_open)draw_nearby(r,width,height,atlas,aw,ah);
    if(alert[0] && alert_strength) {
        /* The upstream flash still owns its timing, RNG and interruption.
           Paint its text in screen space instead of changing map geometry. */
        char wrapped[4096];
        if(br_text_wrap(wrapped,sizeof wrapped,alert,(width-64)/10)>=0) {
            char *newline=strchr(wrapped,'\n');if(newline)*newline=0;
            int w=clamp(br_text_width(wrapped)*10+24,24,width-32);
            SDL_Rect box={(width-w)/2,viewport.y+viewport.h/2-16,w,32};
            SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r,alert_back.r,alert_back.g,alert_back.b,alert_strength*255/100);
            SDL_RenderFillRect(r,&box);
            SDL_Color fore=alert_fore;fore.a=alert_strength*255/100;
            text(r,wrapped,box.x+12,box.y+6,fore);
            SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
        }
    }
    char coordinates[128];
    if (acknowledging) snprintf(coordinates,sizeof coordinates,acknowledging==2?"A/B 继续  X 背包  m 消息  R1 信息":"A/B 继续    m 消息    R1 信息");
    else if (automated) snprintf(coordinates,sizeof coordinates,"[B] 中断自动行动");
    else if (aiming || mode) snprintf(coordinates,sizeof coordinates,"光标 %d,%d    角色 %d,%d",(aiming?tx:cx)+1,(aiming?ty:cy)+1,px+1,py+1);
    else if(replay) snprintf(coordinates,sizeof coordinates,"ST 播放  左右 逐回合  上下 变速  L1 菜单");
    else snprintf(coordinates,sizeof coordinates,"X背包 Y搜索 ST等待 m消息 R1信息 L1行动 L2探索");
    if(aiming && !acknowledging) {
        size_t used=strlen(coordinates);snprintf(coordinates+used,sizeof coordinates-used,"  Select 全图  R1 信息");
    }
    if(!nearby_open)text(r,coordinates,16,height-80,BR_UI_HINT);
    if(nearby_open) {
        /* The panel footer owns the last line while the map stays visible. */
    } else if(aiming && !acknowledging) {
        text(r,aiming==2?(replay?"方向检查  B 返回  X 全知视角":"A 前往  B 返回  X 切换目标"):"A 确认  B 取消  X 切换目标",16,height-48,BR_UI_INK);
        char wrapped[4096];
        if(br_text_wrap(wrapped,sizeof wrapped,context_note,(width-32)/10)>=0) {
            char *newline=strchr(wrapped,'\n');if(newline)*newline=0;
            text(r,wrapped,16,height-24,BR_UI_HINT);
        }
    } else if (mode && !acknowledging) {
        text(r,mode == 1 ? "方向浏览    A 放大    B 返回    X 回角色" :
             "方向浏览  A 详情  X 回角色  B 全图  Select 退出",16,height-48,BR_UI_INK);
    } else {
        char wrapped[24000];
        int count=br_text_wrap(wrapped,sizeof wrapped,message,(width-32)/10);
        if (count<0) return;
        int first=count>2 ? count-2 : 0;
        int line=0,x=16,y=height-50;
        SDL_Color color=BR_UI_INK;
        /* A hairline separates the running log from the map, so the two bottom
           lines do not read as one grey block. It sits below the key hint's own
           20px box: at height-62 it cut through the hint's descenders. */
        br_ui_rule(r,16,height-55,width-16);
        /* Prioritize the newest lines, retaining styles from preceding lines.
           Full messages remain available in F2. */
        for (const char *p=wrapped; *p;) {
            BrTextToken token=br_text_next(p);
            if (token.is_color) {
                const unsigned char *c=(const unsigned char*)p;
                color=(SDL_Color){(c[1]-25)*255/100,(c[2]-25)*255/100,(c[3]-25)*255/100,255};
            } else if (token.codepoint=='\n') {line++; x=16; if(line>first)y+=24;}
            else if(line>=first && line<first+2) {
                br_draw_glyph(r,token.codepoint,(SDL_Rect){x,y,token.columns*10,20},color);x+=token.columns*10;
            }
            p+=token.bytes;
        }
    }
}

/* Independent screen-space composition pass. The platform calls this after the
   complete map/HUD, before presentation; it owns no tile or camera geometry. */
void br_nearby_overlay_draw(SDL_Renderer *r,int width,int height,SDL_Texture *atlas,int aw,int ah) {
    if(br_map_showing() && nearby_overlay)
        draw_nearby_overlay(r,width,height,atlas,aw,ah);
}

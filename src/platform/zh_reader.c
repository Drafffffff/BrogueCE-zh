/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "zh_reader.h"
#include "zh_glyph.h"
#include "zh_ui.h"
#include "zh_text.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

static char content[20000] = "打开物品说明或查看消息后，按 L1 进入大字阅读。";
static char alternate[20000], alternate_title[160];
static int active, offset, maximum, closing, use_alternate;
static void copy_text(char *to,size_t capacity,const char *from);
static int page_step=13,history_shown,history_offset=INT_MAX;
#define HISTORY_CAPACITY 600000
static char history[HISTORY_CAPACITY],history_wrapped[HISTORY_CAPACITY*2];
static size_t history_used;
static int history_count,history_dirty=1,history_width,history_line_count;
typedef struct {const char *text;SDL_Color ink;} HistoryLine;
static HistoryLine *history_lines;
int br_reader_history_begin(void) {
    if(active)return 0;
    history_used=history_count=0;history[0]=0;history_dirty=1;
    return 1;
}
void br_reader_history_add(unsigned long turn,const char *value) {
    if(active || history_count>=500)return;
    char message[1152];
    copy_text(message,sizeof message,value);
    int n=snprintf(history+history_used,sizeof history-history_used,"\x19\x5e\x5a\x63%lu · \x19\x70\x6e\x70%s\n",turn,message);
    if(n<0 || (size_t)n>=sizeof history-history_used) {history[history_used]=0;return;}
    history_used+=n;history_count++;
}
void br_reader_history_end(void) {
    if(active)return;
    if(history_used && history[history_used-1]=='\n')history[--history_used]=0;
    history_offset=INT_MAX;
}
int br_reader_history_count(void) {return history_count;}
static void close_reader(void) {
    if(history_shown)history_offset=offset;
    active=0;closing=1;use_alternate=history_shown=0;
}
static SDL_Color decode_ink(const char *value) {
    const unsigned char *c=(const unsigned char*)value;
    return (SDL_Color){(c[1]-25)*255/100,(c[2]-25)*255/100,(c[3]-25)*255/100,255};
}
static void copy_text(char *to,size_t capacity,const char *from) {
    size_t used=0;
    for (BrTextToken t;(t=br_text_next(from)).bytes;from+=t.bytes) {
        if(used+t.bytes>=capacity) break;
        memcpy(to+used,from,t.bytes); used+=t.bytes;
    }
    to[used]=0;
}
void br_reader_open(const char *title,const char *text) {
    if(active) return;
    copy_text(alternate,sizeof alternate,text);
    copy_text(alternate_title,sizeof alternate_title,title);
    use_alternate=active=1;history_shown=0; offset=maximum=closing=0;
}
void br_reader_set(const char *text) {
    if (active) return;
    size_t used=0;
    for (BrTextToken t; (t=br_text_next(text)).bytes; text+=t.bytes) {
        if (used+t.bytes >= sizeof(content)) break;
        memcpy(content+used,text,t.bytes); used+=t.bytes;
    }
    content[used]=0; offset=0;
}
int br_reader_active(void) { return active; }
int br_reader_event(const SDL_Event *e) {
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_F2) {
        if(!e->key.repeat) {
            if(active)close_reader();
            else {active=1;closing=use_alternate=0;history_shown=history_count>0;offset=history_shown?history_offset:0;}
        }
        return 1;
    }
    if (closing) {
        if (e->type==SDL_TEXTINPUT || e->type==SDL_KEYUP) return 1;
        if (e->type==SDL_KEYDOWN) closing=0;
    }
    if (!active) return 0;
    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode key=e->key.keysym.sym;
        if (key==SDLK_ESCAPE || key==SDLK_b || key==SDLK_RETURN || key==SDLK_a) { close_reader(); }
        else if (key==SDLK_UP && offset>0) offset--;
        else if (key==SDLK_DOWN && offset<maximum) offset++;
        else if (key==SDLK_PAGEUP) offset=offset>page_step?offset-page_step:0;
        else if (key==SDLK_PAGEDOWN) offset=offset+page_step<maximum?offset+page_step:maximum;
        else if(key==SDLK_END)offset=maximum;
        else if(key==SDLK_HOME)offset=0;
        return 1;
    }
    return e->type==SDL_KEYUP || e->type==SDL_TEXTINPUT || e->type==SDL_MOUSEBUTTONDOWN || e->type==SDL_MOUSEBUTTONUP || e->type==SDL_MOUSEWHEEL;
}

static void draw_line(SDL_Renderer *r,const char *text,int x,int y,SDL_Color color) {
    for (BrTextToken t;(t=br_text_next(text)).bytes;text+=t.bytes) {
        if(t.is_color) continue;
        br_draw_glyph(r,t.codepoint,(SDL_Rect){x,y,t.columns*10,20},color);
        x+=t.columns*10;
    }
}
static int history_cache(int columns) {
    if(!history_dirty && history_width==columns)return history_line_count>0;
    int count=br_text_wrap(history_wrapped,sizeof history_wrapped,history,columns);
    if(count<1)return 0;
    HistoryLine *next=realloc(history_lines,(size_t)count*sizeof *next);
    if(!next)return 0;
    history_lines=next;history_width=columns;history_line_count=count;history_dirty=0;
    SDL_Color ink=BR_UI_INK;int row=0;
    history_lines[0]=(HistoryLine){history_wrapped,ink};
    for(const char *p=history_wrapped;*p;) {
        BrTextToken t=br_text_next(p);
        if(t.is_color)ink=decode_ink(p);
        else if(t.codepoint=='\n' && row+1<count)history_lines[++row]=(HistoryLine){p+t.bytes,ink};
        p+=t.bytes;
    }
    return 1;
}
static void draw_history(SDL_Renderer *r,int width,int height) {
    char title[128];snprintf(title,sizeof title,"消息 · 最近 %d 条",history_count);
    br_ui_frame(r,width,height,title,1);
    draw_line(r,"上下回看  L2/R2 翻页  X 最新  B 返回  L1 关闭",20,height-32,BR_UI_MUTED);
    if(!history_cache((width-40)/10))return;
    int visible=(height-100)/26;page_step=visible>1?visible-1:1;if(visible<1)visible=1;
    page_step=visible>1?visible-1:1;
    maximum=history_line_count>visible?history_line_count-visible:0;
    if(offset>maximum)offset=maximum;
    if(offset<0)offset=0;
    history_offset=offset;
    for(int row=offset;row<history_line_count && row<offset+visible;row++) {
        int x=20,y=56+(row-offset)*26;SDL_Color ink=history_lines[row].ink;
        for(const char *p=history_lines[row].text;*p;) {
            BrTextToken t=br_text_next(p);
            if(t.is_color)ink=decode_ink(p);
            else if(t.codepoint=='\n')break;
            else {br_draw_glyph(r,t.codepoint,(SDL_Rect){x,y,t.columns*10,20},ink);x+=t.columns*10;}
            p+=t.bytes;
        }
    }
}
void br_reader_draw(SDL_Renderer *r,int width,int height) {
    if(!active) return;
    if(history_shown) {draw_history(r,width,height);return;}
    br_ui_frame(r,width,height,use_alternate?alternate_title:"消息",1);
    draw_line(r,"上下滚动  L2/R2 翻页  B 返回  L1 关闭",20,height-32,BR_UI_MUTED);
    char wrapped[40000];
    int count=br_text_wrap(wrapped,sizeof wrapped,use_alternate?alternate:content,(width-40)/10);
    if(count<0) return;
    int visible=(height-100)/26;page_step=visible>1?visible-1:1;
    maximum=count>visible?count-visible:0;
    if(offset>maximum)offset=maximum;
    int line=0,x=20,y=56;
    SDL_Color color=BR_UI_INK;
    for(const char *p=wrapped;*p;) {
        BrTextToken t=br_text_next(p);
        if(t.is_color) {
            const unsigned char *c=(const unsigned char*)p;
            color=(SDL_Color){(c[1]-25)*255/100,(c[2]-25)*255/100,(c[3]-25)*255/100,255};
        } else if(t.codepoint=='\n') {line++;x=20;if(line>offset)y+=26;}
        else if(line>=offset && line<offset+visible) {
            br_draw_glyph(r,t.codepoint,(SDL_Rect){x,y,t.columns*10,20},color);x+=t.columns*10;
        }
        p+=t.bytes;
    }
}

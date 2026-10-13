/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "zh_keyboard.h"
#include "zh_text.h"
#include "zh_glyph.h"
#include "zh_ui.h"
#include <string.h>
#include <stdio.h>
static int active,numeric,selected,used,limit,closing;
static char prompt[2048],value[2048],suffix[128];
static int count(void) {return (numeric?10:95)+3;}
static void copy(char *to,size_t cap,const char *from) {
    size_t n=0;
    for(BrTextToken t;(t=br_text_next(from)).bytes;from+=t.bytes) {
        if(n+t.bytes>=cap) break;
        memcpy(to+n,from,t.bytes);n+=t.bytes;
    }
    to[n]=0;
}
void br_keyboard_begin(const char *heading,int digits) {
    copy(prompt,sizeof prompt,heading);numeric=digits;selected=0;active=1;closing=0;
}
void br_keyboard_value(const char *text,int bytes,int capacity,const char *ending) {
    if(bytes<0)bytes=0;
    if(bytes>=(int)sizeof value)bytes=sizeof value-1;
    memcpy(value,text,bytes);value[bytes]=0;used=bytes;limit=capacity;
    copy(suffix,sizeof suffix,ending);
}
void br_keyboard_end(void) {active=0;closing=1;}
int br_keyboard_active(void) {return active;}
int br_keyboard_event(SDL_Event *e) {
    if(closing) {
        if(e->type==SDL_KEYUP || e->type==SDL_TEXTINPUT) return 1;
        if(e->type==SDL_KEYDOWN) closing=0;
    }
    if(!active) return 0;
    if(e->type==SDL_TEXTINPUT) return 0; /* Original ASCII/type/filename filter remains authoritative. */
    if(e->type==SDL_KEYDOWN) {
        int step=0;SDL_Keycode key=e->key.keysym.sym;
        if(key==SDLK_ESCAPE || key==SDLK_BACKSPACE) return 0;
        if(key==SDLK_F7) {if(e->key.repeat)return 1;e->key.keysym.sym=SDLK_RETURN;return 0;}
        if(key==SDLK_LEFT)step=-1;
        if(key==SDLK_RIGHT)step=1;
        if(key==SDLK_UP)step=-12;
        if(key==SDLK_DOWN)step=12;
        if(step) {selected=(selected+step+count())%count();return 1;}
        if(key==SDLK_RETURN) {
            if(e->key.repeat) return 1;
            int chars=numeric?10:95;
            if(selected<chars) {
                if(used>=limit)return 1;
                Uint32 window=e->key.windowID;
                memset(e,0,sizeof *e);e->type=SDL_TEXTINPUT;e->text.windowID=window;
                e->text.text[0]=(numeric?'0':' ')+selected;
            } else e->key.keysym.sym=selected==chars?SDLK_BACKSPACE:selected==chars+1?SDLK_RETURN:SDLK_ESCAPE;
            return 0;
        }
        /* Physical character keydowns are ignored; the matching SDL_TEXTINPUT is accepted. */
        return 1;
    }
    return e->type==SDL_KEYUP || e->type==SDL_MOUSEMOTION || e->type==SDL_MOUSEBUTTONDOWN || e->type==SDL_MOUSEBUTTONUP || e->type==SDL_MOUSEWHEEL;
}
static void line(SDL_Renderer *r,const char *text,int x,int y,int columns,SDL_Color color) {
    for(BrTextToken t;(t=br_text_next(text)).bytes;text+=t.bytes) {
        if(t.is_color)continue;
        if(t.codepoint=='\n' || columns<t.columns)break;
        br_draw_glyph(r,t.codepoint,(SDL_Rect){x,y,t.columns*10,20},color);
        x+=t.columns*10;columns-=t.columns;
    }
}
void br_keyboard_draw(SDL_Renderer *r,int width,int height) {
    if(!active)return;
    SDL_Color white=BR_UI_INK,gold=BR_UI_FOCUS,gray=BR_UI_MUTED;
    br_ui_frame(r,width,height,numeric?"输入数字":"输入文字",2);
    char wrapped[4096];int columns=(width-40)/10;
    if(br_text_wrap(wrapped,sizeof wrapped,prompt,columns)>=0) {
        char *next=strchr(wrapped,'\n');if(next)*next++=0;
        line(r,wrapped,20,44,columns,white);if(next)line(r,next,20,66,columns,white);
    }
    char caption[2300];snprintf(caption,sizeof caption,"%s_  %s",value,suffix);
    /* Tail remains visible when the original field is wider than this viewport. */
    const char *tail=caption;while(br_text_width(tail)>columns && *tail)tail+=br_text_next(tail).bytes;
    line(r,tail,20,104,columns,white);
    int chars=numeric?10:95,cell=(width-40)/12;
    for(int i=0;i<count();i++) {
        int x=20+i%12*cell,y=150+i/12*28;
        if(i==selected) line(r,">",x,y,1,gold);
        char label[2]={(numeric?'0':' ')+i,0};
        const char *name=i<chars?(label[0]==' '?"空格":label):i==chars?"删":i==chars+1?"定":"返";
        line(r,name,x+14,y,(cell-14)/10,i==selected?gold:white);
    }
    snprintf(caption,sizeof caption,"%d/%d 字节  方向选字符  A 输入  X 删除",used,limit);
    line(r,caption,20,height-54,columns,gray);
    line(r,"ST 完成  B 取消  删=删除  定=完成  返=取消",20,height-28,columns,gray);
}

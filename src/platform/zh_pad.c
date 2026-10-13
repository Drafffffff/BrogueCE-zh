/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "zh_pad.h"
#include <string.h>
#include <stdlib.h>
#ifdef __linux__
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#endif
/* Repeat from held physical state, never the kernel's unbounded repeat queue.
   UI/acknowledgment transitions disarm it until a fresh gesture. */
static int emit(SDL_Event *out,SDL_Keycode key,char text) {
    memset(out,0,sizeof *out);
    if(text) {out->type=SDL_TEXTINPUT;out->text.text[0]=text;}
    else {out->type=SDL_KEYDOWN;out->key.keysym.sym=key;out->key.state=SDL_PRESSED;}
    return 1;
}
static void context_sync(BrPad *p,int context) {
    if(p->context!=context) {p->pending=0;p->chord=0;p->running=0;p->repeat_kind=0;p->menu_used=1;p->context=context;}
}
static int direction_event(unsigned d,int context,int running,SDL_Event *out) {
    int x=!!(d&8)-!!(d&4),y=!!(d&2)-!!(d&1);
    if(!x && !y) return 0;
    if(context==1 || context==3) {
        const char *keys[3]={"yku","h l","bjn"};
        if(context==1 && running) {
            emit(out,keys[y+1][x+1],0);out->key.keysym.mod=KMOD_CTRL;return 1;
        }
        return emit(out,0,keys[y+1][x+1]);
    }
    return emit(out,y<0?SDLK_UP:y>0?SDLK_DOWN:x<0?SDLK_LEFT:SDLK_RIGHT,0);
}
int br_pad_tick(BrPad *p,Uint32 now,int context,SDL_Event *out) {
    context_sync(p,context);
    if(p->held&(1u<<8)) {p->pending=p->repeat_kind=0;return 0;}
    if(p->pending && (Sint32)(now-p->due)>=0) {
        p->pending=0;
        if((context==1 || context==3 || context==5) && p->direction && !p->running) {
            p->repeat_kind=1;p->repeat_due=now+(context==1?300:220);
        }
        return direction_event(p->chord,context,p->running,out);
    }
    /* Walking, aiming and the map cursor all repeat under a held direction.
       The two cursors move at the faster rate; walking keeps its own pace. */
    if(context!=1 && context!=3 && context!=5) return 0;
    if(!p->repeat_kind || (Sint32)(now-p->repeat_due)<0) return 0;
    /* At most one action per poll. Never catch up missed turns after a pause. */
    p->repeat_due=now+(context==1?140:60);
    if(p->repeat_kind==1 && p->direction) return direction_event(p->direction,context,0,out);
    if(context==1 && p->repeat_kind==2 && (p->held&(1u<<2))) return emit(out,0,'s');
    p->repeat_kind=0;return 0;
}
int br_pad_feed(BrPad *p,int code,int value,Uint32 now,int context,SDL_Event *out) {
    context_sync(p,context);
    unsigned bit=code==103?1:code==108?2:code==105?4:code==106?8:0;
    if(value!=0 && value!=1) return 0; /* Kernel repeat never advances turns. */
    if(value==1 && code!=312 && (p->held&(1u<<8)))p->menu_used=1;
    if(bit) {
        if(value) {
            if(p->direction&bit) return 0;
            if(!p->direction && !(p->held&(1u<<8))) {p->pending=1;p->chord=0;p->running=context==1 && (p->held&(1u<<10));p->due=now+45;}
            p->direction|=bit;
            if(p->pending) p->chord|=bit;
        } else {
            p->direction&=~bit;
            if(!p->direction && p->repeat_kind==1) p->repeat_kind=0;
            if(!p->direction && p->pending) {p->due=now;return br_pad_tick(p,now,context,out);}
        }
        return 0;
    }
    int index=code>=304 && code<=312?code-304:code==314?9:code==315?10:-1;
    if(index<0) return 0; /* Volume and power remain available to the system. */
    unsigned mask=1u<<index;
    if(!value) {int was_held=!!(p->held&mask);p->held&=~mask;
        if(code==312 && was_held && !p->menu_used && context!=2 && context!=8 && context!=9 && context!=10)return emit(out,SDLK_F2,0);
        if(code==306 && p->repeat_kind==2)p->repeat_kind=0;return 0;}
    if(p->held&mask) return 0;
    if(code==312)p->menu_used=!!(p->held || p->direction);
    p->held|=mask;
    p->repeat_kind=0;
    if(code==312) {p->pending=0;return 0;} /* Release tap opens history; chords remain system-owned. */
    if(p->held&(1u<<8)) {p->pending=0;return 0;}
    p->pending=0;
    switch(code) {
        case 304:return emit(out,SDLK_RETURN,0); /* A */
        case 305:return emit(out,SDLK_ESCAPE,0); /* B */
        case 307:return context==6?emit(out,SDLK_END,0):(context==0 || context==8 || context==9 || context==10)?emit(out,SDLK_F8,0):context==5?emit(out,SDLK_HOME,0):context==7?emit(out,0,'i'):context==4?emit(out,0,'i'):context==3?emit(out,SDLK_TAB,0):context==2?emit(out,SDLK_BACKSPACE,0):context==1?emit(out,0,(p->held&(1u<<10))?'x':'i'):0; /* X: inventory; R2+X: explore */
        case 306:
            if(context==1) {
                if(p->held&(1u<<10)) {emit(out,SDLK_s,0);out->key.keysym.mod=KMOD_CTRL;return 1;}
                p->repeat_kind=2;p->repeat_due=now+345;return emit(out,0,'s');
            }
            return context==4?emit(out,SDLK_TAB,0):0; /* Y: search */
        case 308:return context==9?0:emit(out,context==10?SDLK_F2:(context==0 || context==6 || context==8)?SDLK_PAGEUP:SDLK_F6,0); /* L1: previous page in modals; actions in dungeon */
        case 309:return context==9?0:emit(out,(context==0 || context==6 || context==8)?SDLK_PAGEDOWN:SDLK_F5,0); /* R1: next page in modals; interactive information in dungeon */
        case 310:return emit(out,SDLK_F3,0); /* Select: map */
        case 311:return context==4?emit(out,0,' '):context==2?emit(out,SDLK_F7,0):context==1?emit(out,0,(p->held&(1u<<10))?'Z':'.'):emit(out,SDLK_RETURN,0); /* Start: wait/rest/confirm */
        case 314:return (context==1 || context==3 || context==4 || context==5)?emit(out,SDLK_F9,0):0; /* L2: passive overlay in dungeon; disabled in modals */
        case 315:return 0; /* R2: dungeon modifier only */
        default:return 0;
    }
}
#ifdef __linux__
/* A fresh key boundary releases overlay close guards before text-only commands.
   UNKNOWN has no upstream action, unlike sending both Return and text. */
static void push(SDL_Event *event) {
    if(event->type==SDL_TEXTINPUT) {
        SDL_Event boundary;memset(&boundary,0,sizeof boundary);
        boundary.type=SDL_KEYDOWN;boundary.key.keysym.sym=SDLK_UNKNOWN;
        SDL_PushEvent(&boundary);
    }
    SDL_PushEvent(event);
}
static int fd=-1,attempted;
static BrPad physical;
static int hat_x,hat_y;
static void open_pad(void) {
    attempted=1;
    const char *enabled=getenv("BROGUE_HANDHELD");
    if(!enabled || strcmp(enabled,"1")) return;
    for(int i=0;i<16;i++) {
        char path[64],name[128]={0};snprintf(path,sizeof path,"/dev/input/event%d",i);
        int candidate=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        if(candidate<0) continue;
        if(ioctl(candidate,EVIOCGNAME(sizeof name),name)>=0 && !strcmp(name,"ANBERNIC-keys")) {
            fd=candidate;fprintf(stderr,"Brogue input: H700 ANBERNIC-keys\n");return;
        }
        close(candidate);
    }
    fprintf(stderr,"Brogue input: H700 device unavailable; keyboard retained\n");
}
#endif
void br_pad_poll(int context) {
#ifdef __linux__
    if(!attempted) open_pad();
    if(fd<0) return;
    struct input_event input;SDL_Event event;
    while(read(fd,&input,sizeof input)==sizeof input) {
        if(input.type==EV_SYN && input.code==SYN_DROPPED) {
            /* Do not infer held buttons after a dropped stream; reopen cleanly. */
            br_pad_close();attempted=1;return;
        }
        if(input.type==EV_ABS && (input.code==ABS_HAT0X || input.code==ABS_HAT0Y)) {
            int *axis=input.code==ABS_HAT0X?&hat_x:&hat_y;
            int negative=input.code==ABS_HAT0X?105:103,positive=input.code==ABS_HAT0X?106:108;
            int next=input.value<0?-1:input.value>0?1:0;
            if(next!=*axis) {
                if(*axis && br_pad_feed(&physical,*axis<0?negative:positive,0,SDL_GetTicks(),context,&event)) push(&event);
                if(next && br_pad_feed(&physical,next<0?negative:positive,1,SDL_GetTicks(),context,&event)) push(&event);
                *axis=next;
            }
        }
        if(input.type==EV_KEY && br_pad_feed(&physical,input.code,input.value,SDL_GetTicks(),context,&event)) push(&event);
    }
    if(br_pad_tick(&physical,SDL_GetTicks(),context,&event)) push(&event);
#else
    (void)context;
#endif
}
void br_pad_close(void) {
#ifdef __linux__
    if(fd>=0) close(fd);
    fd=-1;hat_x=hat_y=0;memset(&physical,0,sizeof physical);
#endif
}

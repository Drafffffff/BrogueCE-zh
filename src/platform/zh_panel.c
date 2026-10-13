/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "zh_panel.h"
#include "Rogue.h"
#include "zh_glyph.h"
#include "zh_ui.h"
#include "zh_text.h"
#include <string.h>
#include <stdio.h>
typedef struct {char label[512],name[512];char state[160];int quantity;SDL_Color state_color;int hotkey,enabled,equipped;SDL_Keycode key;unsigned glyph;SDL_Color color;} Choice;
static Choice choices[50];
static char title[160],body[20000],hint_text[256];
static int active,count,selected,offset,maximum,inventory,detail;
static char tabs[5][32],object_name[512];
static int tab_count,tab_selected,object,reading,prefer_details,read_from_inventory;
static unsigned object_glyph;
static SDL_Color object_color;
static int page_size=8,cover;
static const int cover_order[9]={0,2,1,3,4,5,6,7,8};
static int list_pages[51],list_page_count;
typedef struct {
    uint64_t category,required,forbidden;
    int mode,index,used;
    uintptr_t identity;
} PackMemory;
static PackMemory pack_memory[12];
static uintptr_t pack_rows[50];
static int pack_slot,pack_count,pack_next;
void br_inventory_clear(void) {
    memset(pack_memory,0,sizeof pack_memory);pack_count=pack_next=0;
}
void br_inventory_forget(const void *item) {
    uintptr_t identity=(uintptr_t)item;
    for(int i=0;i<12;i++)if(pack_memory[i].identity==identity)pack_memory[i].identity=0;
}
void br_inventory_restore(uint64_t category,uint64_t required,uint64_t forbidden,
                          int mode,const void *const *items,int number) {
    pack_count=number>50?50:number;pack_slot=-1;
    for(int i=0;i<12;i++) {
        PackMemory *m=&pack_memory[i];
        if(m->used && m->category==category && m->required==required && m->forbidden==forbidden && m->mode==mode) {pack_slot=i;break;}
    }
    if(pack_slot<0) {
        pack_slot=pack_next++%12;
        pack_memory[pack_slot]=(PackMemory){.category=category,.required=required,.forbidden=forbidden,.mode=mode,.used=1};
    }
    PackMemory *m=&pack_memory[pack_slot];int focus=m->index;
    if(focus>=pack_count)focus=pack_count-1;
    if(focus<0)focus=0;
    for(int i=0;i<pack_count;i++) {
        pack_rows[i]=(uintptr_t)items[i];
        if(m->identity && m->identity==pack_rows[i])focus=i;
    }
    /* A filtered action prompt starts on a usable object; plain inventory
       can inspect all objects. Neither restore path submits a command. */
    if(!(mode&1) && count && !choices[focus].enabled) {
        for(int n=1;n<=count;n++)if(choices[(focus+n)%count].enabled) {focus=(focus+n)%count;break;}
    }
    br_panel_focus(focus);
}
void br_inventory_remember(int index) {
    if(index<0 || index>=pack_count || pack_slot<0)return;
    pack_memory[pack_slot].index=index;
    pack_memory[pack_slot].identity=pack_rows[index];
}
static void copy(char *to,size_t capacity,const char *from) {
    size_t used=0;
    for(BrTextToken t;(t=br_text_next(from)).bytes;from+=t.bytes) {
        if(used+t.bytes>=capacity) break;
        memcpy(to+used,from,t.bytes);used+=t.bytes;
    }
    to[used]=0;
}
void br_panel_begin(const char *heading,const char *text) {
    copy(title,sizeof title,heading);copy(body,sizeof body,text?text:"");
    hint_text[0]=0;
    active=1;count=selected=offset=maximum=inventory=detail=0;
    tab_count=object=reading=cover=0;page_size=8;list_page_count=0;
}
void br_panel_add(const char *label,int hotkey,int enabled) {
    SDL_Keycode key=hotkey==RETURN_KEY?SDLK_RETURN:hotkey==ESCAPE_KEY?SDLK_ESCAPE:
        hotkey==UP_ARROW?SDLK_UP:hotkey==DOWN_ARROW?SDLK_DOWN:
        hotkey==LEFT_ARROW?SDLK_LEFT:hotkey==RIGHT_ARROW?SDLK_RIGHT:
        hotkey==TAB_KEY?SDLK_TAB:0;
    if(count>=50 || (!key && (hotkey<32 || hotkey>126))) return;
    memset(&choices[count],0,sizeof choices[count]);
    copy(choices[count].label,sizeof choices[count].label,label);
    choices[count].hotkey=hotkey;choices[count].key=key;choices[count++].enabled=enabled;
}
void br_panel_scroll_bottom(void) {if(active && body[0]) offset=1048576;}
int br_panel_selected(void) {return selected;}
void br_panel_focus(int index) {selected=index>=0 && index<count?index:0;}
void br_panel_hint(const char *text) {copy(hint_text,sizeof hint_text,text);}
void br_panel_tabs(const char *const *labels,int number,int current) {
    tab_count=number>5?5:number;tab_selected=current;
    for(int i=0;i<tab_count;i++)copy(tabs[i],sizeof tabs[i],labels[i]);
}
void br_panel_prefer_details(int enabled) {prefer_details=enabled;}
void br_panel_object(const char *name,unsigned glyph,SDL_Color color) {
    object=1;reading=count==0 || prefer_details;prefer_details=0;object_glyph=glyph;object_color=color;
    copy(object_name,sizeof object_name,name);
    /* itemDetails begins with its name/letter paragraph. The object frame
       already owns that heading; keep the following description untouched. */
    char *description=strstr(body,"\n\n");
    if(description)memmove(body,description+2,strlen(description+2)+1);
    for(int i=0;i<count;i++) {
        const char *verb=choices[i].hotkey=='a'?(glyph=='!'?"喝下":glyph==':'?"食用":glyph=='?'?"阅读":"使用"):
            choices[i].hotkey=='e'?"装备":choices[i].hotkey=='r'?"卸下":choices[i].hotkey=='t'?"投掷":
            choices[i].hotkey=='d'?"丢弃":choices[i].hotkey=='c'?"命名":choices[i].hotkey=='R'?"调整标记":NULL;
        if(verb)copy(choices[i].label,sizeof choices[i].label,verb);
    }
    for(int i=0;i+1<count;i++)if(choices[i].hotkey=='d' && choices[i+1].hotkey=='t') {
        Choice swap=choices[i];choices[i]=choices[i+1];choices[i+1]=swap;
    }
    /* Reading is an explicit visible choice as well as R1. It is the safe
       default for equipped/throw-only objects; all native actions stay shown. */
    if(count && count<50) {
        int first=choices[0].hotkey!='a' && choices[0].hotkey!='e';
        if(first)memmove(choices+1,choices,count*sizeof *choices);
        int index=first?0:count;
        memset(&choices[index],0,sizeof choices[index]);
        copy(choices[index].label,sizeof choices[index].label,"查看详情");choices[index].enabled=1;
        count++;
    }
    read_from_inventory=reading;
}
int br_panel_overlay(void) {return active && (tab_count || (object && !reading));}
void br_panel_cover(void) {cover=1;page_size=3;}
int br_panel_cover_active(void) {return active && cover;}
void br_panel_end(void) {active=inventory=detail=object=reading=tab_count=cover=0;}
void br_panel_inventory(void) {inventory=1;}
int br_panel_inventory_active(void) {return active && inventory;}
void br_panel_inventory_item(int index,const char *name,unsigned glyph,SDL_Color color,int equipped) {
    if(index<0 || index>=count)return;
    copy(choices[index].name,sizeof choices[index].name,name);
    choices[index].glyph=glyph;choices[index].color=color;choices[index].equipped=equipped;
}
void br_panel_inventory_state(int index,int quantity,const char *state,SDL_Color ink) {
    if(index<0 || index>=count)return;
    choices[index].quantity=quantity;choices[index].state_color=ink;
    copy(choices[index].state,sizeof choices[index].state,state);
}
int br_panel_take_detail(void) {int value=detail;detail=0;return value;}
int br_panel_active(void) {return active;}
int br_panel_event(SDL_Event *e) {
    if(!active) return 0;
    if(e->type==SDL_KEYDOWN) {
        SDL_Keycode key=e->key.keysym.sym;
        if(cover && count==9 && (key==SDLK_LEFT || key==SDLK_RIGHT || key==SDLK_UP || key==SDLK_DOWN || key==SDLK_PAGEUP || key==SDLK_PAGEDOWN)) {
            if(!e->key.repeat) {
                int at=0;while(at<8 && cover_order[at]!=selected)at++;
                int col=at/3,row=at%3;
                if(key==SDLK_LEFT || key==SDLK_PAGEUP)col=(col+2)%3;
                else if(key==SDLK_RIGHT || key==SDLK_PAGEDOWN)col=(col+1)%3;
                else row=(row+(key==SDLK_UP?2:1))%3;
                selected=cover_order[col*3+row];
            }
            Uint32 window=e->key.windowID;memset(e,0,sizeof *e);
            e->type=SDL_TEXTINPUT;e->text.windowID=window;e->text.text[0]=':';return 0;
        }
        if(inventory && (key==SDLK_PAGEUP || key==SDLK_PAGEDOWN))return 1;
        if(inventory && (key==SDLK_LEFT || key==SDLK_RIGHT || key==SDLK_UP || key==SDLK_DOWN)) {
            int step=key==SDLK_LEFT?-1:key==SDLK_RIGHT?1:key==SDLK_UP?-4:4;
            int next=selected+step;
            if(next>=0 && next<count && (step!=1 || selected%4<3) && (step!=-1 || selected%4>0))selected=next;
            return 1;
        }
        if(key==SDLK_F5 && object) {if(!e->key.repeat) {reading=!reading;read_from_inventory=0;}return 1;}
        if(key==SDLK_ESCAPE) {if(object && reading && count && !read_from_inventory) {reading=0;return 1;}return 0;}
        if(key==SDLK_HOME) {selected=offset=0;return 1;}
        if(key==SDLK_F8 && !inventory) return 1;
        if(key==SDLK_RETURN && object && reading) {if(count && !e->key.repeat) {reading=0;read_from_inventory=0;}return 1;}
        if(key==SDLK_RETURN && object && !reading && count && !choices[selected].hotkey) {
            if(!e->key.repeat) {reading=1;read_from_inventory=0;}return 1;
        }
        if(key==SDLK_RETURN || key==SDLK_F8) {
            if(!e->key.repeat && count && (choices[selected].enabled || key==SDLK_F8)) {
                detail=key==SDLK_F8;
                Uint32 window=e->key.windowID;
                memset(e,0,sizeof *e);
                if(choices[selected].key) {
                    e->type=SDL_KEYDOWN;e->key.windowID=window;e->key.state=SDL_PRESSED;
                    e->key.keysym.sym=choices[selected].key;
                } else {
                    e->type=SDL_TEXTINPUT;e->text.windowID=window;
                    e->text.text[0]=choices[selected].hotkey;
                }
                return 0;
            }
            return 1;
        }
        if(key==SDLK_UP || key==SDLK_DOWN || key==SDLK_PAGEUP || key==SDLK_PAGEDOWN) {
            int step=key==SDLK_PAGEUP?-8:key==SDLK_PAGEDOWN?8:key==SDLK_UP?-1:1;
            if(body[0] && (!object || reading)) {offset+=step;if(offset<0)offset=0;if(offset>maximum)offset=maximum;}
            else if(count) {
                if(key==SDLK_PAGEUP || key==SDLK_PAGEDOWN) {
                    int next=selected+(key==SDLK_PAGEUP?-page_size:page_size);
                    if(!tab_count && !object && list_page_count) {
                        int page=0;
                        while(page+1<list_page_count && selected>=list_pages[page+1])page++;
                        int row=selected-list_pages[page];
                        int target=page+(key==SDLK_PAGEUP?-1:1);
                        if(target<0 || target>=list_page_count)return 1;
                        next=list_pages[target]+row;
                        if(next>=list_pages[target+1])next=list_pages[target+1]-1;
                    }
                    if(next<0)next=0;
                    if(next>=count)next=count-1;
                    selected=next;
                } else {selected=(selected+step)%count;if(selected<0)selected+=count;}
            }
            if(cover) {
                /* Wake the native flame loop after navigation, without submitting
                   a command or leaking PageUp/Down into SDL window resizing. */
                Uint32 window=e->key.windowID;
                memset(e,0,sizeof *e);e->type=SDL_TEXTINPUT;
                e->text.windowID=window;e->text.text[0]=':';return 0;
            }
            return 1;
        }
        if(key==SDLK_LEFT || key==SDLK_RIGHT) {
            if(body[0] && !object && count) selected=(selected+count+(key==SDLK_LEFT?-1:1))%count;
            return 1;
        }
        return 0; /* Direct letter hotkeys retain upstream semantics. */
    }
    return e->type==SDL_MOUSEMOTION || e->type==SDL_MOUSEBUTTONDOWN || e->type==SDL_MOUSEBUTTONUP || e->type==SDL_MOUSEWHEEL;
}
static void line(SDL_Renderer *r,const char *value,int x,int y,SDL_Color color,int limit) {
    int columns=0;
    for(BrTextToken t;(t=br_text_next(value)).bytes;value+=t.bytes) {
        if(t.is_color) {
            const unsigned char *c=(const unsigned char*)value;
            color=(SDL_Color){(c[1]-25)*255/100,(c[2]-25)*255/100,(c[3]-25)*255/100,255};
            continue;
        }
        if(t.codepoint=='\n' || columns+t.columns>limit) break;
        br_draw_glyph(r,t.codepoint,(SDL_Rect){x,y,t.columns*10,20},color);
        x+=t.columns*10;columns+=t.columns;
    }
}
static void item_label(SDL_Renderer *r,const char *value,int y,SDL_Color color,int limit) {
    char wrapped[1200];int total=br_text_wrap(wrapped,sizeof wrapped,value,limit);
    if(total<0) {line(r,"名称过长，请打开详情",40,y,color,limit);return;}
    int row=0,x=40,columns=0;
    for(const char *p=wrapped;*p && row<2;) {
        BrTextToken t=br_text_next(p);
        if(t.is_color) {const unsigned char *c=(const unsigned char*)p;color=(SDL_Color){(c[1]-25)*255/100,(c[2]-25)*255/100,(c[3]-25)*255/100,255};}
        else if(t.codepoint=='\n') {row++;x=40;columns=0;}
        else {
            if(columns+t.columns<=limit-(row==1 && total>2?2:0)) {
                br_draw_glyph(r,t.codepoint,(SDL_Rect){x,y+row*20,t.columns*10,20},color);x+=t.columns*10;
            }
            columns+=t.columns;
        }
        p+=t.bytes;
    }
    if(total>2) br_draw_glyph(r,0x2026,(SDL_Rect){40+(limit-2)*10,y+20,20,20},color);
}
static void draw_tabs(SDL_Renderer *r,int width,int height) {
    int left=(width-612)/2,top=(height-326)/2+10;
    if(left<16)left=16;
    int right=width-left;
    br_ui_fill(r,(SDL_Rect){left,top,right-left,326},BR_UI_BOX);
    line(r,title,left+22,top+12,BR_UI_FOCUS,(right-left-44)/10);
    for(int i=0;i<tab_count;i++) {
        char label[40];snprintf(label,sizeof label,i==tab_selected?"[%s]":"%s",tabs[i]);
        line(r,label,left+22+i*102,top+38,i==tab_selected?BR_UI_FOCUS:BR_UI_MUTED,10);
    }
    for(int x=left+22;x+10<right-22;x+=10)line(r,"-",x,top+64,BR_UI_RULE,1);
    page_size=8;int first=selected/page_size*page_size;
    for(int i=first;i<count && i<first+page_size;i++) {
        int y=top+84+(i-first)*26;
        if(i==selected) {
            br_ui_fill(r,(SDL_Rect){left+19,y,right-left-46,26},BR_UI_SELECTED);
            line(r,">",left+26,y,BR_UI_FOCUS,1);
        }
        line(r,choices[i].label,left+50,y,choices[i].enabled?(i==selected?BR_UI_INK:BR_UI_MUTED):BR_UI_MUTED,(right-left-78)/10);
    }
    char page[40];snprintf(page,sizeof page,"%d/%d",selected/page_size+1,(count+page_size-1)/page_size);
    line(r,hint_text,left+22,top+300,BR_UI_MUTED,46);
    line(r,page,right-68,top+300,BR_UI_MUTED,5);
    br_ui_fill(r,(SDL_Rect){0,height-28,width,28},(SDL_Color){0,0,0,255});
    line(r,"L2/R2 切签  L1/R1 翻页  X 回常用",18,height-24,BR_UI_MUTED,(width-36)/10);
}
static void draw_object(SDL_Renderer *r,int width,int height) {
    int left=(width-612)/2,top=(height-332)/2,right=width-left;
    if(left<16)left=16;
    br_ui_fill(r,(SDL_Rect){left,top,right-left,332},BR_UI_BOX);
    line(r,"物品",left+22,top+12,BR_UI_FOCUS,4);
    br_draw_glyph(r,object_glyph,(SDL_Rect){left+22,top+46,20,20},object_color);
    line(r,object_name,left+60,top+46,object_color,(right-left-82)/10);
    for(int x=left+22;x+10<right-22;x+=10)line(r,"-",x,top+82,BR_UI_RULE,1);
    /* Full original text is always one R1 away. A short synopsis may wrap;
       every applicable native action remains visible beside it. */
    char wrapped[40000];
    if(br_text_wrap(wrapped,sizeof wrapped,strstr(body,"\n\n")?strstr(body,"\n\n")+2:body,(right-left-276)/10)>=0) {
        int row=0,x=left+22,y=top+114;SDL_Color color=BR_UI_INK;
        for(const char *p=wrapped;*p && row<7;) {
            BrTextToken t=br_text_next(p);
            if(t.is_color) {const unsigned char *c=(const unsigned char*)p;color=(SDL_Color){(c[1]-25)*255/100,(c[2]-25)*255/100,(c[3]-25)*255/100,255};}
            else if(t.codepoint=='\n') {row++;x=left+22;y+=22;}
            else {br_draw_glyph(r,t.codepoint,(SDL_Rect){x,y,t.columns*10,20},color);x+=t.columns*10;}
            p+=t.bytes;
        }
    }
    int action_x=right-226;
    for(int i=0;i<count;i++) {
        int y=top+110+i*26;
        if(i==selected) {br_ui_fill(r,(SDL_Rect){action_x-8,y,202,26},BR_UI_SELECTED);line(r,">",action_x,y,BR_UI_FOCUS,1);}
        line(r,choices[i].label,action_x+26,y,choices[i].enabled?BR_UI_INK:BR_UI_MUTED,16);
    }
    line(r,"上下选动作  A 执行  B 返回",left+22,top+304,BR_UI_MUTED,(right-left-44)/10);
}
/* A fixed 26-slot field leaves the whole pack visible. Original itemName
   supplies public names; the selected native label retains all known stats. */
static void grid_text(SDL_Renderer *r,const char *value,int x,int y,int columns,int rows,SDL_Color ink) {
    char wrapped[1800];int total=br_text_wrap(wrapped,sizeof wrapped,value,columns);
    if(total<0) {line(r,"名称过长",x,y,ink,columns);return;}
    int row=0,left=x,used=0,first_y=y;
    for(const char *p=wrapped;*p && row<rows;) {
        BrTextToken t=br_text_next(p);
        if(t.is_color) {const unsigned char *c=(const unsigned char*)p;ink=(SDL_Color){(c[1]-25)*255/100,(c[2]-25)*255/100,(c[3]-25)*255/100,255};}
        else if(t.codepoint=='\n') {row++;x=left;used=0;y+=20;}
        else {
            int limit=columns-(row==rows-1 && total>rows?2:0);
            if(used+t.columns<=limit)br_draw_glyph(r,t.codepoint,(SDL_Rect){x,y,t.columns*10,20},ink);
            x+=t.columns*10;used+=t.columns;
        }
        p+=t.bytes;
    }
    if(total>rows)br_draw_glyph(r,0x2026,(SDL_Rect){left+(columns-2)*10,first_y+(rows-1)*20,20,20},ink);
}
static void draw_inventory(SDL_Renderer *r,int width,int height) {
    br_ui_frame(r,width,height,title,1);
    int left=18,cell=(width-36)/4,top=52,step=(height-172)/7;

    for(int i=0;i<MAX_PACK_ITEMS;i++) {
        int x=left+(i%4)*cell,y=top+(i/4)*step;
        if(i==selected && i<count) {
            br_ui_fill(r,(SDL_Rect){x-2,y-1,cell-6,step-2},BR_UI_SELECTED);
            line(r,">",x,y,BR_UI_FOCUS,1);
        }
        if(i>=count) {line(r,".",x+8,y+12,BR_UI_RULE,1);continue;}
        Choice *choice=&choices[i];SDL_Color ink=choice->enabled?(choice->glyph=='!'?choice->color:BR_UI_INK):BR_UI_MUTED;
        if(!choice->glyph)ink=choice->enabled?BR_UI_INK:BR_UI_MUTED;
        br_draw_glyph(r,choice->glyph?choice->glyph:'*',(SDL_Rect){x+12,y,20,20},choice->enabled?choice->color:BR_UI_MUTED);
        grid_text(r,choice->name[0]?choice->name:choice->label,x+36,y,(cell-40)/10,1,ink);
        int reserve=choice->quantity>1?40:0;
        grid_text(r,choice->state,x+36,y+20,(cell-40-reserve)/10,1,choice->enabled?choice->state_color:BR_UI_MUTED);
        if(choice->quantity>1) {
            char amount[32];snprintf(amount,sizeof amount,"×%d",choice->quantity);
            int columns=(int)strlen(amount);
            line(r,amount,x+cell-8-columns*10,y+20,choice->enabled?BR_UI_INK:BR_UI_MUTED,columns);
        }
    }
    int footer=height-108;
    SDL_SetRenderDrawColor(r,BR_UI_RULE.r,BR_UI_RULE.g,BR_UI_RULE.b,255);
    for(int x=20;x<width-20;x+=10)SDL_RenderDrawLine(r,x,footer-10,x+4,footer-10);
    if(count)grid_text(r,choices[selected].label,20,footer,(width-40)/10,3,BR_UI_INK);
    line(r,"方向选物品  A 操作  X 详情  B 返回",20,height-30,BR_UI_MUTED,(width-40)/10);
}
void br_panel_draw(SDL_Renderer *r,int width,int height) {
    if(!active) return;
    if(cover) {
        /* A quiet threshold beneath the original live flame/logo. All nine
           entrances remain visible; this pass never clears the title art. */
        int top=height-138,margin=36,cell=(width-margin*2)/3;
        SDL_BlendMode prior;SDL_GetRenderDrawBlendMode(r,&prior);
        SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
        for(int y=top;y<height;y++) {
            int alpha=y<top+20?(y-top)*220/20:y>=height-24?255:220;
            br_ui_fill(r,(SDL_Rect){0,y,width,1},(SDL_Color){10,8,17,(Uint8)alpha});
        }
        const char *groups[3]={"冒险","往事","设定"};
        for(int col=0;col<3;col++) {
            int x=margin+col*cell;
            /* Section labels use the existing font at the floating-layer size. */
            const char *label=groups[col];int used=0;
            for(BrTextToken t;(t=br_text_next(label)).bytes;label+=t.bytes) {
                br_draw_glyph(r,t.codepoint,(SDL_Rect){x+used*6,top+4,t.columns*6,12},(SDL_Color){169,146,110,255});used+=t.columns;
            }
            SDL_SetRenderDrawColor(r,86,67,51,255);SDL_RenderDrawLine(r,x+36,top+10,x+cell-32,top+10);
            for(int row=0;row<3;row++) {
                int i=cover_order[col*3+row];if(i>=count)continue;
                int y=top+26+row*26;
                if(i==selected) {
                    for(int dx=0;dx<cell-26;dx++)
                        br_ui_fill(r,(SDL_Rect){x-10+dx,y-2,1,24},(SDL_Color){116,85,46,(Uint8)(82*(cell-26-dx)/(cell-26))});
                    line(r,">",x-14,y,BR_UI_FOCUS,1);
                }
                line(r,choices[i].label,x+6,y,i==selected?BR_UI_FOCUS:(SDL_Color){202,194,183,255},(cell-24)/10);
            }
        }
        SDL_SetRenderDrawBlendMode(r,prior);
        const char *meta=strstr(title,"·");meta=meta?meta+strlen("·"):title;
        while(*meta==' ')meta++;
        const char *texts[2]={"方向选择   A 确认",meta};
        for(int n=0;n<2;n++) {
            int x=n?width-margin-br_text_width(texts[n])*6:margin,used=0;
            for(const char *p=texts[n];*p;) {
                BrTextToken t=br_text_next(p);p+=t.bytes;
                br_draw_glyph(r,t.codepoint,(SDL_Rect){x+used*6,height-19,t.columns*6,12},(SDL_Color){146,133,123,255});used+=t.columns;
            }
        }
        return;
    }
    if(inventory) {draw_inventory(r,width,height);return;}
    if(tab_count) {draw_tabs(r,width,height);return;}
    if(object && !reading) {draw_object(r,width,height);return;}
    br_ui_frame(r,width,height,object?object_name:title,1);
    if(body[0]) {
        char wrapped[40000];int total=br_text_wrap(wrapped,sizeof wrapped,body,(width-40)/10);
        if(total<0) return;
        int visible=(height-164)/26;maximum=total>visible?total-visible:0;if(offset>maximum)offset=maximum;
        int row=0,x=20,y=56;SDL_Color color=BR_UI_INK;
        for(const char *p=wrapped;*p;) {
            BrTextToken t=br_text_next(p);
            if(t.is_color) {const unsigned char *c=(const unsigned char*)p;color=(SDL_Color){(c[1]-25)*255/100,(c[2]-25)*255/100,(c[3]-25)*255/100,255};}
            else if(t.codepoint=='\n') {row++;x=20;if(row>offset)y+=26;}
            else if(row>=offset && row<offset+visible) {br_draw_glyph(r,t.codepoint,(SDL_Rect){x,y,t.columns*10,20},color);x+=t.columns*10;}
            p+=t.bytes;
        }
        if(count && !object) {
            char caption[600];snprintf(caption,sizeof caption,"操作 %d/%d  < %s >",selected+1,count,choices[selected].label);
            line(r,caption,20,height-100,BR_UI_FOCUS,(width-40)/10);
        }
        line(r,object?(read_from_inventory?"上下阅读  L1/R1翻页  A 操作  B 返回背包":"上下阅读  L1/R1翻页  A 操作  B 返回操作"):"上下阅读 左右选操作 [A]确认 [B]返回 L1/R1翻页",20,height-32,BR_UI_MUTED,(width-40)/10);
    } else {
        /* Physical controls need readable type, not touch-sized empty rows.
           Partition by rendered height so every wrapped name stays together. */
        int row_height[50],used=0;
        list_page_count=0;list_pages[list_page_count++]=0;
        for(int i=0;i<count;i++) {
            char wrapped[1200];
            int lines=br_text_wrap(wrapped,sizeof wrapped,choices[i].label,(width-64)/10);
            row_height[i]=lines>1?44:26;
            if(used && used+row_height[i]>height-100) {
                list_pages[list_page_count++]=i;used=0;
            }
            used+=row_height[i];
        }
        list_pages[list_page_count]=count;
        int page=0;while(page+1<list_page_count && selected>=list_pages[page+1])page++;
        int first=list_pages[page],last=list_pages[page+1],y=56;
        page_size=last-first;
        for(int i=first;i<last;i++) {
            if(i==selected) {
                br_ui_fill(r,(SDL_Rect){17,y-2,width-34,row_height[i]},BR_UI_SELECTED);
                line(r,">",20,y,BR_UI_FOCUS,1);
            }
            item_label(r,choices[i].label,y,choices[i].enabled?(i==selected?BR_UI_FOCUS:BR_UI_INK):BR_UI_MUTED,(width-64)/10);
            y+=row_height[i];
        }
        char hint[256];snprintf(hint,sizeof hint,"%d/%d 上下选择 [A]确认 %s[B]返回 L1/R1翻页",count?selected+1:0,count,inventory?"[X]详情 ":"");
        line(r,hint_text[0]?hint_text:hint,20,height-32,BR_UI_MUTED,(width-40)/10);
    }
}

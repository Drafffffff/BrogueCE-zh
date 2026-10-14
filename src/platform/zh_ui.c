/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "zh_ui.h"
#include "zh_glyph.h"
#include "zh_text.h"
void br_ui_fill(SDL_Renderer *r,SDL_Rect box,SDL_Color color) {
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);SDL_RenderFillRect(r,&box);
}
void br_ui_text(SDL_Renderer *r,const char *value,int x,int y,SDL_Color color,int limit) {
    int used=0;
    for(BrTextToken t;(t=br_text_next(value)).bytes;value+=t.bytes) {
        if(t.is_color) {
            const unsigned char *c=(const unsigned char*)value;
            color=(SDL_Color){(c[1]-25)*255/100,(c[2]-25)*255/100,(c[3]-25)*255/100,255};continue;
        }
        if(t.codepoint=='\n' || used+t.columns>limit)break;
        br_draw_glyph(r,t.codepoint,(SDL_Rect){x,y,t.columns*10,20},color);
        x+=t.columns*10;used+=t.columns;
    }
}
void br_ui_frame(SDL_Renderer *r,int width,int height,const char *title,int footer_rows) {
    SDL_SetRenderDrawColor(r,BR_UI_BOX.r,BR_UI_BOX.g,BR_UI_BOX.b,255);SDL_RenderClear(r);
    int columns=(width-40)/10,bottom=height-(footer_rows>1?80:64);
    if(columns<8 || bottom<56)return;
    br_ui_text(r,title,20,16,BR_UI_FOCUS,columns);
    int used=br_text_width(title);if(used>columns)used=columns;
    int start=20+(used+2)*10;
    /* Hairlines are drawn, not spelled with hyphens: the typeface hyphen sits
       on a baseline of its own and breaks into a ragged dotted run. */
    br_ui_rule(r,start,25,width-20);
    if(footer_rows>=0)br_ui_rule(r,20,bottom+9,width-20);
}
void br_ui_rule(SDL_Renderer *r,int x,int y,int right) {
    if(right<=x)return;
    br_ui_fill(r,(SDL_Rect){x,y,right-x,1},BR_UI_RULE);
}

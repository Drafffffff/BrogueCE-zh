/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "zh_glyph.h"
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fixed-size cache bounds memory for long encyclopaedia/battle-log sessions. */
typedef struct {
    unsigned int cp;
    SDL_Texture *texture;
    SDL_Rect ink;
    int min_x, max_y, advance;
} Glyph;
static Glyph cache[256];
static unsigned int cursor;
static TTF_Font *font;
static SDL_Renderer *owner;
static int pixels;
extern char dataDirectory[];

static void fail(void) {
    fprintf(stderr, "Chinese glyph rendering failed: %s / %s\n", TTF_GetError(), SDL_GetError());
    exit(1);
}

void br_draw_glyph(SDL_Renderer *renderer, unsigned int cp, SDL_Rect area, SDL_Color color) {
    int target = area.h;
    if (owner != renderer || pixels != target) {
        for (int i = 0; i < 256; i++) {
            if (cache[i].texture) SDL_DestroyTexture(cache[i].texture);
            cache[i] = (Glyph){0};
        }
        if (font) TTF_CloseFont(font);
        if (!TTF_WasInit() && TTF_Init()) fail();
        char path[2048];
        const char *override = getenv("BROGUE_CJK_FONT");
        if (override) snprintf(path, sizeof(path), "%s", override);
        else snprintf(path, sizeof(path), "%s/assets/NotoSansCJKsc-Regular.otf", dataDirectory);
        font = TTF_OpenFont(path, target);
        if (!font) fail();
        owner = renderer; pixels = target;
    }
    Glyph *glyph = NULL;
    for (int i = 0; i < 256; i++) if (cache[i].texture && cache[i].cp == cp) { glyph = &cache[i]; break; }
    if (!glyph) {
        glyph = &cache[cursor++ % 256];
        if (glyph->texture) SDL_DestroyTexture(glyph->texture);
        /* UTF-8 works with SDL_ttf releases predating the Uint32 glyph API. */
        char encoded[5] = {0};
        if (cp < 0x80) encoded[0] = cp;
        else if (cp < 0x800) { encoded[0] = 0xc0 | (cp >> 6); encoded[1] = 0x80 | (cp & 63); }
        else if (cp < 0x10000) { encoded[0] = 0xe0 | (cp >> 12); encoded[1] = 0x80 | ((cp >> 6) & 63); encoded[2] = 0x80 | (cp & 63); }
        else { encoded[0] = 0xf0 | (cp >> 18); encoded[1] = 0x80 | ((cp >> 12) & 63); encoded[2] = 0x80 | ((cp >> 6) & 63); encoded[3] = 0x80 | (cp & 63); }
        SDL_Surface *surface = TTF_RenderUTF8_Blended(font, encoded, (SDL_Color){255,255,255,255});
        if (!surface) fail();
        int min_x, max_x, min_y, max_y, advance;
        if (TTF_GlyphMetrics32(font, cp, &min_x, &max_x, &min_y, &max_y, &advance)) fail();
        int left=surface->w, top=surface->h, right=-1, bottom=-1;
        if (SDL_LockSurface(surface)) fail();
        for(int y=0;y<surface->h;y++) for(int x=0;x<surface->w;x++) {
            Uint32 pixel=0; Uint8 red,green,blue,alpha;
            memcpy(&pixel,(char*)surface->pixels+y*surface->pitch+x*surface->format->BytesPerPixel,surface->format->BytesPerPixel);
            SDL_GetRGBA(pixel,surface->format,&red,&green,&blue,&alpha);
            if(alpha) {if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;}
        }
        SDL_UnlockSurface(surface);
        *glyph = (Glyph){cp, SDL_CreateTextureFromSurface(renderer, surface),
            right<left?(SDL_Rect){0,0,0,0}:(SDL_Rect){left,top,right-left+1,bottom-top+1},
            min_x, max_y, advance};
        SDL_FreeSurface(surface);
        if (!glyph->texture) fail();
    }
    SDL_SetTextureColorMod(glyph->texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(glyph->texture, color.a);
    /* Remove line leading, but retain bearings and a shared baseline. Centering
       each ink rectangle would lift commas/full stops into the middle of a line. */
    if(!glyph->ink.w || !glyph->ink.h) return;
    double scale = 1;
    if (glyph->advance > area.w) scale = (double)area.w / glyph->advance;
    if (glyph->ink.w * scale > area.w) scale = (double)area.w / glyph->ink.w;
    if (glyph->ink.h*scale > area.h) scale = (double)area.h / glyph->ink.h;
    SDL_Rect dest = {area.x, area.y, glyph->ink.w * scale, glyph->ink.h * scale};
    dest.x += (area.w - glyph->advance * scale) / 2 + glyph->min_x * scale;
    dest.y += area.h * 9 / 10 - glyph->max_y * scale;
    if (dest.x < area.x) dest.x = area.x;
    if (dest.x + dest.w > area.x + area.w) dest.x = area.x + area.w - dest.w;
    if (dest.y < area.y) dest.y = area.y;
    if (dest.y + dest.h > area.y + area.h) dest.y = area.y + area.h - dest.h;
    if (SDL_RenderCopy(renderer, glyph->texture, &glyph->ink, &dest)) fail();
}

/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "zh_text.h"
#include <string.h>
#include <limits.h>
size_t br_text_copy(char *out,size_t capacity,const char *text) {
    size_t used=0;
    if(!capacity) return 0;
    for(BrTextToken token;(token=br_text_next(text)).bytes;text+=token.bytes) {
        if(token.bytes>=capacity-used) break;
        memcpy(out+used,text,token.bytes);used+=token.bytes;
    }
    out[used]=0;return used;
}

int br_text_columns(uint32_t cp) {
    return cp == '\n' ? 0 : (cp >= 0x1100 &&
        (cp <= 0x115f || (cp >= 0x2e80 && cp <= 0xa4cf) ||
         (cp >= 0xac00 && cp <= 0xd7a3) || (cp >= 0xf900 && cp <= 0xfaff) ||
         (cp >= 0xfe10 && cp <= 0xfe6f) || (cp >= 0xff01 && cp <= 0xff60) ||
         (cp >= 0xffe0 && cp <= 0xffe6) || cp >= 0x20000) ? 2 : 1);
}

/* Brogue's byte 25 introduces THREE opaque color bytes, not UTF-8. */
BrTextToken br_text_next(const char *text) {
    const unsigned char *s = (const unsigned char *)text;
    BrTextToken token = {0, 0, 0, 0};
    if (!s || !s[0]) return token;
    if (s[0] == 25 && s[1] && s[2] && s[3]) {
        token.bytes = 4; token.is_color = 1; return token;
    }
    uint32_t cp = s[0];
    size_t n = 1;
    if (cp >= 0xc2 && cp <= 0xdf) { cp &= 0x1f; n = 2; }
    else if (cp >= 0xe0 && cp <= 0xef) { cp &= 0x0f; n = 3; }
    else if (cp >= 0xf0 && cp <= 0xf4) { cp &= 7; n = 4; }
    else if (cp >= 0x80 || cp == 25) { cp = 0xfffd; }
    for (size_t i = 1; i < n; i++) {
        if (!s[i] || (s[i] & 0xc0) != 0x80) { cp = 0xfffd; n = 1; break; }
        cp = (cp << 6) | (s[i] & 0x3f);
    }
    if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) ||
        (n == 4 && cp < 0x10000) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
        cp = 0xfffd; n = 1;
    }
    token.codepoint = cp; token.bytes = n;
    /* CJK text occupies two terminal columns. ASCII/newlines retain upstream layout. */
    token.columns = br_text_columns(cp);
    return token;
}

int br_text_width(const char *text) {
    int width = 0;
    for (BrTextToken t; (t = br_text_next(text)).bytes; text += t.bytes) {
        int advance = !t.is_color && t.codepoint == '\n' ? 1 : t.columns;
        if (width > INT_MAX - advance) return INT_MAX;
        width += advance;
    }
    return width;
}

int br_text_has_unicode(const char *text) {
    for (BrTextToken t; (t = br_text_next(text)).bytes; text += t.bytes)
        if (!t.is_color && t.codepoint > 127) return 1;
    return 0;
}

static int opening_punctuation(uint32_t cp) {
    return cp == 0xff08 || cp == 0x300a || cp == 0x3010 || cp == 0x300c ||
        cp == 0x300e || cp == 0x3008 || cp == 0x3014 || cp == 0xff3b || cp == 0xff5b;
}

static int closing_punctuation(uint32_t cp) {
    return cp == 0xff0c || cp == 0x3002 || cp == 0x3001 || cp == 0xff01 ||
        cp == 0xff1f || cp == 0xff1a || cp == 0xff1b || cp == 0xff09 ||
        cp == 0x300b || cp == 0x3011 || cp == 0x300d || cp == 0x300f ||
        cp == 0x3009 || cp == 0x3015 || cp == 0xff3d || cp == 0xff5d;
}

/* Keep a character and its closing punctuation together. Opening brackets also
   keep the next character. Color bytes have no bearing on break positions.
   Groups wider than a whole line fall back to character wrapping. */
static int unbroken_width(const char *text, BrTextToken first, int width) {
    if (!first.columns || closing_punctuation(first.codepoint)) return first.columns;
    int columns = first.columns, need_character = opening_punctuation(first.codepoint);
    for (const char *p = text + first.bytes; ; ) {
        BrTextToken next = br_text_next(p);
        if (!next.bytes) break;
        p += next.bytes;
        if (next.is_color) continue;
        if (next.codepoint == '\n' || next.codepoint == ' ') break;
        if (!need_character && !closing_punctuation(next.codepoint)) break;
        columns += next.columns;
        if (columns > width) return first.columns;
        need_character = opening_punctuation(next.codepoint);
    }
    return columns;
}

/* Bounded character wrapping. ASCII-only game callers keep the upstream wrapper. */
int br_text_wrap(char *out, size_t capacity, const char *text, int width) {
    if (width < 1 || (out && !capacity)) return -1;
    size_t used = 0;
    size_t last_space = (size_t)-1;
    int after_space = 0;
    int column = 0, lines = 1;
    if (out) out[0] = 0;
    for (BrTextToken t; (t = br_text_next(text)).bytes; text += t.bytes) {
        if (t.columns > width) return -1;
        int required = unbroken_width(text, t, width);
        if (t.columns && column + required > width) {
            if (last_space != (size_t)-1) {
                if (out) out[last_space] = '\n';
                column -= after_space; lines++;
                last_space = (size_t)-1;
            }
        }
        if (t.columns && column + required > width) {
            if (out) {
                if (used + 1 >= capacity) return -1;
                out[used] = '\n'; out[used + 1] = 0;
            }
            used++; column = 0; lines++; last_space = (size_t)-1;
        }
        if (!t.is_color && t.codepoint == ' ') {
            last_space = used; after_space = column + 1;
        }
        if (out) {
            if (t.bytes >= capacity - used) return -1;
            memcpy(out + used, text, t.bytes); out[used + t.bytes] = 0;
        }
        used += t.bytes;
        if (!t.is_color && t.codepoint == '\n') { column = 0; lines++; last_space = (size_t)-1; }
        else column += t.columns;
    }
    return lines;
}

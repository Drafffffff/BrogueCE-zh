/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef BROGUE_ZH_TEXT_H
#define BROGUE_ZH_TEXT_H
#include <stddef.h>
#include <stdint.h>
#ifndef BR_MESSAGE_BYTES
#define BR_MESSAGE_BYTES 1024
#endif
#define BR_UNICODE_FLAG 0x200000
typedef struct {
    uint32_t codepoint;
    size_t bytes;
    int columns;
    int is_color;
} BrTextToken;
BrTextToken br_text_next(const char *text);
int br_text_columns(uint32_t cp);
int br_text_width(const char *text);
int br_text_has_unicode(const char *text);
/* Copies complete UTF-8/color tokens, leaving space for the terminator. */
size_t br_text_copy(char *out,size_t capacity,const char *text);
int br_text_wrap(char *out, size_t capacity, const char *text, int width);
#endif

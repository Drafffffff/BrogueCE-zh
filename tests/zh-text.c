/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "zh_text.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    char copied[12];memset(copied,'!',sizeof copied);
    assert(br_text_copy(NULL,0,"中文")==0);
    assert(br_text_copy(copied,1,"中文")==0 && !copied[0] && copied[1]=='!');
    assert(br_text_copy(copied,6,"中文")==3 && !strcmp(copied,"中"));
    assert(br_text_copy(copied,7,"中文")==6 && !strcmp(copied,"中文"));
    assert(br_text_copy(copied,4,"\x19\xc8\xe0\xf0" "A")==0 && !copied[0]);
    assert(br_text_copy(copied,5,"\x19\xc8\xe0\xf0" "A")==4 && !strcmp(copied,"\x19\xc8\xe0\xf0"));
    assert(br_text_copy(copied,sizeof copied,"\xf0\xa0\x80\x80中文")==10);
    assert(br_text_width("背包 Inventory") == 14);
    assert(br_text_width("a\nb") == 3);
    assert(br_text_width("\x19\xc8\xe0\xf0中文") == 4);
    assert(!br_text_has_unicode("\x19\xc8\xe0\xf0" "ASCII"));
    assert(br_text_next("\xf0\xa0\x80\x80").codepoint == 0x20000);
    assert(br_text_next("\xed\xa0\x80").codepoint == 0xfffd);
    assert(br_text_next("\xf4\x90\x80\x80").codepoint == 0xfffd);
    assert(br_text_next("\xe4\xb8").bytes == 1);
    assert(br_text_next("\x19").bytes == 1);
    char out[128];
    assert(br_text_wrap(out, sizeof out, "中文测试", 4) == 2);
    assert(!strcmp(out, "中文\n测试"));
    assert(br_text_wrap(out, sizeof out, "中文\x19\xc8\xe0\xf0测试", 4) == 2);
    assert(!strcmp(out, "中文\x19\xc8\xe0\xf0\n测试"));
    assert(br_text_wrap(out, 7, "中文测试", 4) == -1);
    assert(out[6] == 0);
    assert(br_text_wrap(NULL, 0, "中文测试", 4) == 2);
    assert(br_text_wrap(out, sizeof out, "long word next", 10) == 2);
    assert(!strcmp(out,"long word\nnext"));
    assert(br_text_wrap(NULL, 0, "long word next", 10) == 2);
    assert(br_text_wrap(out, sizeof out, "中文，", 4) == 2);
    assert(!strcmp(out,"中\n文，"));
    assert(br_text_wrap(out, sizeof out, "中（文", 4) == 2);
    assert(!strcmp(out,"中\n（文"));
    assert(br_text_wrap(out, sizeof out, "中文。】", 6) == 2);
    assert(!strcmp(out,"中\n文。】"));
    assert(br_text_wrap(out, sizeof out, "中文\x19\xc8\xe0\xf0，", 4) == 2);
    assert(!strcmp(out,"中\n文\x19\xc8\xe0\xf0，"));
    assert(br_text_wrap(NULL, 0, "中文\x19\xc8\xe0\xf0，", 4) == 2);
    assert(br_text_wrap(out, sizeof out, "中文，", 2) == 3);
    assert(!strcmp(out,"中\n文\n，")); /* A pair cannot fit in a one-character line. */
    assert(br_text_wrap(out, sizeof out, "中\n（文）", 6) == 2);
    assert(!strcmp(out,"中\n（文）"));
    assert(br_text_wrap(out, sizeof out, "中文《物品》", 8) == 2);
    assert(!strcmp(out,"中文《物\n品》"));
    assert(br_text_wrap(out, 7, "中文，", 4) == -1);
    assert(!strcmp(out,"中\n"));
    puts("UTF-8, opaque colors, bounded wrapping and invalid sequences passed");
}

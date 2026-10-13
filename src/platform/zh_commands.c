/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "zh_commands.h"
#include "zh_panel.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* These are display choices only; original executeKeystroke owns all rules,
   inventory filters, turn recording, confirmation and save behavior. */
typedef struct {const char *label;char key;char trigger;int control;} Command;
static const Command commands[]={
    {"背包与物品详情",'i',0,0},
    {"等待一回合",'.',0,0},
    {"搜索周围",'s',0,0},
    {"自动探索",'x',0,0},
    {"休息恢复",'Z',0,0},
    {"下楼 / 前往下层楼梯",'>',0,0},
    {"上楼 / 前往上层楼梯",'<',0,0},
    {"使用物品",'a',0,0},
    {"装备物品",'e',0,0},
    {"卸下装备",'r',0,0},
    {"投掷物品",'t',0,0},
    {"再次投掷上一件物品",'T',0,0},
    {"切换上一套装备",'w',0,0},
    {"丢弃物品",'d',0,0},
    {"命名物品 / 种类",'c',0,0},
    {"调整物品标记",'R',0,0},
    {"已发现的物品种类",'D',0,0},
    {"本局成就",'F',0,0},
    {"掌机操作帮助",'?',0,0},
    {"完整消息记录",'M',0,0},
    {"显示 / 隐藏潜行范围",']',0,0},
    {"切换色彩效果",'\\',0,0},
    {"切换图形模式",'G',0,0},
    {"查看本局种子",'~',0,0},
    {"保存并退出",'S',0,0},
    {"放弃本局（原版会确认）",'Q',0,0},
    {"新开一局（前 50 回合直接结束本局）",'N',0,0},
    {"开启休闲模式（不可撤销）",'&',0,0},
    {"开启自动游玩（原版会确认）",'A',0,0},
    {"连续搜索",'s','1',1},
    {"向上奔跑",'k','2',1},
    {"向下奔跑",'j','3',1},
    {"向左奔跑",'h','4',1},
    {"向右奔跑",'l','5',1},
    {"向左上奔跑",'y','6',1},
    {"向右上奔跑",'u','7',1},
    {"向左下奔跑",'b','8',1},
    {"向右下奔跑",'n','9',1},
    {"快速自动探索",'x','0',1},
    {"快速自动游玩（原版会确认）",'A','!',1},
    {"巫师：生成物品 / 怪物",'C',0,0},
    {"保存截图",',',0,0},
};
static const Command playback_commands[]={
    {"暂停 / 继续播放",' ',0,0},
    {"前进一回合",'l',0,0},
    {"后退一回合",'h',0,0},
    {"加快播放",'k',0,0},
    {"减慢播放",'j',0,0},
    {"跳转到回合（输入数字）",'0',0,0},
    {"上一层",'<',0,0},
    {"下一层",'>',0,0},
    {"检查周围",'\n',0,0},
    {"查看背包",'i',0,0},
    {"已发现的物品种类",'D',0,0},
    {"本局成就",'F',0,0},
    {"完整消息记录",'M',0,0},
    {"开启 / 关闭全知视角",'\t',0,0},
    {"掌机录像帮助",'?',0,0},
    {"查看本局种子",'~',0,0},
    {"显示 / 隐藏潜行范围",']',0,0},
    {"切换色彩效果",'\\',0,0},
    {"切换图形模式",'G',0,0},
    {"打开其他录像",'V',0,0},
    {"打开存档继续游玩",'O',0,0},
    {"关闭录像并开始新游戏",'N',0,0},
    {"退出录像回到标题",'Q',0,0},
    {"前进 5 回合",'l','1',2},
    {"后退 5 回合",'h','2',2},
    {"前进 20 回合",'l','3',1},
    {"后退 20 回合",'h','4',1},
    {"前进 100 回合",'l','5',3},
    {"后退 100 回合",'h','6',3},
};

/* Five stable tabs share the same page grammar as inventory and readers. */
typedef struct {int command,child;const char *label;} Entry;
typedef struct {const char *title;int parent;const Entry *entries;int count;} Menu;
#define ACTION(n) {n,-1,NULL}
#define FOLDER(n,text) {-1,n,text}
static const Entry common[]={ACTION(3),ACTION(4),ACTION(2),ACTION(29),ACTION(5),ACTION(6),FOLDER(10,"选择奔跑方向 >"),ACTION(38)};
static const Entry items[]={ACTION(0),ACTION(7),ACTION(8),ACTION(9),ACTION(10),ACTION(11),ACTION(12),ACTION(13),ACTION(14),ACTION(15)};
static const Entry information[]={ACTION(19),ACTION(16),ACTION(17),ACTION(23),ACTION(18),ACTION(41)};
static const Entry display[]={ACTION(20),ACTION(21),ACTION(22)};
static const Entry session[]={ACTION(24),ACTION(25),ACTION(26),ACTION(27),ACTION(28),ACTION(39),ACTION(40)};
static const Entry replay_play[]={ACTION(0),ACTION(1),ACTION(2),ACTION(3),ACTION(4),ACTION(8),ACTION(9)};
static const Entry replay_seek[]={ACTION(5),ACTION(6),ACTION(7),ACTION(23),ACTION(24),ACTION(25),ACTION(26),ACTION(27),ACTION(28)};
static const Entry replay_info[]={ACTION(10),ACTION(11),ACTION(12),ACTION(14),ACTION(15)};
static const Entry replay_display[]={ACTION(13),ACTION(16),ACTION(17),ACTION(18)};
static const Entry replay_files[]={ACTION(19),ACTION(20),ACTION(21),ACTION(22)};
static const Entry running[]={ACTION(30),ACTION(31),ACTION(32),ACTION(33),ACTION(34),ACTION(35),ACTION(36),ACTION(37)};
#define MENU(title,parent,entries) {title,parent,entries,sizeof entries/sizeof *entries}
static const Menu menus[]={
    MENU("常用",-1,common),MENU("物品",-1,items),MENU("见闻",-1,information),MENU("显示",-1,display),MENU("本局",-1,session),
    MENU("播放",-1,replay_play),MENU("跳转",-1,replay_seek),MENU("见闻",-1,replay_info),MENU("显示",-1,replay_display),MENU("文件",-1,replay_files),
    MENU("奔跑方向",0,running),
};
static int active,closing,pending,view,scope,loaded,suspended;
static int remembered[sizeof menus/sizeof *menus],last_view[2]={0,5};
static int wizard,easy,rethrow=1,swap=1,graphics=1;
static Command selected_command;
/* Separate UI state in the game's existing per-port state directory. It never
   enters game saves or recordings. An empty override disables disk memory. */
static const char *memory_path(void) {
    const char *path=getenv("BROGUE_UI_MEMORY");return path?path:".brogue-handheld-menu";
}
static void load_memory(void) {
    if(loaded)return;
    loaded=1;const char *path=memory_path();if(!*path)return;
    FILE *f=fopen(path,"r");if(!f)return;
    char magic[16];int last[2],values[11],valid=1;
    if(fscanf(f,"%15s %d %d",magic,&last[0],&last[1])!=3 || strcmp(magic,"BRMENU2")
       || last[0]<0 || last[0]>4 || last[1]<5 || last[1]>9)valid=0;
    for(int i=0;i<11 && valid;i++)
        if(fscanf(f,"%d",&values[i])!=1 || values[i]<0 || values[i]>=menus[i].count)valid=0;
    fclose(f);if(!valid)return;
    memcpy(remembered,values,sizeof values);memcpy(last_view,last,sizeof last);
}
static void save_memory(void) {
    const char *path=memory_path();if(!*path)return;
    char temp[1024];if(snprintf(temp,sizeof temp,"%s.tmp",path)>=(int)sizeof temp)return;
    FILE *f=fopen(temp,"w");if(!f)return;
    int ok=fprintf(f,"BRMENU2 %d %d\n",last_view[0],last_view[1])>0;
    for(int i=0;i<11;i++)if(fprintf(f,"%d%c",remembered[i],i==10?'\n':' ')<0)ok=0;
    if(fclose(f))ok=0;
    if(ok) {if(rename(temp,path))remove(temp);} else remove(temp);
}
void br_commands_state(int wizard_mode,int easy_mode,int can_rethrow,int can_swap,int has_graphics) {
    wizard=wizard_mode;easy=easy_mode;rethrow=can_rethrow;swap=can_swap;graphics=has_graphics;
}
void br_commands_forget(void) {
    if(active)br_panel_end();
    active=closing=pending=scope=view=suspended=0;loaded=1;
    memset(remembered,0,sizeof remembered);last_view[0]=0;last_view[1]=5;
}
void br_commands_reload(void) {br_commands_forget();loaded=0;}
static const Command *command(const Entry *entry) {return &(scope?playback_commands:commands)[entry->command];}
static const char *unavailable(const Entry *entry) {
    if(entry->child>=0)return NULL;
    if(scope)return entry->command==18 && !graphics?"当前资源不支持图块":NULL;
    switch(entry->command) {
        case 11:return rethrow?NULL:"尚未投掷物品";
        case 12:return swap?NULL:"没有上一套装备";
        case 22:return graphics?NULL:"当前资源不支持图块";
        case 27:return easy?"已经开启":NULL;
        case 40:return wizard?NULL:"仅巫师模式";
        default:return NULL;
    }
}
static void show(void) {
    const Menu *menu=&menus[view];const char *tabs[5];int start=scope?5:0;
    for(int i=0;i<5;i++)tabs[i]=menus[start+i].title;
    br_panel_begin(scope?"录像":"行动",NULL);
    br_panel_tabs(tabs,5,view==10?0:view-start);
    for(int i=0;i<menu->count;i++) {
        const Entry *entry=&menu->entries[i];
        const char *label=entry->child>=0?entry->label:command(entry)->label;
        const char *reason=unavailable(entry);char full[512];
        if(reason) {snprintf(full,sizeof full,"%s（%s）",label,reason);label=full;}
        br_panel_add(label,'a'+i,reason==NULL);
    }
    br_panel_focus(remembered[view]);
    br_panel_hint(view==10?"上下选方向  A 奔跑  B 返回":"上下选择  A 执行  B 返回");
}
static void remember(void) {
    remembered[view]=br_panel_selected();last_view[scope]=view==10?0:view;save_memory();
}
int br_commands_take(int *key,int *control,int *shift) {
    if(!pending)return 0;
    pending=0;*key=selected_command.key;*control=selected_command.control&1;*shift=(selected_command.control&2)!=0;return 1;
}
int br_commands_active(void) {return active;}
int br_commands_begin(long key,int playback) {
    int origin=suspended && key==selected_command.key && scope==!!playback;
    suspended=0;return origin;
}
void br_commands_finish(int origin,int resume) {
    if(origin && resume) {show();active=1;closing=0;}
}
static int modal_command(int key) {
    return strchr("iaertTdcRDF?M~SQN&ACVO0\n",key)!=NULL && key!=0;
}
int br_commands_event(SDL_Event *e,int context) {
    if(closing) {
        if(e->type==SDL_TEXTINPUT || e->type==SDL_KEYUP)return 1;
        if(e->type==SDL_KEYDOWN)closing=0;
    }
    if(!active) {
        if(e->type!=SDL_KEYDOWN || e->key.keysym.sym!=SDLK_F6)return 0;
        if(!e->key.repeat && context && !br_panel_active()) {
            load_memory();scope=context==2;view=last_view[scope];show();active=1;
        }
        return 1;
    }
    if(e->type==SDL_KEYDOWN) {
        SDL_Keycode key=e->key.keysym.sym;
        if(key==SDLK_ESCAPE || key==SDLK_F6) {
            if(!e->key.repeat) {
                remember();
                if(view==10 && key==SDLK_ESCAPE) {view=0;show();}
                else {active=0;closing=1;br_panel_end();}
            }
            return 1;
        }
        if(key==SDLK_F8) {
            if(!e->key.repeat) {remember();view=scope?5:0;remembered[view]=0;show();remember();}
            return 1;
        }
        if(key==SDLK_LEFT || key==SDLK_RIGHT || key==SDLK_F2 || key==SDLK_F5) {
            if(!e->key.repeat) {
                remember();int start=scope?5:0,tab=view==10?0:view-start;
                view=start+(tab+((key==SDLK_LEFT || key==SDLK_F2)?4:1))%5;show();
            }
            return 1;
        }
    }
    int index=br_panel_selected();const Entry *entry=&menus[view].entries[index];
    int confirm=e->type==SDL_KEYDOWN && e->key.keysym.sym==SDLK_RETURN && !e->key.repeat;
    if(br_panel_event(e))return 1;
    if(confirm) {
        remember();
        if(entry->child>=0) {view=entry->child;show();return 1;}
        selected_command=*command(entry);pending=selected_command.control!=0;
        suspended=modal_command(selected_command.key);
        active=0;br_panel_end();
        Uint32 window=e->type==SDL_TEXTINPUT?e->text.windowID:e->key.windowID;
        memset(e,0,sizeof *e);
        if(selected_command.key=='\n' || selected_command.key=='\t') {
            e->type=SDL_KEYDOWN;e->key.windowID=window;e->key.keysym.sym=selected_command.key=='\n'?SDLK_RETURN:SDLK_TAB;
        } else {e->type=SDL_TEXTINPUT;e->text.windowID=window;e->text.text[0]=selected_command.key;}
        return 0;
    }
    return e->type==SDL_KEYDOWN || e->type==SDL_KEYUP || e->type==SDL_TEXTINPUT;
}

#!/usr/bin/env python3
"""Instrument an isolated generated tree; never ship this fixture."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);p=root/'src/brogue/MainMenu.c';s=p.read_text()
anchor='                mainInputLoop();'
assert s.count(anchor)>=1
block=r'''
                if(getenv("BR_TEST_DETAILS")) {
                    FILE *out=fopen("monster-details.txt","w");
                    if(!out)exit(20);
                    short oldRNG=rogue.RNG;rogue.RNG=RNG_COSMETIC;
                    for(int id=1;id<NUMBER_MONSTER_KINDS;id++) {
                        creature *base=generateMonster(id,false,false);
                        for(int mutation=-1;mutation<NUMBER_MUTATORS;mutation++) {
                            creature test=*base;test.loc=player.loc;
                            if(mutation>=0)mutateMonster(&test,mutation);
                            test.currentHP=max(1,test.info.maxHP);
                            for(int state=0;state<3;state++) {
                                test.creatureState=state==1?MONSTER_ALLY:MONSTER_SLEEPING;
                                test.bookkeepingFlags=state==2?MB_CAPTIVE:0;
                                char text[100000];
                                unsigned long turn=rogue.playerTurnNumber;
                                short hp=player.currentHP;
                                monsterDetails(text,&test);
                                if(rogue.playerTurnNumber!=turn || player.currentHP!=hp)exit(21);
                                fprintf(out,"\n=== %d %d %d ===\n%s\n",id,mutation,state,text);
                            }
                        }
                    }
                    rogue.RNG=oldRNG;fclose(out);
                    creature *monkey=generateMonster(MK_MONKEY,false,false);monkey->loc=player.loc;
                    printMonsterDetails(monkey);
                    rogueEvent testEvent;nextBrogueEvent(&testEvent,false,false,true);
                    exit(0);
                }
'''

target=r"""
                if(getenv("BR_TEST_TARGET")) {
                    for(creatureIterator it=iterateCreatures(monsters);hasNextCreature(it);)nextCreature(&it)->bookkeepingFlags|=MB_IS_DYING;
                    pmapAt(player.loc)->flags&=~HAS_PLAYER;
                    player.loc=(pos){40,15};
                    for(int x=37;x<=48;x++)for(int y=12;y<=18;y++) {
                        for(int l=0;l<NUMBER_TERRAIN_LAYERS;l++)pmap[x][y].layers[l]=NOTHING;
                        pmap[x][y].layers[DUNGEON]=FLOOR;
                        pmap[x][y].flags=VISIBLE|IN_FIELD_OF_VIEW|DISCOVERED;
                    }
                    pmapAt(player.loc)->flags|=HAS_PLAYER;
                    for(int x=42;x<=46;x+=2) {
                        creature *m=generateMonster(MK_MONKEY,false,false);m->loc=x==42?(pos){42,15}:x==44?(pos){40,17}:(pos){38,15};
                        pmapAt(m->loc)->flags|=HAS_MONSTER;
                    }
                    item *dart=generateItem(WEAPON,DART);
                    FILE *out=fopen("target-results.txt","w");if(!out)exit(30);
                    unsigned long turn=rogue.playerTurnNumber;
                    for(int trial=0;trial<4;trial++) {
                        rogue.lastTarget=NULL;refreshSideBar(-1,-1,false);displayLevel();
                        fprintf(out,"READY %d\n",trial);fflush(out);
                        pos result=INVALID_POS,first=INVALID_POS,expected=INVALID_POS;
                        nextTargetAfter(dart,&first,player.loc,AUTOTARGET_MODE_THROW,false);
                        if(trial<2)nextTargetAfter(dart,&expected,first,AUTOTARGET_MODE_THROW,trial==1);
                        else expected=first;
                        boolean ok=chooseTarget(&result,12,AUTOTARGET_MODE_THROW,dart);
                        fprintf(out,"RESULT %d %d %d %d\n",trial,ok,result.x,result.y);fflush(out);
                        if(rogue.playerTurnNumber!=turn)exit(31);
                        if(trial<3 && (!ok || !posEq(result,expected)))exit(32);
                        if(trial==3 && ok)exit(33);
                    }
                    fclose(out);exit(0);
                }
"""

potion=r"""
                if(getenv("BR_TEST_POTION")) {
                    extern void br_test_potion_throw(void);
                    br_test_potion_throw();
                    rogueEvent testEvent;nextBrogueEvent(&testEvent,false,false,true);exit(0);
                }
"""
s=s.replace(anchor,potion+target+block+anchor,1);p.write_text(s)



p=root/'src/brogue/Items.c';s=p.read_text();anchor='        moveCursor(&targetConfirmed, &canceled, &tabKey, &targetLoc, &event, NULL, false, true, false);'
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'\n        if(getenv("BR_TEST_TARGET"))fprintf(stderr,"EVENT %ld ctrl%d shift%d tab%d cur%d,%d\\n",event.param1,event.controlKey,event.shiftKey,tabKey,targetLoc.x,targetLoc.y);')
p.write_text(s)

p=root/'src/brogue/Items.c';s=p.read_text()+r"""
void br_test_potion_throw(void) {
    pos target={min(DCOLS-2,player.loc.x+1),player.loc.y};
    for(int l=0;l<NUMBER_TERRAIN_LAYERS;l++)pmapAt(target)->layers[l]=NOTHING;
    pmapAt(target)->layers[DUNGEON]=FLOOR;pmapAt(target)->flags=VISIBLE|IN_FIELD_OF_VIEW|DISCOVERED;
    item *potion=generateItem(POTION,POTION_LIFE);
    strcpy(potionTable[POTION_LIFE].flavor,"indigo");
    throwItem(potion,&player,target,12);
}
""";p.write_text(s)

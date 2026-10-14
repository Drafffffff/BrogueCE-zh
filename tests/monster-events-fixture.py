#!/usr/bin/env python3
"""Add isolated native event and repeat-throw checks; never ship this tree."""
from pathlib import Path
import sys
root=Path(sys.argv[1]);p=root/'src/brogue/MainMenu.c';s=p.read_text()
anchor='                mainInputLoop();'
block=r'''
                if(getenv("BR_TEST_MONSTER_EVENTS")) {
                    FILE *out=fopen("monster-events.txt","w");if(!out)exit(20);
                    for(int i=0;i<NUMBER_MONSTER_KINDS;i++) {
                        if(monsterText[i].DFMessage[0])fprintf(out,"death %d: %s\n",i,br_tr(monsterText[i].DFMessage));
                        if(monsterText[i].summonMessage[0])fprintf(out,"summon %d: %s\n",i,br_tr(monsterText[i].summonMessage));
                        fprintf(out,"absorb %d: %s / %s\n",i,br_tr(monsterText[i].absorbing),br_tr(monsterText[i].absorbStatus));
                    }
                    creature *bloat=generateMonster(MK_BLOAT,false,false);bloat->loc=player.loc;
                    bloat->status[STATUS_INVISIBLE]=0;
                    unsigned long turn=rogue.playerTurnNumber;
                    killCreature(bloat,false);
                    if(rogue.playerTurnNumber!=turn)exit(21);
                    for(int i=0;i<MESSAGE_LINES;i++)fprintf(out,"visible %s\n",displayedMessage[i]);
                    fclose(out);refreshScreen();
                    rogueEvent event;nextBrogueEvent(&event,false,false,true);exit(0);
                }
'''
s=s.replace(anchor,block+anchor,1)
a='                    FILE *out=fopen("target-results.txt","w");if(!out)exit(30);'
assert a in s
s=s.replace(a,'''                    dart->quantity=15;dart->inventoryLetter='z';dart->nextItem=packItems->nextItem;packItems->nextItem=dart;
'''+a)
a='                    fclose(out);exit(0);\n                }\n';start=s.index('if(getenv("BR_TEST_TARGET"))');end=s.index(a,start)
s=s[:end]+r'''
                    pos initial=INVALID_POS;
                    refreshSideBar(-1,-1,false);nextTargetAfter(dart,&initial,player.loc,AUTOTARGET_MODE_THROW,false);
                    rogue.lastItemThrown=dart;rogue.lastTarget=monsterAtLoc(initial);
                    fprintf(out,"RETHROW_READY\n");fflush(out);
                    executeKeystroke(RETHROW_KEY,false,false);
                    if(rogue.playerTurnNumber!=turn || dart->quantity!=15)exit(34);
                    fprintf(out,"RETHROW_CANCEL_OK\n");fflush(out);
                    creature *blade=generateMonster(MK_SPECTRAL_BLADE,false,false);blade->loc=(pos){41,15};pmapAt(blade->loc)->flags|=HAS_MONSTER;
                    rogue.lastTarget=NULL;refreshSideBar(-1,-1,false);
                    pos selected=INVALID_POS;
                    boolean found=false;selected=player.loc;
                    for(int j=0;j<10;j++){pos next;if(!nextTargetAfter(dart,&next,selected,AUTOTARGET_MODE_THROW,false))break;selected=next;if(posEq(selected,blade->loc)){found=true;break;}}
                    if(!found)exit(35);
                    blade->status[STATUS_INVISIBLE]=100;
                    selected=player.loc;
                    for(int j=0;j<10;j++){pos next;if(!nextTargetAfter(dart,&next,selected,AUTOTARGET_MODE_THROW,false))break;selected=next;if(posEq(selected,blade->loc))exit(36);}
                    fprintf(out,"VISIBLE_BLADE_AND_HIDDEN_FILTER_OK\n");fflush(out);
'''+s[end:];p.write_text(s)

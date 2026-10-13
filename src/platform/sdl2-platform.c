#ifdef SDL_PATHS
#include <unistd.h>
#endif

#include <limits.h>
#include <SDL_image.h>
#include "platform.h"
#include "zh_pad.h"
#include "zh_commands.h"
#include "GlobalsBase.h"
#include "tiles.h"
#include "zh_reader.h"
#include "zh_map.h"
#include "zh_panel.h"
#include "zh_keyboard.h"

#define PAUSE_BETWEEN_EVENT_POLLING     36L//17
#define MAX_REMAPS  128

struct keypair {
    char from;
    char to;
};

static struct keypair remapping[MAX_REMAPS];
static size_t nremaps = 0;
static enum graphicsModes showGraphics = TEXT_GRAPHICS;

static rogueEvent lastEvent;


static void sdlfatal(char *file, int line) {
    fprintf(stderr, "Fatal SDL error (%s:%d): %s\n", file, line, SDL_GetError());
    exit(EXIT_STATUS_FAILURE_PLATFORM_ERROR);
}


static void imgfatal(char *file, int line) {
    fprintf(stderr, "Fatal SDL_image error (%s:%d): %s\n", file, line, IMG_GetError());
    exit(EXIT_STATUS_FAILURE_PLATFORM_ERROR);
}


/*
If the key is to be processed, returns true and updates event. False
otherwise. This function only listens for keypresses which do not produce
corresponding TextInputEvents.
*/
static boolean eventFromKey(rogueEvent *event, SDL_Keycode key) {
    event->param1 = -1;

    switch (key) {
        case SDLK_ESCAPE:
            event->param1 = ESCAPE_KEY;
            return true;
        case SDLK_UP:
            event->param1 = UP_ARROW;
            return true;
        case SDLK_DOWN:
            event->param1 = DOWN_ARROW;
            return true;
        case SDLK_RIGHT:
            event->param1 = RIGHT_ARROW;
            return true;
        case SDLK_LEFT:
            event->param1 = LEFT_ARROW;
            return true;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            event->param1 = RETURN_KEY;
            return true;
        case SDLK_BACKSPACE:
            event->param1 = DELETE_KEY;
            return true;
        case SDLK_TAB:
            event->param1 = TAB_KEY;
            return true;
        case SDLK_PRINTSCREEN:
            event->param1 = PRINTSCREEN_KEY;
            return true;
    }

    /*
    Only process keypad events when we're holding a modifier, as there is no
    TextInputEvent then.
    */
    if (event->shiftKey || event->controlKey) {
        switch (key) {
            case SDLK_KP_0:
                event->param1 = NUMPAD_0;
                return true;
            case SDLK_KP_1:
                event->param1 = NUMPAD_1;
                return true;
            case SDLK_KP_2:
                event->param1 = NUMPAD_2;
                return true;
            case SDLK_KP_3:
                event->param1 = NUMPAD_3;
                return true;
            case SDLK_KP_4:
                event->param1 = NUMPAD_4;
                return true;
            case SDLK_KP_5:
                event->param1 = NUMPAD_5;
                return true;
            case SDLK_KP_6:
                event->param1 = NUMPAD_6;
                return true;
            case SDLK_KP_7:
                event->param1 = NUMPAD_7;
                return true;
            case SDLK_KP_8:
                event->param1 = NUMPAD_8;
                return true;
            case SDLK_KP_9:
                event->param1 = NUMPAD_9;
                return true;
        }
    }

    // Ctrl+letter doesn't give a TextInputEvent
    if (event->controlKey && key >= SDLK_a && key <= SDLK_z) {
        event->param1 = 'a' + (key - SDLK_a);
        if (event->shiftKey) event->param1 -= 'a' - 'A';
        return true;
    }

    return false;
}


static boolean _modifierHeld(int mod) {
    SDL_Keymod km = SDL_GetModState();
    return mod == 0 && (km & (KMOD_LSHIFT | KMOD_RSHIFT))
        || mod == 1 && (km & (KMOD_LCTRL | KMOD_RCTRL));
}


static char applyRemaps(char c) {
    for (size_t i=0; i < nremaps; i++) {
        if (remapping[i].from == c) return remapping[i].to;
    }
    return c;
}


/*
If an event is available, returns true and updates returnEvent. Otherwise
it returns false and an error event. This function also processes
platform-specific inputs/behaviours.
*/
/* Convert physical pointer coordinates back through the camera, including HiDPI. */
static int br_camera_mouse(Uint32 windowID,int x,int y,int *wx,int *wy) {
    if (!br_map_showing()) return -1;
    SDL_Window *window=SDL_GetWindowFromID(windowID);
    int w,h,rw,rh,mx,my;
    if(!window) return 0;
    SDL_GetWindowSize(window,&w,&h);
    if(!w || !h || SDL_GetRendererOutputSize(SDL_GetRenderer(window),&rw,&rh)) return 0;
    if(!br_map_pointer(x*rw/w,y*rh/h,&mx,&my)) return 0;
    *wx=mapToWindowX(mx); *wy=mapToWindowY(my);
    return 1;
}

static boolean pollBrogueEvent(rogueEvent *returnEvent, boolean textInput) {
    static int mx = 0, my = 0;

    returnEvent->eventType = EVENT_ERROR;
    returnEvent->shiftKey = _modifierHeld(0);
    returnEvent->controlKey = _modifierHeld(1);

    SDL_Event event;
    boolean ret = false;


    // ~ for (int i=0; i < 100 && SDL_PollEvent(&event); i++) {
    int brPlayback=rogue.playbackMode && (rogue.playbackPaused || rogue.playbackBetweenTurns);
    br_map_playback(brPlayback,rogue.playbackPaused,rogue.playbackOOS,rogue.playerTurnNumber,rogue.howManyTurns);
    br_map_player(player.loc.x,player.loc.y,rogue.depthLevel,player.currentHP,player.info.maxHP);
    br_commands_state(WIZARD_MODE,rogue.mode==GAME_MODE_EASY,rogue.lastItemThrown!=NULL,rogue.swappedIn!=NULL && rogue.swappedOut!=NULL,hasGraphics);
    br_pad_poll(br_keyboard_active()?2:br_reader_active()?6:br_commands_active()?10:br_panel_inventory_active()?9:(br_panel_active() || br_map_nearby_active())?8:br_map_browsing()?5:br_map_ack_active()==2?7:br_map_ack_active()?0:br_map_aiming()?3:brPlayback?4:br_map_moving());
    while (SDL_PollEvent(&event)) {
        boolean brTextEntry=br_keyboard_active() || br_panel_active();
        if (br_keyboard_event(&event)) { ret=false; returnEvent->eventType=EVENT_ERROR; refreshScreen(); updateScreen(); continue; }
        if(brPlayback && event.type==SDL_KEYDOWN && !event.key.repeat
           && (event.key.keysym.sym==SDLK_F6 || event.key.keysym.sym==SDLK_F3)
           && !br_reader_active() && !br_panel_active() && !br_keyboard_active() && !br_map_aiming())
            rogue.playbackPaused=true;
        if (!br_commands_active() && !(br_map_nearby_active() && !br_reader_active() && event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_F2) && br_reader_event(&event)) { ret=false; returnEvent->eventType=EVENT_ERROR; refreshScreen(); updateScreen(); continue; }
        if(!br_panel_active() && !br_keyboard_active() && br_map_event(&event)) { ret=false; returnEvent->eventType=EVENT_ERROR; refreshScreen(); updateScreen(); continue; }
        if (br_commands_event(&event,(!br_reader_active() && !br_map_nearby_active() && !br_map_aiming() && !br_map_browsing())?(brPlayback?2:br_map_moving()):0)) { ret=false; returnEvent->eventType=EVENT_ERROR; refreshScreen(); updateScreen(); continue; }
        int brKey,brControl,brShift;
        if(br_commands_take(&brKey,&brControl,&brShift)) {
            returnEvent->eventType=KEYSTROKE;returnEvent->param1=brKey;
            returnEvent->param2=0;returnEvent->controlKey=brControl;returnEvent->shiftKey=brShift;
            return true;
        }
        if (br_panel_event(&event)) { ret=false; returnEvent->eventType=EVENT_ERROR; refreshScreen(); updateScreen(); continue; }
        if(br_panel_take_detail()) returnEvent->shiftKey=true;
        if (event.type == SDL_QUIT) {
            // the player clicked the X button!
            SDL_Quit();
            int statusCode = quitImmediately();
            exit(statusCode);
        } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
            resizeWindow(event.window.data1, event.window.data2);
        } else if (event.type == SDL_KEYDOWN) {
            SDL_Keycode key = event.key.keysym.sym;
            returnEvent->controlKey = _modifierHeld(1) || (event.key.keysym.mod & KMOD_CTRL);
            returnEvent->shiftKey = _modifierHeld(0) || (event.key.keysym.mod & KMOD_SHIFT);

            if (key == SDLK_PAGEUP) {
                resizeWindow(max(windowWidth * 11/10, windowWidth + 1), max(windowHeight * 11/10, windowHeight + 1));
                continue;
            } else if (key == SDLK_PAGEDOWN) {
                fullScreen = false;
                resizeWindow(max(windowWidth * 10/11, 1), max(windowHeight * 10/11, 1));
                continue;
            } else if (key == SDLK_F11 || key == SDLK_F12
                    || key == SDLK_RETURN && !brTextEntry && (SDL_GetModState() & KMOD_ALT)) {
                fullScreen = !fullScreen;
                resizeWindow(-1, -1);  // Reset to starting resolution
                continue;
            }

            if (eventFromKey(returnEvent, key)) {
                returnEvent->eventType = KEYSTROKE;
                return true;
            }
        } else if (event.type == SDL_TEXTINPUT && (unsigned char)(event.text.text[0]) < 0x80) {
            /*
            It's difficult/impossible to check what characters are on the
            shifts of keys. So to detect '&', '>' etc. reliably we need to
            listen for text input events as well as keydowns. This results
            in hybrid keyboard code, where Brogue KEYSTROKEs can come from
            different SDL events.
            */
            char c = event.text.text[0];

            if (!textInput) {
                c = applyRemaps(c);
                if (c == '=' || c == '+') {
                    resizeWindow(max(windowWidth * 11/10, windowWidth + 1), max(windowHeight * 11/10, windowHeight + 1));
                } else if (c == '-') {
                    fullScreen = false;
                    resizeWindow(max(windowWidth * 10/11, 1), max(windowHeight * 10/11, 1));
                }
            }

            returnEvent->eventType = KEYSTROKE;
            returnEvent->param1 = c;
            // ~ printf("textinput %s\n", event.text.text);
            return true;
        } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT) {
                if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                    returnEvent->eventType = MOUSE_DOWN;
                } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                    returnEvent->eventType = MOUSE_UP;
                } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
                    returnEvent->eventType = RIGHT_MOUSE_DOWN;
                } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) {
                    returnEvent->eventType = RIGHT_MOUSE_UP;
                }
                int wx,wy,mapped=br_camera_mouse(event.button.windowID,event.button.x,event.button.y,&wx,&wy);
                if(mapped==0) { ret=false; returnEvent->eventType=EVENT_ERROR; continue; }
                returnEvent->param1 = mapped>0 ? wx : event.button.x * COLS / windowWidth;
                returnEvent->param2 = mapped>0 ? wy : event.button.y * ROWS / windowHeight;
                return true;
            }
        } else if (event.type == SDL_MOUSEMOTION) {
            // We don't want to return on a mouse motion event, because only the last
            // in the queue is important. That's why we just set ret=true
            int xcell = event.motion.x * COLS / windowWidth,
                ycell = event.motion.y * ROWS / windowHeight;
            int mapped=br_camera_mouse(event.motion.windowID,event.motion.x,event.motion.y,&xcell,&ycell);
            if(mapped==0) continue;
            if (xcell != mx || ycell != my) {
                returnEvent->eventType = MOUSE_ENTERED_CELL;
                returnEvent->param1 = xcell;
                returnEvent->param2 = ycell;
                mx = xcell;
                my = ycell;
                ret = true;
            }
        }
    }

    return ret;
}


static void _gameLoop() {
#ifdef SDL_PATHS
    char *path = SDL_GetBasePath();
    if (path) {
        fprintf(stderr, "Base path: %s\n", path);
        path[strlen(path) - 1] = '\0';  // remove trailing separator
        strcpy(dataDirectory, path);
    } else {
        fprintf(stderr, "Failed to find the path to the application\n");
        exit(EXIT_STATUS_FAILURE_PLATFORM_ERROR);
    }
    free(path);

    path = SDL_GetPrefPath("Brogue", "Brogue CE");
    if (!path || chdir(path) != 0) {
        fprintf(stderr, "Failed to find or change to the save directory\n");
        exit(EXIT_STATUS_FAILURE_PLATFORM_ERROR);
    }
    fprintf(stderr, "Save path: %s\n", path);
    free(path);
#endif

    if (SDL_Init(SDL_INIT_VIDEO) < 0) sdlfatal(__FILE__, __LINE__);

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) imgfatal(__FILE__, __LINE__);

    initTiles();

    lastEvent.eventType = EVENT_ERROR;

    resizeWindow(windowWidth, windowHeight);

    int statusCode = rogueMain();

    SDL_Quit();

    exit(statusCode);
}

static long lastDelayTime = 0;

// Like SDL_Delay, but reduces the delay if time has passed since the last delay
static void _delayUpTo(short ms) {
    long curTime = SDL_GetTicks();
    long timeDiff = curTime - lastDelayTime;
    ms -= timeDiff;

    if (ms > 0) {
        SDL_Delay(ms);
    } // else delaying further would go past the time we want to delay until

    lastDelayTime = SDL_GetTicks();
}

static boolean _pauseForMilliseconds(short ms, PauseBehavior behavior) {
    updateScreen();
    _delayUpTo(ms);

    if (lastEvent.eventType != EVENT_ERROR
        && (lastEvent.eventType != MOUSE_ENTERED_CELL || behavior.interuptForMouseMove)) {
        return true; // SDL already gave us an interrupting event to process
    }

    return pollBrogueEvent(&lastEvent, false) // ask SDL for a new event if one is available
        && lastEvent.eventType != EVENT_ERROR // and check if it is interrupting
        && (lastEvent.eventType != MOUSE_ENTERED_CELL || behavior.interuptForMouseMove);
}


static void _nextKeyOrMouseEvent(rogueEvent *returnEvent, boolean textInput, boolean colorsDance) {
    updateScreen();

    if (lastEvent.eventType != EVENT_ERROR) {
        *returnEvent = lastEvent;
        lastEvent.eventType = EVENT_ERROR;
        return;
    }

    while (true) {
        if (colorsDance) {
            shuffleTerrainColors(3, true);
            commitDraws();
        }

        updateScreen();

        if (pollBrogueEvent(returnEvent, textInput)) break;

        _delayUpTo(PAUSE_BETWEEN_EVENT_POLLING);
    }
}


/*
Returns the index of the sprite representing the given glyph. Sprites <256 are
from the text font sheet, 256+ are from the tiles sheet.
*/
static int fontIndex(enum displayGlyph glyph) {
    if ((unsigned int)glyph & 0x200000) return glyph;
    // These are the only non-ASCII glyphs which always come from the font sheet
    if (glyph == G_UP_ARROW) return 0x90;
    if (glyph == G_DOWN_ARROW) return 0x91;

    if (glyph < 128) {
        // ASCII characters map directly
        return glyph;
    } else if (showGraphics == TILES_GRAPHICS || (showGraphics == HYBRID_GRAPHICS && isEnvironmentGlyph(glyph))) {
        // Tile glyphs have sprite indices starting at 256
        // -2 to disregard the up and down arrow glyphs
        return glyph + 128 - 2;
    } else {
        unsigned int code = glyphToUnicode(glyph);
        switch (code) {
            case U_MIDDLE_DOT: return 0x80;
            case U_FOUR_DOTS: return 0x81;
            case U_DIAMOND: return 0x82;
            case U_FLIPPED_V: return 0x83;
            case U_ARIES: return 0x84;
            case U_ESZETT: return 0xdf;
            case U_ANKH: return 0x85;
            case U_MUSIC_NOTE: return 0x86;
            case U_CIRCLE: return 0x87;
            case U_LIGHTNING_BOLT: return 0x99;
            case U_FILLED_CIRCLE: return 0x89;
            case U_NEUTER: return 0x8a;
            case U_U_ACUTE: return 0xda;
            case U_CURRENCY: return 0xa4;
            case U_UP_ARROW: return 0x90;
            case U_DOWN_ARROW: return 0x91;
            case U_LEFT_ARROW: return 0x92;
            case U_RIGHT_ARROW: return 0x93;
            case U_OMEGA: return 0x96;
            case U_CIRCLE_BARS: return 0x8c;
            case U_FILLED_CIRCLE_BARS: return 0x8d;
            case U_LEFT_TRIANGLE: return 0x8e;

            default:
                brogueAssert(code < 256);
                return code;
        }
    }
}


void br_game_nearby_add(const BrNearby *entry,int glyph) {
    BrNearby copy=*entry;copy.atlas=fontIndex(glyph);br_map_nearby_add(&copy);
}
void br_map_record(int x,int y,int glyph,int fr,int fg,int fb,int br,int bg,int bb) {
    br_map_put(x,y,fontIndex(glyph),(SDL_Color){fr*255/100,fg*255/100,fb*255/100,255},
        (SDL_Color){br*255/100,bg*255/100,bb*255/100,255});
}

static void _plotChar(
    enum displayGlyph inputChar,
    short x, short y,
    short foreRed, short foreGreen, short foreBlue,
    short backRed, short backGreen, short backBlue
) {
    updateTile(y, x, fontIndex(inputChar),
        foreRed, foreGreen, foreBlue,
        backRed, backGreen, backBlue);
}


static void _remap(const char *from, const char *to) {
    if (nremaps < MAX_REMAPS) {
        remapping[nremaps].from = from[0];
        remapping[nremaps].to = to[0];
        nremaps++;
    }
}


/*
 * Take screenshot in current working directory (ScreenshotN.png)
 */
static boolean _takeScreenshot() {
    SDL_Surface *screenshot = captureScreen();
    if (!screenshot) return false;

    // choose filename
    char screenshotFilepath[BROGUE_FILENAME_MAX];
    getAvailableFilePath(screenshotFilepath, "Screenshot", SCREENSHOT_SUFFIX);
    strcat(screenshotFilepath, SCREENSHOT_SUFFIX);

    // save to PNG
    IMG_SavePNG(screenshot, screenshotFilepath);
    SDL_FreeSurface(screenshot);

    return true;
}


static enum graphicsModes _setGraphicsMode(enum graphicsModes mode) {
    showGraphics = mode;
    refreshScreen();
    updateScreen();
    return mode;
}


struct brogueConsole sdlConsole = {
    _gameLoop,
    _pauseForMilliseconds,
    _nextKeyOrMouseEvent,
    _plotChar,
    _remap,
    _modifierHeld,
    NULL,
    _takeScreenshot,
    _setGraphicsMode
};

#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu
SDCARD_PATH=${SDCARD_PATH:-/mnt/SDCARD}
SYSTEM_PATH=${SYSTEM_PATH:-$SDCARD_PATH/.system/h700}
GAMEDIR="$SDCARD_PATH/Roms/Ports (PORTS)/.ports/brogue-rg34xx"
STATE="$SDCARD_PATH/.userdata/shared/brogue-rg34xx"
mkdir -p "$STATE"
exec >>"$STATE/launch.log" 2>&1
[ -x "$GAMEDIR/brogue" ] || { echo 'Missing Brogue engine'; exit 1; }
LOCK=/tmp/brogue-rg34xx.lock
if ! mkdir "$LOCK" 2>/dev/null; then echo 'Brogue is already running, or a stale lock remains.'; exit 1; fi
child=
cleanup() { rmdir "$LOCK" 2>/dev/null || true; }
stop() { [ -z "$child" ] || kill -TERM "$child" 2>/dev/null || true; }
trap cleanup EXIT
trap stop INT TERM
unset LD_PRELOAD GT_INPUT_REMAP GT_HUD
export BROGUE_HANDHELD=1 BROGUE_LANG=zh
export BROGUE_CJK_FONT="$GAMEDIR/assets/NotoSansCJKsc-Regular.otf"
export XDG_DATA_HOME="$STATE" XDG_CONFIG_HOME="$STATE"
export SDL_VIDEODRIVER=mali SDL_AUDIODRIVER=alsa
export SDL_JOYSTICK_DISABLE_UDEV=1 SDL_HIDAPI_DISABLE_UDEV=1
export SDL_VIDEO_EGL_DRIVER=/usr/lib/libEGL.so SDL_VIDEO_GL_DRIVER=/usr/lib/libGLESv2.so
export LD_LIBRARY_PATH="$GAMEDIR/lib:$SYSTEM_PATH/lib:/usr/lib:/usr/lib/aarch64-linux-gnu:/lib/aarch64-linux-gnu"
cd "$STATE"
"$GAMEDIR/brogue" --data-dir "$GAMEDIR" --full-screen &
child=$!
status=0
wait "$child" || status=$?
if kill -0 "$child" 2>/dev/null; then wait "$child" || status=$?; fi
exit "$status"

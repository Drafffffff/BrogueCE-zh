#!/bin/sh
set -eu
BASE="$HOME/Games/brogue-zh-events-test"
TEST=$(mktemp -d /tmp/brogue-target.XXXXXX)
export DISPLAY=:93 LD_LIBRARY_PATH="$HOME/Games/Brogue/lib" BROGUE_CJK_FONT="$BASE/bin/assets/NotoSansCJKsc-Regular.otf" BROGUE_LANG=zh BR_TEST_TARGET=1
Xvfb :93 -screen 0 1600x1000x24 -noreset >"$TEST/xvfb.log" 2>&1 &
xpid=$!;pid=
cleanup() { [ -z "$pid" ] || kill "$pid" 2>/dev/null || true; kill "$xpid" 2>/dev/null || true; }
trap cleanup EXIT
sleep 1
cd "$TEST"
"$BASE/bin/brogue" --data-dir "$BASE/bin" --no-gpu >game.log 2>&1 &
pid=$!
sleep 3
window=$(xdotool search --onlyvisible --name Brogue | head -1)
xdotool windowsize "$window" 1600 1000
xdotool mousemove 1590 990
xdotool key n
sleep 2
xdotool key Tab
sleep 0.2
xdotool key Return
sleep 0.5
xdotool keydown ctrl
sleep 0.2
xdotool key Tab
sleep 0.2
xdotool keyup ctrl
xdotool key Return
sleep 0.5
xdotool key Tab
sleep 0.2
xdotool keydown ctrl
sleep 0.2
xdotool key Tab
sleep 0.2
xdotool keyup ctrl
xdotool key Return
sleep 0.5
xdotool key Escape
sleep 0.5
xdotool key Tab
sleep 0.2
xdotool key ctrl+Tab
sleep 0.2
xdotool key Escape
wait "$pid"
pid=
cp target-results.txt "$BASE/target-results.txt"
cp game.log "$BASE/target.log"

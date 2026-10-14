#!/bin/sh
set -eu
BASE="$HOME/Games/brogue-zh-events-test"
TEST=$(mktemp -d /tmp/brogue-events.XXXXXX)
export DISPLAY=:94 LD_LIBRARY_PATH="$HOME/Games/Brogue/lib" BROGUE_CJK_FONT="$BASE/bin/assets/NotoSansCJKsc-Regular.otf" BROGUE_LANG=zh BR_TEST_MONSTER_EVENTS=1
Xvfb :94 -screen 0 1600x1000x24 -noreset >"$TEST/xvfb.log" 2>&1 &
xpid=$!;pid=
cleanup() { [ -z "$pid" ] || kill "$pid" 2>/dev/null || true; kill "$xpid" 2>/dev/null || true; }
trap cleanup EXIT
sleep 1
cd "$TEST"
"$BASE/bin/brogue" --data-dir "$BASE/bin" --no-gpu --seed 123 >game.log 2>&1 &
pid=$!
sleep 3
xdotool mousemove 1590 990
xdotool key n
sleep 3
ffmpeg -hide_banner -loglevel error -f x11grab -i :94 -frames:v 1 -y "$BASE/monster-events.png"
xdotool key space
wait "$pid"
pid=
cp monster-events.txt "$BASE/monster-events.txt"
cp game.log "$BASE/monster-events.log"

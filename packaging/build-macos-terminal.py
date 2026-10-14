#!/usr/bin/env python3
"""Build an Apple Silicon Chinese Terminal package with system ncurses."""
from pathlib import Path
import argparse,subprocess,shutil,hashlib,json
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'dist/terminal');a=p.parse_args();out=a.output.resolve()
if out.exists():raise SystemExit('Output exists; choose a new directory')
subprocess.run(['make','-B','-j4','TERMINAL=YES','GRAPHICS=NO','MAC_APP=NO','RELEASE=YES','CFLAGS=-mmacosx-version-min=11.0','LDLIBS=-mmacosx-version-min=11.0'],cwd=ROOT,check=True)
folder=out/'Brogue Terminal';folder.mkdir(parents=True)
exe=folder/'brogue';shutil.copy2(ROOT/'bin/brogue',exe)
subprocess.run(['codesign','--force','--sign','-',str(exe)],check=True)
subprocess.run(['codesign','--verify','--strict',str(exe)],check=True)
for n in ['LICENSE.txt','README.md']:shutil.copy2(ROOT/n,folder/n)
shutil.copy2(ROOT/'bin/keymap.txt',folder/'keymap.txt')
launch=folder/'launch.sh';launch.write_text('''#!/bin/sh
set -eu
bundle_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LANG=en_US.UTF-8 LC_CTYPE=en_US.UTF-8 BROGUE_LANG=${BROGUE_LANG:-zh}
export TERM=${TERM:-xterm-256color}
state_dir="$HOME/Library/Application Support/Brogue/Brogue CE"
mkdir -p "$state_dir"
cd "$state_dir"
exec "$bundle_dir/brogue" --data-dir "$bundle_dir" "$@"
''');launch.chmod(0o755)
command=out/'Brogue Terminal.command';command.write_text('''#!/bin/sh
set -eu
bundle_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
printf '\033[8;40;120t'
exec "$bundle_dir/Brogue Terminal/launch.sh" "$@"
''');command.chmod(0o755)
(out/'使用说明.txt').write_text('Brogue CE 中文 Terminal · Apple Silicon · macOS 11+\n\n双击 Brogue Terminal.command；或在终端中运行 Brogue Terminal/launch.sh。\n终端至少 100 列、34 行。使用终端默认等宽字体和中文回退字体。\n存档与 SDL2 图形版共用 ~/Library/Application Support/Brogue/Brogue CE/。\n不要同时打开同一局。键位见 Brogue Terminal/README.md。\n源码：https://github.com/Drafffffff/BrogueCE-zh/tree/汉化\n')
manifest=dict(platform='macOS arm64',minimum_macos='11.0',binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),libraries=[line.strip().split(' (')[0] for line in subprocess.check_output(['otool','-L',str(exe)],text=True).splitlines()[1:]])
(out/'terminal-build.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n');print(json.dumps(manifest))

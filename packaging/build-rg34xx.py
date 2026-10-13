#!/usr/bin/env python3
"""Build the checked-in Chinese RG34XX sources with a pinned NextUI SDK."""
from pathlib import Path
import hashlib,json,shutil,subprocess,tarfile,urllib.request
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'.build';BUILD.mkdir(exist_ok=True)
lock=json.loads((ROOT/'packaging/h700-dependencies.json').read_text())
for key,folder in [('sdl_image','SDL2_image-2.8.2'),('freetype','freetype-VER-2-13-3'),('sdl_ttf','SDL2_ttf-2.22.0')]:
 spec=lock[key];archive=BUILD/(spec.get('filename') or spec['url'].rsplit('/',1)[1])
 if not archive.exists():
  with urllib.request.urlopen(spec['url']) as response:archive.write_bytes(response.read())
 if hashlib.sha256(archive.read_bytes()).hexdigest()!=spec['sha256']:raise SystemExit(f'Checksum mismatch: {archive}')
 if not (BUILD/folder).exists():
  with tarfile.open(archive) as source:source.extractall(BUILD,filter='data')
source=BUILD/'source'
if source.exists():shutil.rmtree(source)
source.mkdir()
for name in ['src','bin','make']:
 shutil.copytree(ROOT/name,source/name,ignore=lambda directory,names:[n for n in names if (Path(directory)/n).is_file() and (n=='brogue' or n.endswith(('.o','.broguesave','.broguerec')))])
for name in ['Makefile','config.mk','LICENSE.txt']:shutil.copy2(ROOT/name,source/name)
script=r'''set -eu
cd /src
prefix=/src/.build/prefix
toolchain=/src/packaging/h700.cmake
cmake -S .build/SDL2_image-2.8.2 -B .build/image-build -DCMAKE_TOOLCHAIN_FILE=$toolchain -DCMAKE_BUILD_TYPE=Release -DSDL2_DIR=/opt/nextui/lib/cmake/SDL2 -DCMAKE_INSTALL_PREFIX=$prefix -DSDL2IMAGE_BACKEND_STB=ON -DSDL2IMAGE_SAMPLES=OFF -DSDL2IMAGE_TESTS=OFF -DSDL2IMAGE_VENDORED=OFF -DSDL2IMAGE_TIF=OFF -DSDL2IMAGE_WEBP=OFF -DBUILD_SHARED_LIBS=ON
cmake --build .build/image-build -j4
cmake --install .build/image-build
cmake -S .build/freetype-VER-2-13-3 -B .build/freetype-build -DCMAKE_TOOLCHAIN_FILE=$toolchain -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$prefix -DBUILD_SHARED_LIBS=ON -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
cmake --build .build/freetype-build -j4
cmake --install .build/freetype-build
cmake -S .build/SDL2_ttf-2.22.0 -B .build/ttf-build -DCMAKE_TOOLCHAIN_FILE=$toolchain -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$prefix -DCMAKE_INSTALL_PREFIX=$prefix -DSDL2_DIR=/opt/nextui/lib/cmake/SDL2 -DSDL2TTF_VENDORED=OFF -DSDL2TTF_HARFBUZZ=OFF -DSDL2TTF_SAMPLES=OFF -DBUILD_SHARED_LIBS=ON
cmake --build .build/ttf-build -j4
cmake --install .build/ttf-build
cd .build/source
make -j4 RELEASE=YES CC=aarch64-nextui-linux-gnu-gcc SDL_CONFIG=/opt/nextui/bin/sdl2-config CFLAGS=-I$prefix/include/SDL2 LDFLAGS=-L$prefix/lib LDLIBS=-Wl,-rpath-link,$prefix/lib
'''
subprocess.run(['docker','run','--rm','-v',f'{ROOT}:/src',lock['toolchain'],'sh','-c',script],check=True)
dist=BUILD/'dist';dist.mkdir(exist_ok=True)
shutil.copy2(source/'bin/brogue',dist/'brogue');shutil.copytree(source/'bin/assets',dist/'assets',dirs_exist_ok=True);shutil.copy2(source/'bin/keymap.txt',dist/'keymap.txt')
(dist/'lib').mkdir(exist_ok=True)
for name in ['libSDL2_image-2.0.so.0','libSDL2_ttf-2.0.so.0','libfreetype.so.6']:shutil.copy2(BUILD/'prefix/lib'/name,dist/'lib'/name)
licenses=dist/'licenses';licenses.mkdir(exist_ok=True)
for src,name in [(ROOT/'LICENSE.txt','Brogue-AGPL.txt'),(ROOT/'bin/assets/NotoSansCJK-OFL.txt','NotoSansCJK-OFL.txt'),(BUILD/'freetype-VER-2-13-3/docs/FTL.TXT','FreeType-FTL.txt'),(BUILD/'freetype-VER-2-13-3/LICENSE.TXT','FreeType-LICENSE.txt'),(BUILD/'SDL2_image-2.8.2/LICENSE.txt','SDL2_image-LICENSE.txt'),(BUILD/'SDL2_ttf-2.22.0/LICENSE.txt','SDL2_ttf-LICENSE.txt')]:shutil.copy2(src,licenses/name)
print('Built:',dist)

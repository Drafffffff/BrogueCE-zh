#!/usr/bin/env python3
"""Build a self-contained Apple Silicon app; never installs or replaces apps."""
from pathlib import Path
import argparse,hashlib,json,os,plistlib,re,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--from-app',type=Path);p.add_argument('--output',type=Path,default=ROOT/'dist/Brogue CE 中文.app');args=p.parse_args()
app=args.output.resolve();app.parent.mkdir(parents=True,exist_ok=True)
if app.exists():raise SystemExit('Output already exists; choose a new destination')
def run(*cmd):return subprocess.check_output(cmd,text=True)
if args.from_app:shutil.copytree(args.from_app,app)
else:
 subprocess.run(['make','-B','-j4','MAC_APP=YES','RELEASE=YES'],cwd=ROOT,check=True)
 (app/'Contents/MacOS').mkdir(parents=True);res=app/'Contents/Resources';res.mkdir()
 shutil.copy2(ROOT/'bin/brogue',app/'Contents/MacOS/brogue')
 shutil.copytree(ROOT/'bin/assets',res/'assets');shutil.copy2(ROOT/'bin/keymap.txt',res/'keymap.txt')
 shutil.copy2(ROOT/'macos/Info.plist',app/'Contents/Info.plist')
 iconset=app.parent/'Brogue.iconset';iconset.mkdir()
 for size in [16,32,64,128,256,512]:
  for scale in [1,2]:
   if size*scale>1024:continue
   subprocess.run(['sips','-z',str(size*scale),str(size*scale),str(ROOT/'bin/assets/icon.png'),'--out',str(iconset/f'icon_{size}x{size}{"@2x" if scale==2 else ""}.png')],check=True,stdout=subprocess.DEVNULL)
 subprocess.run(['iconutil','-c','icns',str(iconset),'-o',str(res/'Brogue.icns')],check=True);shutil.rmtree(iconset)
frameworks=app/'Contents/Frameworks';frameworks.mkdir(exist_ok=True)
exe=app/'Contents/MacOS/brogue';copied={};origins={};queue=[exe]
def dependencies(path):return [line.strip().split(' (compatibility')[0] for line in run('otool','-L',str(path)).splitlines()[1:]]
def add(original,name=None):
 original=Path(original);name=name or original.name
 if name not in copied:
  target=frameworks/name;shutil.copy2(original.resolve(),target);target.chmod(0o755)
  copied[name]=target;origins[name]=original.resolve();queue.append(target)
 return name
# SDL2-compat loads SDL3 at runtime; @loader_path/libSDL3.dylib is a supported lookup.
if any('sdl2-compat' in d for d in dependencies(exe)):
 prefix=Path(run('brew','--prefix','sdl3').strip());add(prefix/'lib/libSDL3.dylib','libSDL3.dylib')
index=0
while index<len(queue):
 binary=queue[index];index+=1
 for dep in dependencies(binary):
  if dep.startswith('/System/') or dep.startswith('/usr/lib/'):continue
  if dep.startswith('@executable_path/../Frameworks/'):continue
  if dep.startswith('@rpath/') or dep.startswith('@loader_path/'):
   source=exe if binary==exe else origins[binary.name]
   candidates=[source.parent/dep.split('/',1)[1],Path('/opt/homebrew/lib')/Path(dep).name]
   original=next((f for f in candidates if f.exists()),None)
   if original is None:raise SystemExit('Unresolved dependency: '+dep)
  elif dep.startswith('/'):original=Path(dep)
  else:raise SystemExit('Unresolved dependency: '+dep)
  # A dylib reports its own install name; do not create a duplicate alias.
  if binary!=exe and original.resolve()==origins[binary.name]:continue
  name=add(original)
  subprocess.run(['install_name_tool','-change',dep,'@executable_path/../Frameworks/'+name,str(binary)],check=True,stderr=subprocess.DEVNULL)
 if binary!=exe:subprocess.run(['install_name_tool','-id','@executable_path/../Frameworks/'+binary.name,str(binary)],check=True,stderr=subprocess.DEVNULL)
licenses=app/'Contents/Resources/licenses';licenses.mkdir(exist_ok=True)
shutil.copy2(ROOT/'LICENSE.txt',licenses/'Brogue-AGPL.txt')
shutil.copy2(ROOT/'bin/assets/NotoSansCJK-OFL.txt',licenses/'NotoSansCJK-OFL.txt')
for original in origins.values():
 parts=original.parts
 if 'Cellar' not in parts:continue
 idx=parts.index('Cellar');formula=parts[idx+1];prefix=Path(*parts[:idx+3])
 for file in prefix.rglob('*'):
  if file.is_file() and len(file.relative_to(prefix).parts)<=6 and re.match(r'(?i)(license|copying|notice|ofl|ftl)([._-]|$)',file.name):
   dest=licenses/formula/file.relative_to(prefix);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,dest)
minimum=(11,0)
for binary in queue:
 for ver in re.findall(r'\bminos\s+([0-9.]+)',run('otool','-l',str(binary))):minimum=max(minimum,tuple(map(int,ver.split('.'))))
info=plistlib.loads((app/'Contents/Info.plist').read_bytes());info.update(CFBundleName='Brogue CE 中文',CFBundleDisplayName='Brogue CE 中文',LSMinimumSystemVersion='.'.join(map(str,minimum)))
(app/'Contents/Info.plist').write_bytes(plistlib.dumps(info))
for binary in queue:subprocess.run(['codesign','--force','--sign','-',str(binary)],check=True,stderr=subprocess.DEVNULL)
subprocess.run(['codesign','--force','--sign','-',str(app)],check=True,stderr=subprocess.DEVNULL)
subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True)
for binary in queue:
 assert not any(d.startswith('/opt/') or d.startswith('/Users/') for d in dependencies(binary)),binary
manifest=dict(platform='macOS arm64',minimum_macos=info['LSMinimumSystemVersion'],self_contained=True,notarized=False,libraries=sorted(copied),binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
# Write outside the signed bundle.
(app.parent/'macOS-build.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(manifest,ensure_ascii=False,indent=2))

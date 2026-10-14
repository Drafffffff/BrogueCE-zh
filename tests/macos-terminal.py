#!/usr/bin/env python3
"""Run an isolated native terminal game in real PTYs; requires pyte."""
from pathlib import Path
import os,pty,subprocess,fcntl,termios,struct,time,select,tempfile,json,sys,codecs
import pyte
binary=Path(sys.argv[1]).resolve();output=Path(sys.argv[2]).resolve();output.mkdir(parents=True,exist_ok=True)
results=[]
for name,term,color in [('16','xterm',''),('256','xterm-256color',''),('truecolor','xterm-256color','truecolor')]:
    with tempfile.TemporaryDirectory(prefix='brogue-terminal-') as state:
        master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',40,120,0,0))
        env=dict(os.environ,TERM=term,COLORTERM=color,LANG='en_US.UTF-8',LC_ALL='en_US.UTF-8',BROGUE_LANG='zh')
        p=subprocess.Popen([str(binary),'--data-dir',str(binary.parent)],cwd=state,env=env,stdin=slave,stdout=slave,stderr=slave,start_new_session=True);os.close(slave)
        screen=pyte.Screen(120,40);stream=pyte.Stream(screen);decoder=codecs.getincrementaldecoder('utf-8')('strict');raw=bytearray()
        def read(seconds):
            until=time.monotonic()+seconds
            while time.monotonic()<until:
                if select.select([master],[],[],.05)[0]:
                    try:data=os.read(master,65536)
                    except OSError:break
                    raw.extend(data);stream.feed(decoder.decode(data))
        def snap(stage):
            text='\n'.join(screen.display);(output/f'{name}-{stage}.txt').write_text(text);return text
        try:
            read(2);title=snap('title');assert '新的冒险' in title,title
            os.write(master,b'n');read(2);dungeon=snap('dungeon');assert '@' in dungeon
            os.write(master,b'i');read(.6);inventory=snap('inventory');assert '匕首' in inventory,inventory
            os.write(master,b'\x1b');read(.3)
            os.write(master,b'S');read(.5);os.write(master,b'\r');read(.5)
            saved=list(Path(state).glob('*.broguesave'));assert saved,'No save created'
            results.append(dict(mode=name,title_chinese=True,inventory_chinese=True,save_bytes=saved[0].stat().st_size))
        finally:
            if p.poll() is None:p.terminate()
            try:p.wait(timeout=3)
            except subprocess.TimeoutExpired:p.kill();p.wait()
            os.close(master);(output/f'{name}.ansi').write_bytes(raw)
(output/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n');print(json.dumps(results))

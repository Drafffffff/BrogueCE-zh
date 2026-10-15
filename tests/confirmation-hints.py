#!/usr/bin/env python3
"""Run an isolated native terminal game in real PTYs; requires pyte."""
from pathlib import Path
import os,pty,subprocess,fcntl,termios,struct,time,select,tempfile,json,sys,codecs
import pyte
binary=Path(sys.argv[1]).resolve();output=Path(sys.argv[2]).resolve();output.mkdir(parents=True,exist_ok=True)
results=[]
for name,term,color in [('truecolor','xterm-256color','truecolor')]:
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
            os.write(master,b'Q');read(.6);prompt=snap('confirm')
            assert '是 (Y)' in prompt and '否 (N)' in prompt,prompt
            os.write(master,b'n');read(.5);assert '是 (Y)' not in snap('cancel-N')
            os.write(master,b'Q');read(.4);os.write(master,b'\x1b');read(.6);assert '是 (Y)' not in snap('cancel-Esc')
            os.write(master,b'Q');read(.4);os.write(master,b'Y');read(.8);assert '是 (Y)' not in snap('accept-Y')
            results.append(dict(mode=name,yes_hint=True,no_hint=True,N_cancel=True,Esc_cancel=True,Y_accept=True))

        finally:
            if p.poll() is None:p.terminate()
            try:p.wait(timeout=3)
            except subprocess.TimeoutExpired:p.kill();p.wait()
            os.close(master);(output/f'{name}.ansi').write_bytes(raw)
(output/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n');print(json.dumps(results))

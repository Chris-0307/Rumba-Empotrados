#!/usr/bin/python3
import os,sys,signal
from pathlib import Path
root=Path(os.environ['ALERT_TEST_ROOT']);marker=root/'music-open';marker.write_text(str(os.getpid()))
def log(s):
 with (root/'commands.log').open('a') as f:f.write(s+'\n')
def stop(*args):
 marker.unlink(missing_ok=True);sys.exit(0)
signal.signal(signal.SIGTERM,stop)
print('@R MPG123 test',flush=True);state=0
for line in sys.stdin:
 line=line.strip();log(line)
 if line.startswith('VOLUME '):print('@V '+line.split()[1]+'%',flush=True)
 elif line.startswith('LOADPAUSED '):state=1;print('@P 1',flush=True)
 elif line.startswith('LOAD '):state=2;print('@F 42 500 1.0 10.0\n@P 2',flush=True)
 elif line=='PAUSE':state=1 if state==2 else 2;print('@P '+str(state),flush=True)
 elif line.startswith('JUMP '):print('@J '+line.split()[1],flush=True)
stop()

#!/usr/bin/python3
import os,sys,time
from pathlib import Path
root=Path(os.environ['ALERT_TEST_ROOT'])
assert not (root/'music-open').exists(), 'mpg123 did not release ALSA'
assert sys.argv[1:3]==['-q','-D']
assert sys.argv[3]=='test-device'
with (root/'alerts.log').open('a') as f:f.write(Path(sys.argv[4]).name+'\n')
(root/'effect-active').write_text('1')
time.sleep(10 if os.environ.get('ALERT_TEST_HANG') else .25)
(root/'effect-active').unlink(missing_ok=True)
sys.exit(1 if os.environ.get('ALERT_TEST_FAIL') else 0)

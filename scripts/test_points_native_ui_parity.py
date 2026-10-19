#!/usr/bin/env python3
"""Exact app selector-off tokens and actual MONO1 selector-on/off frame parity."""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE='fe612078f486dfa67a654f0f7ef09ac3fee9382e'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--system',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True)
p.add_argument('--output',type=Path,default=ROOT/'build/points-native-ui-parity');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);system=a.system.resolve();utilities=a.utilities.resolve()
# Legacy paper fixture intentionally qualifies the retained untagged API1 path.
incs=[system/'lib/PortableApps/include',utilities/'lib/Alarm/include',system/'lib/NativeApps/include',system/'Apps',ROOT/'Apps']
base=out/'points_in_time.c';base.write_bytes(subprocess.check_output(['git','show',BASE+':Apps/points_in_time.c'],cwd=ROOT))
cc=os.environ.get('CC','cc');rows=[]
for nova in [False,True]:
 for alarm in [False,True]:
  for ret in [False,True]:
   flags=(['PORTABLE_NOVA_UI'] if nova else [])+(['PORTABLE_ALARM_CLIENT'] if alarm else [])+(['POINTS_RETURN_APP="springboard.elf"'] if ret else [])
   values=[]
   for path in [base,ROOT/'Apps/points_in_time.c']:
    text=subprocess.check_output([cc,'-E','-P',*['-D'+x for x in flags],*['-I'+str(x) for x in incs],str(path)],text=True)
    values.append('\n'.join(re.findall(r'''"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_0-9]+|[^\s]''',text)))
   assert values[0]==values[1],flags
   assert 'portable_points_native_' not in values[1]
   rows.append({'defines':flags,'token_sha256':hashlib.sha256(values[0].encode()).hexdigest()})
framesets=[]
for enabled in [False,True]:
 for sanitized in [False,True]:
  folder=out/f'paper-{int(enabled)}-{int(sanitized)}';folder.mkdir(exist_ok=True)
  flags=['PORTABLE_NOVA_UI','PORTABLE_DISPLAY_ROTATION=90']
  if enabled:flags+=['PORTABLE_POINTS_NATIVE_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_APP_OWNS_TOUCH_CHROME']
  san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  exe=folder/'test';subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*san,'-include',str(system/'lib/PortableApps/include/AlarmServiceV1.h'),*['-D'+x for x in flags],*['-I'+str(x) for x in incs],str(ROOT/'test/native_apps/points_paper_test.c'),str(system/'lib/PortableApps/src/adapter.c'),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','POINTS_PAPER_FRAMES':str(folder)})
  (folder/'test.log').write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');result.check_returncode()
  hashes={x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in folder.glob('*.pbm')};assert hashes
  framesets.append(hashes)
assert all(x==framesets[0] for x in framesets)
(out/'results.json').write_text(json.dumps({'baseline':BASE,'selector_off':rows,'paper_frame_hashes':framesets[0],'paper_compositions':4},indent=2)+'\n')
print(f'PASS: {len(rows)} exact app feature-off token profiles; {len(framesets[0])} actual MONO1 frames identical across selector off/on and normal/sanitized')

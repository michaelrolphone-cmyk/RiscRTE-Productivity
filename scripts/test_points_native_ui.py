#!/usr/bin/env python3
"""Execute actual Points app, native raster, raw input and production adapter."""
import argparse, json, os, subprocess
from pathlib import Path
from build_points_native_utc import stage_headers, HELPERS
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','utilities','runtime'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--output',type=Path,default=ROOT/'build/points-native-ui')
p.add_argument('--quick',action='store_true');p.add_argument('--snapshot',action='store_true');p.add_argument('--sanitized',action='store_true')
p.add_argument('--full-frames',action='store_true');p.add_argument('--scenario',type=int);p.add_argument('--size');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
system=a.system.resolve();includes=stage_headers(out,system,system,a.utilities.resolve(),a.runtime.resolve())
defines=['PORTABLE_POINTS_NATIVE_UI','PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_RETAINED_RGB565_HANDOFF','ALARM_NATIVE_UTC','ALARM_SERVICE_TAGGED_V2','PORTABLE_ALARM_CLIENT','PORTABLE_ALARM_NATIVE','PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_INPUT_NAVIGATION','POINTS_RETURN_APP="springboard.elf"','PORTABLE_HOME_APP="default.elf"']
if a.full_frames:defines+=['PORTABLE_FORCE_FULL_FRAMES']
if a.quick:defines+=['PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_NATIVE']
if a.snapshot:defines+=['PORTABLE_RASTER_SNAPSHOT']
flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitized else []
sources=[ROOT/'test/native_apps/points_native_ui_test.c',system/'lib/PortableApps/src/adapter.c',*[system/'lib/PortableApps/src'/x for x in HELPERS]]
if a.quick:sources += [system/'lib/PortableApps/src'/x for x in ('quick_actions.c','quick_render.c','quick_session.c')]
exe=out/'test';cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-D'+s for s in defines],*['-I'+str(s) for s in includes],*map(str,sources),'-o',str(exe)]
(out/'command.json').write_text(json.dumps(cmd,indent=2)+'\n');subprocess.run(cmd,check=True)
frames=out/'frames';frames.mkdir(exist_ok=True)
passed=[]
for size in [a.size] if a.size else ['160x160','240x240','240x280','320x240','410x502']:
 for scenario in [a.scenario] if a.scenario is not None else range(10 if a.quick else 9):
  cmd=[str(exe),*size.split('x'),str(scenario)]
  result=subprocess.run(cmd,text=True,capture_output=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1',**({'POINTS_NATIVE_FRAMES':str(frames)} if not a.sanitized else {})})
  (out/f'{size}-{scenario}.log').write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode();passed.append([size,scenario])
(out/'results.json').write_text(json.dumps({'full_frames':a.full_frames,'quick':a.quick,'snapshot':a.snapshot,'sanitized':a.sanitized,'passed':passed,'hardware_verified':False},indent=2)+'\n')

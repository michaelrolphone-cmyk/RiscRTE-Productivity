#!/usr/bin/env python3
"""Actual Points/Timecard, native adapter and MONO1 scrolling regression matrix."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
import native_paper_transitions as motion
import build_points_native_utc as points
import build_timecard_native_time as timecard
CASES='drag bounds tap busy-hit busy-drag stop-tap queued-up cancelled replaced reordered deleted changed horizontal footer-drag fit keyboard editor-list keyboard-busy back modal confirm-hidden'.split()
p=argparse.ArgumentParser()
for name in ('system-apps','adapter-system','utilities','runtime'):p.add_argument('--'+name,type=Path,required=True)
motion.options(p);p.add_argument('--case',choices=CASES,action='append');p.add_argument('--normal-only',action='store_true');p.add_argument('--app',choices=['points','timecard'],action='append');p.add_argument('--flip',action='store_true');a=p.parse_args()
if not a.touch_scrolling:p.error('--touch-scrolling required')
system,adapter,utilities,runtime=(x.resolve() for x in (a.system_apps,a.adapter_system,a.utilities,a.runtime));out=points.ROOT/'build/productivity-scroll';out.mkdir(parents=True,exist_ok=True)
results=[];commands=[];source_paths=set()
for name in a.app or ['points','timecard']:
 b=points if name=='points' else timecard;b.verify(system,adapter,utilities,runtime)
 selected,defines,_,receipt=motion.select(a,p,adapter,b.DEFINES);includes=b.stage_headers(out/name,system,selected,utilities,runtime)
 defines+=['-DTEST_PRODUCTIVITY_SCROLL_FIXTURE','-DTEST_PAPER_SHEET_MOTION']+(['-DTEST_POINTS_SCROLL'] if name=='points' else [])
 sources=[points.ROOT/'test/native_apps/productivity_scroll_test.c',selected/'lib/PortableApps/src/adapter.c',*[system/'lib/PortableApps/src'/s for s in b.HELPERS],*[selected/'lib/PortableApps/src'/s for s in (['PortableNativeTimeSource.c'] if name=='timecard' else [])+['quick_actions.c','quick_render.c','quick_session.c']]]
 for san in [False] if a.normal_only else [False,True]:
  exe=out/f'{name}-{int(san)}';flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*defines,*flags,*['-I'+str(x) for x in includes],*map(str,sources),'-o',str(exe)]
  subprocess.run(command,check=True);commands.append(command);source_paths.update(sources)
  for case in a.case or CASES:
   dest=out/(name+('-flip' if a.flip else '')+'-frames')/case;dest.mkdir(parents=True,exist_ok=True)
   run=subprocess.run([str(exe),case,*([str(dest)] if not san else [])],check=True,stdout=subprocess.PIPE,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0',**({'TEST_PAPER_FLIP':'1'} if a.flip else {})},timeout=30)
   print(name,san,case,run.stdout.strip(),flush=True);results.append({'app':name,'sanitized':san,'case':case,'flip':a.flip})
source_paths.update(path for folder in ('Apps','test/native_apps') for path in (points.ROOT/folder).glob('*') if path.suffix in ('.c','.h','.inc'))
(out/('receipt-flip.json' if a.flip else 'receipt.json')).write_text(json.dumps({'runs':results,'system':receipt,'hardware_verified':False,'source_commit':subprocess.check_output(['git','-C',str(points.ROOT),'rev-parse','HEAD'],text=True).strip(),'source_sha256':{str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(source_paths)},'compile_commands':commands},indent=2)+'\n')

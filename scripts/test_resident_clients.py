#!/usr/bin/env python3
"""Run real selected controllers and shared adapter from target-build receipts."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--app',action='append',choices=['points_in_time','timecard']);a=p.parse_args()
out=ROOT/'build/resident-clients';out.mkdir(parents=True,exist_ok=True);runs=[]
for name,fixture,cases in [('points_in_time','resident_points_test.c',['draft','uncertain','home','failure','preservation','editor-navigation','text-input','text-retention','text-frame','text-abandoned',*[f'text-runtime-{i}' for i in range(1,7)],'types-scroll']),('timecard','resident_timecard_test.c',['draft','uncertain','home','failure',*[str(i) for i in range(9)],*[str(i) for i in range(10,23)],'26','27','28','29'])]:
 if a.app and name not in a.app:continue
 r=json.loads((a.build/name/'x4-native-app.json').read_text());cmd=r['compile_command'];flags=[s for s in cmd if s.startswith(('-D','-I'))]
 src=[s for s in cmd if s.endswith('.c') and Path(s).name not in ('catalog.c','points_catalog_app.c','timecard_portable.c')]
 target=out/(name+('-san' if a.sanitize else ''))
 san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
 command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*san,*flags,'-DTEST_RESIDENT_CLIENT','-I'+str(a.utilities/'test/native_apps'),ROOT/'test/native_apps'/fixture,*src,'-o',target]
 subprocess.run(list(map(str,command)),check=True)
 for case in cases:
  subprocess.run([str(target),case],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  runs.append({'app':name,'case':case,'sanitized':a.sanitize})
(out/('test-evidence-san.json' if a.sanitize else 'test-evidence.json')).write_text(json.dumps(runs,indent=2)+'\n')

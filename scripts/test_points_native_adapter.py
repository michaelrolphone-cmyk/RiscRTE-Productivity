#!/usr/bin/env python3
"""Actual Points app + realtime helper + System native toolbar/Quick adapter."""
import argparse,os,subprocess
from pathlib import Path
import native_paper_transitions as motion
from build_points_native_utc import ROOT,DEFINES,HELPERS,verify,stage_headers
p=argparse.ArgumentParser()
for name in ('system-apps','adapter-system','utilities','runtime'):p.add_argument('--'+name,type=Path,required=True)
motion.options(p)
a=p.parse_args();system,adapter,utilities,runtime=(x.resolve() for x in (a.system_apps,a.adapter_system,a.utilities,a.runtime))
verify(system,adapter,utilities,runtime)
adapter,defines,_,_=motion.select(a,p,adapter,DEFINES)
if a.paper_transitions:defines.append("-DTEST_PAPER_SHEET_MOTION")
out=ROOT/'build'/('points-native-adapter-broadcast' if a.ble_broadcast else 'points-native-adapter-paper-transitions' if a.paper_transitions else 'points-native-adapter');out.mkdir(parents=True,exist_ok=True);includes=stage_headers(out,system,adapter,utilities,runtime)
sources=[ROOT/'test/native_apps/points_native_adapter_test.c',adapter/'lib/PortableApps/src/adapter.c',*[system/'lib/PortableApps/src'/x for x in HELPERS],*[adapter/'lib/PortableApps/src'/x for x in ('quick_actions.c','quick_render.c','quick_session.c')]]
for sanitized in (False,True):
 exe=out/f'test-{int(sanitized)}';flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*defines,*flags,*['-I'+str(x) for x in includes],*map(str,sources),'-o',str(exe)],check=True)
 for scenario in [*range(11 if a.ble_broadcast else 9), *range(100,108)] if a.paper_transitions else range(9):subprocess.run([str(exe),str(scenario)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 if a.ble_broadcast:
  for scenario in range(100,108):
   subprocess.run([str(exe),str(scenario)],check=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','TEST_PAPER_FLIP':'1'})
print('Actual native Points/controller/helper/toolbar composition with Quick enabled passed normal + ASan/UBSan')

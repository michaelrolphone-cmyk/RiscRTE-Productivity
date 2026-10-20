#!/usr/bin/env python3
"""Execute actual Timecard controller/native raster/ordered input/production adapter."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
from build_timecard_native_ui import stage_headers, selected_defines, SOURCES, UTC_SOURCES
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('system-apps','utilities','runtime'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--output',type=Path,default=ROOT/'build/timecard-native-ui')
for flag in ('utc','snapshot','sanitized','capture'):p.add_argument('--'+flag,action='store_true')
p.add_argument('--scenario',type=int);p.add_argument('--size');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
roots={'productivity':ROOT,'system':a.system_apps.resolve(),'utilities':a.utilities.resolve(),'runtime':a.runtime.resolve()}
includes=stage_headers(out,roots);includes += [ROOT/'lib/NativeApps/include']
defines=selected_defines(a.utc,a.snapshot)
flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitized else []
sources=[ROOT/'test/native_apps/timecard_native_ui_test.c',*[roots['system']/'lib/PortableApps/src'/x for x in (*SOURCES,*(UTC_SOURCES if a.utc else ()))]]
exe=out/'test';cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-D'+s for s in defines],*['-I'+str(s) for s in includes],*map(str,sources),'-o',str(exe)]
(out/'command.json').write_text(json.dumps(cmd,indent=2)+'\n');subprocess.run(cmd,check=True)
frames=out/'frames';frames.mkdir(exist_ok=True);passed=[]
for size in [a.size] if a.size else ['160x160','240x240','240x280','320x240','410x502']:
 for scenario in [a.scenario] if a.scenario is not None else range(13):
  for latency in (0,100):
   result=subprocess.run([str(exe),*size.split('x'),str(scenario),str(latency)],text=True,capture_output=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1',**({'TIMECARD_NATIVE_FRAMES':str(frames)} if a.capture and not a.sanitized and latency==0 else {})})
   (out/f'{size}-{scenario}-{latency}.log').write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='',flush=True);result.check_returncode();passed.append([size,scenario,latency])
(out/'results.json').write_text(json.dumps({'utc':a.utc,'snapshot':a.snapshot,'sanitized':a.sanitized,'passed':passed,'hardware_verified':False,'rasters':{x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in frames.glob('*.ppm')}},indent=2)+'\n')

#!/usr/bin/env python3
"""Tagged API2 paper regression on real UTC Points + current production adapter."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from build_points_native_utc import stage_headers,DEFINES,HELPERS
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','utilities','runtime','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();system=a.system.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
includes=stage_headers(out,system,system,a.utilities.resolve(),a.runtime.resolve());includes.append(ROOT/'test/native_apps')
source=(ROOT/'test/native_apps/points_native_adapter_test.c').read_text().replace('#include "../../Apps/points_in_time.c"','#include '+json.dumps(str(ROOT/'Apps/points_in_time.c')))
needle='frames=0;*t=++presents;'
assert needle in source
source=source.replace(needle,needle+'''const char*dir=getenv("PAPER_UTC_FRAMES");if(dir){char file[512];snprintf(file,sizeof(file),"%s/frame-%u.pbm",dir,presents);FILE*f=fopen(file,"wb");assert(f);fprintf(f,"P4\\n800 480\\n");assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));assert(!fclose(f));}''')
fixture=out/'fixture.c';fixture.write_text(source);records=[];reference={}
for enabled in (False,True):
 for snapshot in (False,True):
  for sanitized in (False,True):
   label=f'selector{int(enabled)}-snapshot{int(snapshot)}-sanitized{int(sanitized)}';folder=out/label;folder.mkdir(exist_ok=True)
   flags=[*DEFINES]
   if enabled:flags += ['-DPORTABLE_POINTS_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RETAINED_RGB565_HANDOFF']
   if snapshot:flags+=['-DPORTABLE_RASTER_SNAPSHOT']
   if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   sources=[fixture,system/'lib/PortableApps/src/adapter.c',*[system/'lib/PortableApps/src'/x for x in (*HELPERS,'quick_actions.c','quick_render.c','quick_session.c')]]
   exe=folder/'test';cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(x) for x in includes],*map(str,sources),'-o',str(exe)]
   (folder/'command.json').write_text(json.dumps(cmd,indent=2)+'\n');subprocess.run(cmd,check=True)
   for scenario in range(9):
    frames=folder/str(scenario);frames.mkdir(exist_ok=True)
    r=subprocess.run([str(exe),str(scenario)],text=True,capture_output=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1','PAPER_UTC_FRAMES':str(frames)})
    (frames/'test.log').write_text(r.stdout+r.stderr);r.check_returncode()
    hashes={x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in frames.glob('*.pbm')};assert hashes
    if scenario not in reference:reference[scenario]=hashes
    assert hashes==reference[scenario],(label,scenario)
    records.append([label,scenario])
   print(label+' PASS',flush=True)
(out/'results.json').write_text(json.dumps({'passed':records,'identical_frame_hashes':reference},indent=2)+'\n')
print('PASS: 72 tagged UTC paper executions; selector on/off, direct/snapshot, normal/sanitized identical frames')

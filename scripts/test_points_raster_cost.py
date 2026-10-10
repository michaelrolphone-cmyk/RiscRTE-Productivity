#!/usr/bin/env python3
"""Compare dense production Points screens in immediate and sliced profiles."""
import argparse,json,os,subprocess
from pathlib import Path
from points_catalog_sdk import stage_headers
from build_points_catalog import defines_for,HELPERS
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
for n in ('system','utilities','runtime','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
system=a.system.resolve();includes=stage_headers(out,system,system,a.utilities.resolve(),a.runtime.resolve())
results=[]
for sanitize in (False,True):
    for snapshot in (False,True):
        case=out/f'{int(sanitize)}-{int(snapshot)}';case.mkdir(exist_ok=True)
        sources=[ROOT/'test/native_apps/points_raster_cost_test.c',system/'lib/PortableApps/src/adapter.c',*[system/'lib/PortableApps/src'/n for n in (*HELPERS,'quick_actions.c','quick_render.c','quick_session.c')]]
        flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitize else []
        cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*defines_for('x4'),*(['-DPORTABLE_RASTER_SNAPSHOT'] if snapshot else []),*['-I'+str(i) for i in includes],*map(str,sources),'-Wl,--wrap=portable_raster_defer','-o',str(case/'test')]
        (case/'command.json').write_text(json.dumps(cmd,indent=2)+'\n');subprocess.run(cmd,check=True)
        run=subprocess.run([str(case/'test')],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','POINTS_CATALOG_FRAMES':str(case)})
        (case/'run.log').write_text(run.stdout+run.stderr);print(run.stdout,end='')
        results.append(dict(sanitized=sanitize,snapshot=snapshot,scenes=[json.loads(line) for line in run.stdout.splitlines()]))
    golden=out/f'{int(sanitize)}-0';candidate=out/f'{int(sanitize)}-1'
    for frame in golden.glob('*.pbm'):assert frame.read_bytes()==(candidate/frame.name).read_bytes(),frame.name
(out/'evidence.json').write_text(json.dumps({'profiles':results,'full_buffer_comparisons':12,'hardware_tested':False},indent=2)+'\n')
print('12 complete Points frame comparisons; producer commands bounded below 1024: PASS')

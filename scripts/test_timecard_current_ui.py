#!/usr/bin/env python3
"""Current owning UI composition; provider doubles, not actual Runtime/device."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
import build_timecard_current as recipe
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['system-apps','runtime','utilities','drivers','output']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true');p.add_argument('--capture',action='store_true');p.add_argument('--syntax-only',action='store_true')
 p.add_argument('--scenario',type=int);p.add_argument('--size',action='append');a=p.parse_args()
 args=argparse.Namespace(system_apps=a.system_apps,runtime=a.runtime,utilities=a.utilities,foreground_idle_helper_dir=a.drivers/'components/portable-light-sleep',foreground_idle_drivers_sdk_dir=a.drivers/'sdk/driver',foreground_idle_fallback_seconds=15,output_dir=a.output,compiler=None)
 out,system,toolchain,manifest,production,flags,record=recipe.prepare(args,p)
 fixture=ROOT/'test/native_apps/timecard_current_ui_test.c'
 # Fixture includes the actual app source and owns catalog/provider doubles;
 # all other production source/define/include selections stay exact.
 sources=[fixture,*[s for s in production if s.name not in ('timecard_portable.c','catalog.c')]]
 ci=record['compile_inputs'];flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*ci['definitions'],*['-I'+v for v in ci['include_directories']]]
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 consumed={}
 if not a.syntax_only:
  for source in sources:
   dependencies=subprocess.check_output([os.environ.get('CC','cc'),*flags,'-MM',str(source)],text=True).replace('\\\n',' ').split(':',1)[1].split()
   for path in dependencies:
    source_path=Path(path).resolve();consumed[str(source_path)]=recipe.sha(source_path)
  record['consumed_source_sha256']=consumed
 cmd=[os.environ.get('CC','cc'),*flags]
 if a.syntax_only:cmd+=['-fsyntax-only',str(fixture)]
 else:cmd += [*map(str,sources),'-o',str(out/'test')]
 (out/'command.json').write_text(json.dumps(cmd,indent=2)+'\n');subprocess.run(cmd,check=True)
 if a.syntax_only:
  (out/'SYNTAX-ONLY.txt').write_text('Fixture syntax only; no executed UI/scene/Quick qualification.\n');return
 for path,digest in consumed.items():
  if recipe.sha(path)!=digest:raise ValueError('Source changed during owning compile: '+path)
 frames=out/'frames';frames.mkdir();passed=[]
 for size in (a.size or ['240x240','320x320','466x466','410x502','502x410','160x160']):
  for scenario in ([a.scenario] if a.scenario is not None else range(14)):
   for latency in (0,100):
    r=subprocess.run([str((out/'test').resolve()),*size.split('x'),str(scenario),str(latency)],text=True,capture_output=True,timeout=90,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1',**({'TIMECARD_NATIVE_FRAMES':str(frames)} if a.capture and not a.sanitize and latency==0 else {})})
    (out/f'{size}-{scenario}-{latency}.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='',flush=True);r.check_returncode();passed.append([size,scenario,latency])
 record.update({'scope':'Actual app/shared adapter/scene/Quick and native helpers; provider tables mocked. Not target, Runtime, radio, physical or Quick write-path qualification.','sanitized':a.sanitize,'passed':passed,'fixture_sha256':recipe.sha(fixture),'runner_sha256':recipe.sha(__file__),'rasters':{f.name:recipe.sha(f) for f in frames.glob('*.ppm')}})
 for path,digest in consumed.items():
  if recipe.sha(path)!=digest:raise ValueError('Source changed during owning execution: '+path)
 (out/'results.json').write_text(json.dumps(record,indent=2)+'\n')
if __name__=='__main__':main()

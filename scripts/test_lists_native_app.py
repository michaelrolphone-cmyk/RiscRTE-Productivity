#!/usr/bin/env python3
"""Run shipped Lists app/host storage and lifecycle qualification (host only)."""
import argparse, hashlib, importlib.util, json, os, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SIZES=('160x160','240x240','320x180','410x502','502x410')
MODES=('workflow','fresh','retry-io','retry-corrupt','normal','cancel','back','home','checkpoint','checkpoint-busy','checkpoint-error','nonresident','policy',
       'clock-invalid','storage-io','storage-size','storage-revision','storage-corrupt','storage-stale',
       'save-io','save-stale','unknown-rejected','unknown-committed','reload-corrupt','reload-io','reload-stale','reload-size',
       'missing-1','missing-2','missing-3','open-error','configure-error','update-error','close-error','close-delay','release-error',
       'exit-idle','exit-busy','exit-edit')
SITES=('acquire-1','acquire-2','acquire-3','bind','stat','read','clock','open','configure','next','update','snapshot','close','release','checkpoint','reopen','replace','reload-stat','reload-read','exit')
def build(runtime,system,tmp,sanitize):
 spec=importlib.util.spec_from_file_location('scene_sdk',system/'scripts/scene_sdk.py');sdk=importlib.util.module_from_spec(spec);spec.loader.exec_module(sdk)
 include=sdk.stage_sdk(runtime,system,tmp)
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DSCENE_HOST_TELEMETRY=0','-DSCENE_HOST_WATCH_240=0','-I'+str(include),'-I'+str(system/'test/scene')]
 if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 exe=tmp/'app'
 sources=[ROOT/p for p in ('test/lists/native_probe.c','test/lists/native_app_test.c','Apps/ListsScene.c','lib/Lists/ListsModel.c','lib/Lists/ListsStore.c')]
 subprocess.run([os.environ.get('CC','cc'),*flags,'-DSCENE_HOST_SOURCE="'+str(system/'Services/scene_host/host.c')+'"',*map(str,sources),'-o',str(exe)],check=True)
 return exe

def run(args):
 args.output.mkdir(parents=True,exist_ok=True)
 def pin(path):
  return {'commit':subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip(),
          'tree':subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD^{tree}'],text=True).strip(),
          'tracked_status':subprocess.check_output(['git','-C',str(path),'status','--porcelain','--untracked-files=no'],text=True).splitlines()}
 source_files=[ROOT/p for p in ('Apps/lists_scene.c','Apps/ListsScene.c','Apps/ListsScene.h','lib/Lists/ListsModel.c','lib/Lists/ListsModel.h','lib/Lists/ListsStore.c','lib/Lists/ListsStore.h','test/lists/native_app_test.c','test/lists/native_probe.c','scripts/test_lists_native_app.py')]
 source_files += [args.system_apps/'Services/scene_host'/p for p in ('host.c','components.inc','keyboard.inc','keyboard_compact.inc','keyboard_paper.inc')]
 evidence={'physical_testing':'not performed','sanitized':args.sanitize,'scenarios':{},
           'sources':{'productivity':pin(ROOT),'system':pin(args.system_apps),'runtime':pin(args.runtime)},
           'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files}}
 with tempfile.TemporaryDirectory(prefix='lists-native-app-') as temp:
  exe=build(args.runtime.resolve(),args.system_apps.resolve(),Path(temp),args.sanitize)
  cases=args.cases.split(',') if args.cases else [*MODES,*['loss-'+p for p in SITES],*['retained-'+p for p in SITES if not p.startswith('acquire') and p not in ('bind','exit')]]
  for size in args.sizes.split(','):
   for mode in cases:
    key=size+'-'+mode;out=args.output/key;out.mkdir(exist_ok=True)
    result=subprocess.run([str(exe),size,str(out),mode],text=True,capture_output=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
    (out/'run.log').write_text(result.stdout+result.stderr);print(key,result.returncode,flush=True)
    if result.returncode:print(result.stdout+result.stderr,flush=True)
    result.check_returncode();hashes={}
    for file in out.iterdir():
     if file.suffix in ('.ppm','.bin'):
      hashes[file.name]=hashlib.sha256(file.read_bytes()).hexdigest()
      if file.suffix=='.ppm':
       if mode=='normal':
        from PIL import Image
        Image.open(file).save(file.with_suffix('.png'))
       file.unlink()
    evidence['scenarios'][key]=hashes
 (args.output/'results.json').write_text(json.dumps(evidence,indent=2)+'\n');print(len(evidence['scenarios']),'Lists app/host cases PASS')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('runtime','system-apps','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true');p.add_argument('--sizes',default=','.join(SIZES));p.add_argument('--cases');run(p.parse_args())

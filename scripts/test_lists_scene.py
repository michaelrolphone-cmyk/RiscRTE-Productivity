#!/usr/bin/env python3
import argparse,importlib.util,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(runtime,system,output=None):
 spec=importlib.util.spec_from_file_location('scene_sdk',system/'scripts/scene_sdk.py');sdk=importlib.util.module_from_spec(spec);spec.loader.exec_module(sdk)
 with tempfile.TemporaryDirectory(prefix='lists-test-') as temporary:
  tmp=Path(temporary);include=sdk.stage_sdk(runtime,system,tmp)
  flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(include)]
  sources=[str(ROOT/p) for p in ('lib/Lists/ListsModel.c','lib/Lists/ListsStore.c','Apps/ListsScene.c','test/lists/model_test.c')]
  subprocess.run([os.environ.get('CC','cc'),*flags,*sources,'-o',str(tmp/'model')],check=True);subprocess.run([str(tmp/'model')],check=True)
  app_sources=[str(ROOT/p) for p in ('lib/Lists/ListsModel.c','lib/Lists/ListsStore.c','Apps/ListsScene.c','Apps/lists_scene.c','test/lists/app_test.c')]
  subprocess.run([os.environ.get('CC','cc'),*flags,*app_sources,'-o',str(tmp/'app')],check=True)
  for mode in ('clean','close-loss','store-loss'):subprocess.run([str(tmp/'app'),mode],check=True)
  if output:
   output.mkdir(parents=True,exist_ok=True)
   render=[str(system/'Services/scene_host/host.c'),str(ROOT/'Apps/ListsScene.c'),str(ROOT/'lib/Lists/ListsModel.c'),str(ROOT/'test/lists/render_test.c')]
   subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(system/'test/scene'),*render,'-o',str(tmp/'render')],check=True)
   for profile in ('watch','paper'):subprocess.run([str(tmp/'render'),profile,str(output)],check=True)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--frames',type=Path);a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.frames)

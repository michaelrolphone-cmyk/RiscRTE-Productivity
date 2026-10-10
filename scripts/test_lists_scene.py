#!/usr/bin/env python3
import argparse,importlib.util,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def verify_completed(path,scale):
 # Reference empty state: 44 logical px circle around the completion check.
 header,dimensions,maximum,pixels=path.read_bytes().split(b'\n',3)
 width,height=map(int,dimensions.split());assert header==b'P6' and maximum==b'255'
 cx=120*scale;cy=(64+34+(48 if scale==2 else 44)+36)*scale
 bg=b'\xff'*3 if scale==2 else b'\0'*3
 def pixel(x,y):
  assert 0<=x<width and 0<=y<height
  return pixels[3*(y*width+x):3*(y*width+x)+3]
 for x,y in [(cx-22*scale,cy),(cx+22*scale-1,cy),(cx,cy-22*scale),(cx,cy+22*scale-1)]:
  assert pixel(x,y)!=bg, 'Completed icon is missing its circle: '+str(path)
 for x,y in [(cx-22*scale,cy-22*scale),(cx+22*scale-1,cy+22*scale-1)]:
  assert pixel(x,y)==bg, 'Completed outline must be circular'
 assert pixel(cx-2*scale,cy+5*scale)!=bg, 'Completion check is missing'

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
   for profile,scale in [('watch',1),('paper',2)]:
    subprocess.run([str(tmp/'render'),profile,str(output)],check=True)
    verify_completed(output/(profile+'-completed.ppm'),scale)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--frames',type=Path);a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.frames)

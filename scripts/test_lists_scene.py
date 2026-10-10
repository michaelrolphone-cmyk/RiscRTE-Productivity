#!/usr/bin/env python3
import argparse,importlib.util,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def verify_row_padding_and_dialogs(output,profile,scale):
 # The reference rows have 8 logical px of padding inside their left border.
 # Dialog pills have an outline and label, with no extra focus underline.
 bg=b'\xff'*3 if scale==2 else b'\0'*3
 def pixels(name):
  header,dimensions,maximum,data=(output/(profile+'-'+name+'.ppm')).read_bytes().split(b'\n',3)
  width,height=map(int,dimensions.split());assert header==b'P6' and maximum==b'255'
  def pixel(x,y):
   at=3*(y*scale*width+x*scale);return data[at:at+3]
  return pixel
 border=3 if scale==2 else 2
 header=60 if scale==2 else 84
 row=42 if scale==2 else 40
 # TODAY's symbol and RENAME's symbol must both clear the frame.
 for name,cy in [('home',header+88+row//2),('task',header+row+row//2)]:
  pixel=pixels(name)
  assert all(pixel(x,y)==bg for x in range(16+border,16+border+8) for y in range(cy-8,cy+8)), 'Row icon intrudes into the left padding: '+name
  assert any(pixel(x,y)!=bg for x in range(16+border+8,16+border+24) for y in range(cy-8,cy+8)), 'Row icon is missing: '+name
 # The first custom-list marker is visible below MY LISTS on X4.
 if scale==2:
  pixel=pixels('home');cy=header+88+2*row+28+row//2
  assert all(pixel(x,y)==bg for x in range(16+border,16+border+8) for y in range(cy-6,cy+6)), 'List marker intrudes into the left padding'
  assert any(pixel(x,y)!=bg for x in range(16+border+8,16+border+22) for y in range(cy-6,cy+6)), 'List marker is missing'
 middle=200 if scale==2 else 120
 for name,cx in [('confirm',75),('alert',182)]:
  pixel=pixels(name)
  assert all(pixel(x,middle+54)==bg for x in range(cx-10,cx+11)), 'Dialog button has an extra focus underline: '+name

def verify_tabs(path,scale,selected):
 # Mockup contract: a continuous outer rail; only the inset selected segment
 # is filled. An unselected tab must not have its own button outline.
 header,dimensions,maximum,pixels=path.read_bytes().split(b'\n',3)
 width,height=map(int,dimensions.split());assert header==b'P6' and maximum==b'255'
 top=(56 if scale==2 else 78)+46+8
 bg=b'\xff'*3 if scale==2 else b'\0'*3
 def pixel(x,y):
  at=3*(y*scale*width+x*scale);return pixels[at:at+3]
 assert pixel(120,top)!=bg, 'Tabs need one continuous outer rail'
 assert pixel(119,top+13)==bg, 'Tab selection needs an inset gap'
 for tab,x in enumerate([68,172]):
  assert (pixel(x,top+4)!=bg)==(tab==selected), 'Only the selected tab is filled'
 assert pixel(220 if selected==0 else 19,top+13)==bg, 'Inactive tab is not a separate outlined button'

def verify_completed(path,scale):
 # Reference empty state: 44 logical px circle around the completion check.
 header,dimensions,maximum,pixels=path.read_bytes().split(b'\n',3)
 width,height=map(int,dimensions.split());assert header==b'P6' and maximum==b'255'
 cx=120*scale;cy=((56 if scale==2 else 78)+46+38+44)*scale
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
    verify_row_padding_and_dialogs(output,profile,scale)
    verify_completed(output/(profile+'-completed.ppm'),scale)
    verify_tabs(output/(profile+'-list.ppm'),scale,0)
    verify_tabs(output/(profile+'-list-all.ppm'),scale,1)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--frames',type=Path);a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.frames)

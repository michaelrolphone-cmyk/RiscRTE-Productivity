#!/usr/bin/env python3
"""Build/test the same shared Timecard application for every scene provider."""
import argparse, hashlib, importlib.util, json, os, platform, shutil, subprocess, sys, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
 sys.path.insert(0,str(path.parent))
 try:
  spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
 finally:sys.path.pop(0)
def stage(runtime,system,tmp):
 sdk=module('scene_sdk',system/'scripts/scene_sdk.py')
 include=sdk.stage_sdk(runtime,system,tmp)
 for p in (ROOT/'lib/NativeApps/include').glob('T5*.h'):shutil.copyfile(p,include/p.name)
 shutil.copyfile(system/'lib/PortableApps/include/PortableTimeFormat.h',include/'PortableTimeFormat.h')
 return include
def run(runtime,system,output,build,frames=None):
 with tempfile.TemporaryDirectory(prefix='timecard-scene-') as temporary:
  tmp=Path(temporary);include=stage(runtime,system,tmp)
  flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(include)]
  variants=[[],['-fsanitize=undefined','-fno-sanitize-recover=all']]
  if platform.system()!='Darwin':variants.append(['-fsanitize=address,undefined','-fno-sanitize-recover=all'])
  for index,variant in enumerate(variants):
   binary=tmp/('test-'+str(index))
   subprocess.run([os.environ.get('CC','cc'),*flags,*variant,str(ROOT/'test/timecard_scene/app_test.c'),'-o',str(binary)],check=True)
   subprocess.run([str(binary)],check=True,timeout=60)
  if frames:
   frames.mkdir(parents=True,exist_ok=True)
   binary=tmp/'render'
   sources=[system/'Services/scene_host/host.c',ROOT/'Apps/timecard_scene.c',ROOT/'test/timecard_scene/render_test.c']
   subprocess.run([os.environ.get('CC','cc'),*flags,'-DTIMECARD_SCENE_FIXTURE','-I'+str(system/'test/scene'),*map(str,sources),'-o',str(binary)],check=True)
   for profile in ('watch','paper','amoled','round-badge'):subprocess.run([str(binary),profile,str(frames)],check=True,timeout=60)
  if not build:return
  if output.exists():raise FileExistsError('Choose a new output directory')
  builder=module('scene_build',system/'scripts/scene_build.py');compiler=builder.compiler_path()
  manifest=json.loads((ROOT/'Apps/timecard_scene.json').read_text())
  sources=[ROOT/'Apps/timecard_scene.c'];exports={'app_main','risc_resident_app_descriptor_v1'}
  receipt=builder.build(compiler,include,output/'timecard',manifest,sources,exports=exports)
  for profile in ('watch','eink','round-badge'):
   repeat=builder.build(compiler,include,tmp/profile,manifest,sources,exports=exports)
   if repeat['sha256']!=receipt['sha256']:raise ValueError('Application differs across provider profiles')
  validator=tmp/'validate'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(system/'test/native_apps/stubs'),'-I'+str(system/'lib/elf_loader/include'),str(system/'lib/elf_loader/src/esp_elf_validate.c'),str(system/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
  subprocess.run([str(validator),str(output/'timecard/timecard.elf')],check=True)
  receipt['model_source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('Apps/timecard.c','Apps/timecard_appdata_bridge.h','Apps/timecard_portable_validation.h')}
  receipt['pins']={name:subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip() for name,path in [('system',system),('runtime',runtime)]}
  builder.json_write(output/'timecard/build.json',receipt)
  builder.json_write(output/'packages.json',{'schema':1,'packages':[receipt],'byte_identical_profiles':['watch','eink','round-badge'],'physical_testing':'not performed'})
  print('Timecard shared Xtensa ELF validated; repeated builds are byte-identical.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True)
 p.add_argument('--output',type=Path,default=ROOT/'dist/timecard-scene');p.add_argument('--build',action='store_true')
 p.add_argument('--frames',type=Path)
 a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.output.resolve(),a.build,a.frames)

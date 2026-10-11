#!/usr/bin/env python3
"""Run the complete native NOVA Reader GUI against synthetic SD books."""
import argparse,json,subprocess,shutil,sys
from pathlib import Path
from reader_sources import ROOT,inputs
p=argparse.ArgumentParser()
for name in ('runtime','system','engine-output','output'):
    p.add_argument('--'+name,type=lambda x:Path(x).resolve(),required=True)
p.add_argument('--app-source',type=lambda x:Path(x).resolve(),default=ROOT/'reader/ReaderApp.cpp')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(a.system/'scripts'))
from scene_sdk import stage_sdk
sdk=stage_sdk(a.runtime,a.system,a.output/'sdk')
_,_,includes=inputs(a.engine_output/'upstream')
includes=[ROOT/'reader/port',ROOT/'reader',sdk,a.runtime/'sdk/cxx']+includes
objects=[]
for name,source in [('scene',a.system/'Services/scene_host/host.c'),('scene-fixture',ROOT/'test/reader/scene_startup.c')]:
    obj=a.output/(name+'.o')
    subprocess.run(['cc','-std=c11','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(sdk),'-I'+str(a.system/'test/scene'),'-c',str(source),'-o',str(obj)],check=True)
    objects.append(str(obj))
binary=a.output/'reader-gui'
subprocess.run(['g++','-pthread','-std=c++17','-fno-rtti','-fno-exceptions','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DRISC_READER_VECTOR_FONTS','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-include',str(ROOT/'reader/port/Arduino.h'),*['-I'+str(x) for x in includes],str(ROOT/'test/reader/gui_app.cpp'),str(a.app_source),*objects,*json.loads((a.engine_output/'objects/objects.json').read_text()),'-o',str(binary)],check=True)
fixture=a.output/'fixture'
if fixture.exists():shutil.rmtree(fixture)
sys.path.insert(0,str(ROOT/'test/reader'))
from make_fixture import create
create(fixture)
shutil.copytree(a.engine_output/'fixture/fonts',fixture/'fonts')
subprocess.run([str(binary),str(fixture),str(a.output/'frames')],check=True,timeout=90)

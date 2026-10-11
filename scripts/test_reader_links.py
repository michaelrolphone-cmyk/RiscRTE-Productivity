#!/usr/bin/env python3
"""Verify book navigation and physical taps using the actual Reader and presenter."""
import argparse,json,subprocess,shutil,sys
from pathlib import Path
from reader_sources import ROOT,inputs
p=argparse.ArgumentParser()
for n in ('runtime','system','engine-output','output'):p.add_argument('--'+n,type=lambda s:Path(s).resolve(),required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(a.system/'scripts'));from scene_sdk import stage_sdk
sdk=stage_sdk(a.runtime,a.system,a.output/'sdk');_,_,inc=inputs(a.engine_output/'upstream')
inc=[ROOT/'reader/port',ROOT/'reader',sdk,a.runtime/'sdk/cxx']+inc
flags=['-std=c++17','-O1','-g','-DRISC_READER_VECTOR_FONTS','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-include',str(ROOT/'reader/port/Arduino.h'),*['-I'+str(x) for x in inc]]
objects=json.loads((a.engine_output/'objects/objects.json').read_text())
sys.path.insert(0,str(ROOT/'test/reader'));from make_links_fixture import create
for name in ('engine','app'):
 fixture=a.output/name
 if fixture.exists():shutil.rmtree(fixture)
 create(fixture);shutil.copytree(a.engine_output/'fixture/fonts',fixture/'fonts')
binary=a.output/'links-engine'
subprocess.run(['g++',*flags,str(ROOT/'test/reader/links_engine_test.cpp'),*objects,'-o',str(binary)],check=True)
subprocess.run([str(binary),str(a.output/'engine')],check=True,timeout=120)
scene=[]
for name,path in [('scene',a.system/'Services/scene_host/host.c'),('fixture',ROOT/'test/reader/scene_startup.c')]:
 obj=a.output/(name+'.o');subprocess.run(['cc','-std=c11','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(sdk),'-I'+str(a.system/'test/scene'),'-c',str(path),'-o',str(obj)],check=True);scene.append(str(obj))
binary=a.output/'links-app'
subprocess.run(['g++','-pthread','-fno-rtti','-fno-exceptions','-fsanitize=undefined','-fno-sanitize-recover=all',*flags,str(ROOT/'test/reader/links_app_test.cpp'),str(ROOT/'reader/ReaderApp.cpp'),*scene,*objects,'-o',str(binary)],check=True)
subprocess.run([str(binary),str(a.output/'app'),str(a.output/'frames')],check=True,timeout=90)

#!/usr/bin/env python3
"""Exercise Reader's real entry with the production scene and host SD fixture.

Run test_reader.py first; --engine-output reuses its compiled upstream engine.
"""
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
binary=a.output/'reader-startup'
subprocess.run(['g++','-pthread','-std=c++17','-fno-rtti','-fno-exceptions','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DRISC_READER_VECTOR_FONTS','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-include',str(ROOT/'reader/port/Arduino.h'),*['-I'+str(x) for x in includes],str(ROOT/'test/reader/app_startup.cpp'),str(a.app_source),*objects,*json.loads((a.engine_output/'objects/objects.json').read_text()),'-o',str(binary)],check=True)
fixture=a.output/'fixture'
if fixture.exists():shutil.rmtree(fixture)
sys.path.insert(0,str(ROOT/'test/reader'))
from make_fixture import create
cases=[(source,None) for source in (None,'/Books/sample.epub','/Books/sample-ncx.epub','/Books/sample.txt','/Books/sample.md')]
cases += [('-', 'catalog'),('-', 'bad-catalog'),('-', 'legacy-scene')]
cases += [(source,'resident') for source in ('/Books/sample.epub','/Books/sample.txt','/Books/sample.md')]
cases += [('/Books/sample.epub','resident-redraw')]
cases += [('/Books/sample.epub','resident-later')]
cases += [('/Books/sample.epub','controls')]
cases += [('/Books/sample.epub','resident-turns')]
cases += [(source,'turns') for source in ('/Books/sample.epub','/Books/sample.txt')]
for source,scenario in cases:
    if fixture.exists():shutil.rmtree(fixture)
    create(fixture)
    shutil.copytree(a.engine_output/'fixture/fonts',fixture/'fonts')
    subprocess.run([str(binary),str(fixture)]+([source] if source else [])+([scenario] if scenario else []),check=True,timeout=60)

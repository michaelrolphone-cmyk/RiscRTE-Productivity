#!/usr/bin/env python3
"""Run the real Reader engine over the production storage.volume/FatFs implementation.

Run test_reader.py first; engine objects and synthetic books are reused.
"""
import argparse,json,shutil,subprocess
from pathlib import Path
from reader_sources import ROOT,inputs
p=argparse.ArgumentParser(description=__doc__)
for name in ('drivers','engine-output','output'):
    p.add_argument('--'+name,type=lambda x:Path(x).resolve(),required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
sdk=a.engine_output/'objects/sdk/include';volume=a.drivers/'lib/StorageFatFs'
objects=[]
for source in (ROOT/'test/reader/fatfs_volume.c',volume/'fatfs/ff.c',volume/'fatfs/ffunicode.c'):
    obj=a.output/(source.stem+'.o')
    subprocess.run(['cc','-std=c11','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-Wno-overflow','-I'+str(sdk),'-DREADER_FATFS_VOLUME_SOURCE="'+str(volume/'volume.c')+'"','-c',str(source),'-o',str(obj)],check=True)
    objects.append(str(obj))
_,_,inc=inputs(a.engine_output/'upstream')
inc=[ROOT/'reader/port',ROOT/'reader',sdk]+inc
binary=a.output/'reader-fatfs'
subprocess.run(['g++','-std=c++17','-fno-rtti','-fno-exceptions','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DRISC_READER_VECTOR_FONTS','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-include',str(ROOT/'reader/port/Arduino.h'),*['-I'+str(x) for x in inc],str(ROOT/'test/reader/cache_fatfs_test.cpp'),*objects,*json.loads((a.engine_output/'objects/objects.json').read_text()),'-o',str(binary)],check=True)
fixture=a.output/'fixture'
if fixture.exists():shutil.rmtree(fixture)
fixture.mkdir()
for name in ('Books','fonts'):shutil.copytree(a.engine_output/'fixture'/name,fixture/name)
subprocess.run([str(binary),str(fixture)],check=True,timeout=180)

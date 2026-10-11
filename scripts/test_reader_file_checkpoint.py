#!/usr/bin/env python3
"""Exercise real Reader file windows and closed-token policy checkpoints."""
import argparse,subprocess,shutil,sys
from pathlib import Path
from reader_sources import ROOT
p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime','system','output'):p.add_argument('--'+name,type=lambda x:Path(x).resolve(),required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(a.system/'scripts'))
from scene_sdk import stage_sdk
sdk=stage_sdk(a.runtime,a.system,a.output/'sdk')
binary=a.output/'checkpoint-test'
subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-I'+str(sdk),'-I'+str(ROOT/'reader/port'),str(ROOT/'test/reader/checkpoint_files_test.cpp'),str(ROOT/'reader/port/ReaderPort.cpp'),str(ROOT/'reader/port/FileCheckpoint.cpp'),'-o',str(binary)],check=True)
for case in ('normal','changed','missing','revoked'):
    fixture=a.output/case
    if fixture.exists():shutil.rmtree(fixture)
    fixture.mkdir()
    subprocess.run([str(binary),str(fixture),case],check=True,timeout=30)

#!/usr/bin/env python3
"""Exercise the actual upstream engine with a capability test provider."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
from reader_sources import ROOT, inputs, stage

p = argparse.ArgumentParser(description=__doc__)
for name in ('crosspoint', 'runtime', 'system', 'png', 'jpeg', 'output'):
    p.add_argument('--' + name, type=lambda x: Path(x).resolve(), required=True)
p.add_argument('--font', type=Path)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
upstream = stage(a.crosspoint, a.output / 'upstream')
objects = a.output / 'objects'
subprocess.run([sys.executable, str(ROOT / 'scripts/compile_reader.py'), '--upstream', str(upstream),
                '--runtime', str(a.runtime), '--system', str(a.system), '--png', str(a.png),
                '--jpeg', str(a.jpeg), '--output', str(objects)], check=True)
_, _, include = inputs(upstream)
include = [ROOT / 'reader/port', ROOT / 'reader', objects / 'sdk/include'] + include
binary = a.output / 'engine-test'
subprocess.run(['g++', '-std=c++17', '-DRISC_READER_VECTOR_FONTS', '-ffunction-sections', '-fdata-sections',
                '-Wl,--gc-sections', '-include', str(ROOT / 'reader/port/Arduino.h'),
                *['-I' + str(x) for x in include], str(ROOT / 'test/reader/engine_test.cpp'),
                str(ROOT / 'test/reader/host_volume.cpp'), *json.loads((objects / 'objects.json').read_text()),
                '-o', str(binary)], check=True)
fixture = a.output / 'fixture'
if fixture.exists():
    shutil.rmtree(fixture)
sys.path.insert(0, str(ROOT / 'test/reader'))
from make_fixture import create
create(fixture)
if a.font:
    (fixture / 'fonts').mkdir()
    shutil.copyfile(a.font, fixture / 'fonts/TestFont.ttf')
subprocess.run([str(binary), str(fixture)], check=True)

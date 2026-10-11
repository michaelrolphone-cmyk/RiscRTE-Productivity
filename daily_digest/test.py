#!/usr/bin/env python3
"""Exercise the actual app entrypoint against deterministic capability doubles."""
import argparse
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parent

def run(*args, **kw):
    subprocess.run(list(map(str, args)), check=True, **kw)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--system', type=Path, required=True)
    p.add_argument('--sanitize', action='store_true')
    a = p.parse_args(); a.runtime=a.runtime.resolve(); a.system=a.system.resolve()
    spec=importlib.util.spec_from_file_location('daily_build',ROOT/'build.py')
    build=importlib.util.module_from_spec(spec);spec.loader.exec_module(build)
    with tempfile.TemporaryDirectory(prefix='daily-paper-test-') as temp:
        temp=Path(temp); include=build.sdk(a.runtime,a.system,temp)
        flags=['-g','-O1','-Wall','-Wextra','-Werror','-I'+str(include),'-I'+str(ROOT),'-I'+str(a.system/'Services/update')]
        if a.sanitize: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
        link=['-no-pie'] if a.sanitize else []
        run('cc','-std=c11',*flags,'-c',ROOT/'app.c','-o',temp/'app.o')
        run('c++','-std=c++17',*flags,*link,ROOT/'test/app_test.cpp',ROOT/'metadata.cpp',temp/'app.o','-o',temp/'app_test')
        run('cc','-std=c11',*flags,'-c',ROOT/'provider/driver.c','-o',temp/'provider.o')
        run('cc','-std=c11',*flags,*link,ROOT/'test/provider_test.c',temp/'provider.o','-o',temp/'provider_test')
        run(temp/'app_test')
        for case in ('normal','busy','retained','failed-open','shutdown','revoked'):
            run(temp/'provider_test',case)

if __name__=='__main__': main()

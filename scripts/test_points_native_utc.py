#!/usr/bin/env python3
"""Production Points native UTC controller and pure schedule checks; no device I/O."""
import argparse, os, shutil, subprocess
from pathlib import Path
from build_points_native_utc import PIN,exact,sha
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args()
system,utilities,runtime=(x.resolve() for x in (a.system_apps,a.utilities,a.runtime))
for path,key in ((system,'system_sha'),(utilities,'utilities_sha'),(runtime,'runtime_sha')):exact(path,PIN[key])
for base,key in ((system,'system_sources'),(runtime,'runtime_sources')):
 for name,digest in PIN[key].items():
  if sha(base/name)!=digest:raise ValueError('Frozen input changed: '+name)
out=ROOT/'build/points-native-utc';out.mkdir(parents=True,exist_ok=True)
inc=out/'include';shutil.copytree(system/'lib/PortableApps/include',inc,dirs_exist_ok=True)
shutil.copytree(system/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
shutil.copyfile(runtime/'sdk/app/RiscRuntimeV1.h',inc/'RiscRuntimeV1.h')
includes=[utilities/'lib/Alarm/include',inc,runtime/'sdk/app',system/'lib/NativeApps/include',system/'Apps']
sources=[ROOT/'test/native_apps/points_native_utc_test.c',*[system/'lib/PortableApps/src'/x for x in ('PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')]]
for sanitized in (False,True):
 binary=out/f'controller-{int(sanitized)}'
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if sanitized else []
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DALARM_NATIVE_UTC','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_ALARM_CLIENT',*flags,*['-I'+str(x) for x in includes],*map(str,sources),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':os.environ.get('ASAN_OPTIONS','detect_leaks=0')})
print('Native UTC Points production controller and schedule: normal + ASan/UBSan passed')

#!/usr/bin/env python3
"""Qualify Watch Points modal draft preservation, with normal and ASan/UBSan runs."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);a=p.parse_args()
 out=ROOT/'build/watch-shared-text-points';out.mkdir(parents=True,exist_ok=True)
 includes=[a.utilities/'lib/Alarm/include',a.system_apps/'lib/PortableApps/include',a.system_apps/'lib/NativeApps/include',a.system_apps/'Apps']
 for san in (False,True):
  flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_ALARM_CLIENT']
  if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']
  binary=out/('san' if san else 'normal');subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(x) for x in includes],str(ROOT/'test/native_apps/points_watch_shared_input_test.c'),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
if __name__=='__main__':main()

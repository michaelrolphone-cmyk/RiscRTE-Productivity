#!/usr/bin/env python3
import argparse,os,pathlib,subprocess
r=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=pathlib.Path,required=True);p.add_argument('--utilities',type=pathlib.Path,required=True);a=p.parse_args()
out=r/'build/points-paper';out.mkdir(parents=True,exist_ok=True);frames=out/'frames';frames.mkdir(exist_ok=True)
includes=[a.utilities/'lib/Alarm/include',a.system_apps/'lib/PortableApps/include',a.system_apps/'lib/NativeApps/include',a.system_apps/'Apps']
exe=out/'points-paper'
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI','-DPORTABLE_DISPLAY_ROTATION=90','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie',*['-I'+str(i) for i in includes],str(r/'test/native_apps/points_paper_test.c'),str(a.system_apps/'lib/PortableApps/src/adapter.c'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True,timeout=30,env={**os.environ,'POINTS_PAPER_FRAMES':str(frames)})

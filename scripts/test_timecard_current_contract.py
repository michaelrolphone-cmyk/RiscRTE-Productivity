#!/usr/bin/env python3
"""Actual current controller/ledger/bridge, mocked synchronous provider media."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
S=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser()
 for x in ['system-apps','runtime','alarm-sdk','output']:p.add_argument('--'+x,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true');a=p.parse_args();o=a.output;o.mkdir(parents=True,exist_ok=False)
 shutil.copytree(a.system_apps/'lib/PortableApps/include',o/'include');shutil.copytree(a.system_apps/'lib/PortableApps/time',o/'time')
 for f in (a.runtime/'sdk/app').glob('*.h'):shutil.copyfile(f,o/'include'/f.name)
 for n in ['AlarmServiceV1.h','AlarmServiceV2.h']:shutil.copyfile(a.alarm_sdk/n,o/'include'/n)
 flags=['cc','-std=c11','-O1','-g','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Wall','-Wextra','-Werror','-Wno-unused-function']
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 for d in [o/'include',a.system_apps/'lib/NativeApps/include',S/'lib/NativeApps/include',a.system_apps/'Apps']:flags+=['-I'+str(d)]
 src=S/'test/native_apps/timecard_current_contract_test.c';stub=S/'test/native_apps/timecard_current_no_render_stubs.c';cmd=[*flags,str(src),str(stub),'-o',str(o/'contract')];subprocess.run(cmd,check=True)
 rows=[]
 for i in range(20):
  r=subprocess.run([str((o/'contract').resolve()),str(i)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'},timeout=30)
  (o/f'case-{i}.log').write_text(r.stdout+r.stderr);r.check_returncode();print(r.stdout,end='',flush=True);rows.append({'case':i,'passed':True})
 files=[S/'Apps'/n for n in ['timecard.c','timecard_portable.c','timecard_native_time.h','timecard_native_ui.inc','timecard_appdata_bridge.h']]+[src,stub,Path(__file__)]
 receipt={'passed':True,'sanitized':a.sanitize,'cases':rows,'scope':'Actual authoritative controller/ledger and AppData bridge; mocked synchronous storage/KV/time media. Unused UI removed by section GC: not renderer, target, Runtime or hardware qualification.','command':cmd,'source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'header_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [*sorted((o/'include').glob('*.h')),*sorted((a.system_apps/'lib/NativeApps/include').glob('*.h'))]}}
 assert receipt['source_sha256'][str(S/'Apps/timecard.c')]=='c17b0fed28eb44d2397c740ef56c8f07bc51c3f707d3080ec9ad5849e25a31a9'
 (o/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
if __name__=='__main__':main()

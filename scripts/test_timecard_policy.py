#!/usr/bin/env python3
"""Timecard modal policy through the actual app, PortableApps adapter and text host.
Scene/device providers are fixtures; physical sleep and target ISA are not run.
"""
import argparse,hashlib,json,os,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for x in ['system-apps','utilities','runtime','output']:p.add_argument('--'+x,type=pathlib.Path,required=True)
 p.add_argument('--native-height',action='store_true');p.add_argument('--normal-only',action='store_true');p.add_argument('--no-leak-checks',action='store_true');p.add_argument('--keep-binaries',action='store_true');a=p.parse_args()
 s,u,r,out=a.system_apps.resolve(),a.utilities.resolve(),a.runtime.resolve(),a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 for root,pin in [(s,'ed74c6827b9c2d0283bd69949aa4b05570dd0f85'),(u,'fc0bdbbbc600ce891e60c44d091fce0586ade7d8'),(r,'5561cc7bb09d1e2b03aafaecb8c95f0470f351f6')]:
  if subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()!=pin or subprocess.check_output(['git','-C',str(root),'status','--porcelain','--untracked-files=no'],text=True).strip():raise ValueError('Wrong or dirty dependency: '+str(root))
 inc=[ROOT/'test/native_apps',s/'lib/PortableApps/include',s/'lib/NativeApps/include',s/'lib/PortableApps/src',s/'Apps',s/'sdk/app',u/'lib/Alarm/include',u/'lib/Contexts/include',u/'Apps',u/'test/native_apps',r/'sdk/app',ROOT/'lib/NativeApps/include',ROOT/'lib/PortableTimecard/include',s/'Services/text_input']
 watch=json.loads((ROOT/'test/native_apps/timecard_watch_product_defines.json').read_text())+['-DPORTABLE_TEXT_INPUT_POLICY','-DTIMECARD_TEST_ALARM_HEADER="'+str(u/'lib/Alarm/include/AlarmServiceV1.h')+'"','-Wl,--wrap=t5_app_get_api','-include',str(ROOT/'test/native_apps/timecard_watch_alarm_profile.h')]
 x4=json.loads((ROOT/'test/native_apps/timecard_x4_product_defines.json').read_text())+['-DPORTABLE_TEXT_INPUT_POLICY','-DPORTABLE_TIMECARD_SHARED_TEXT_INPUT','-DPORTABLE_TEXT_INPUT_CLIENT','-DPORTABLE_CHECKED_STORAGE_RUNTIME','-DPORTABLE_APP_HOME_GUARD','-DTEST_RESIDENT_CLIENT','-DTEST_PRODUCTIVITY_SCROLL_FIXTURE','-DTEST_TIMECARD_SETTLE_DRAWS']
 if a.native_height:
  watch=[f for f in watch if f not in ('-DPORTABLE_WATCH_COLOR_ONLY','-DPORTABLE_WATCH_FROZEN_ICONS')]
  watch+=['-DTIMECARD_TEST_WIDTH=410','-DTIMECARD_TEST_HEIGHT=502','-DPORTABLE_WATCH_NATIVE_HEIGHT']
 cases={'watch':['idle','low-battery','pause-again','pause-retained','resume-retained','unsupported','background','background-busy','background-message','background-retained'],'x4':['idle','continuity','activity','queue','continuity-queue','pause-again','pause-busy','continuity-latched','pause-retained','resume-retained','retained','continuity-invalid-redraw','exit']}
 sources={'watch':['adapter.c','quick_actions.c','quick_render.c','quick_session.c','quick_radios.c'],'x4':['adapter.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c','PortableNativeTimeSource.c']}
 records=[];env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'}
 if a.no_leak_checks:env['ASAN_OPTIONS']='detect_leaks=0'
 for san in [False] if a.normal_only else [False,True]:
  for device,flags in ([('watch',watch)] if a.native_height else [('watch',watch),('x4',x4)]):
   binary=out/(device+('-san' if san else '-normal'))
   cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation',*(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if san else []),*dict.fromkeys(flags),*['-I'+str(x) for x in inc],str(ROOT/'test/native_apps'/('timecard_policy_'+device+'_test.c')),*[str(s/'lib/PortableApps/src'/x) for x in sources[device]],str(s/'Services/text_input/host.c'),'-o',str(binary)]
   subprocess.run(cmd,check=True,timeout=180)
   digest=hashlib.sha256(binary.read_bytes()).hexdigest()
   for case in cases[device]:
    run=subprocess.run([str(binary),case],capture_output=True,text=True,env=env,timeout=30)
    records.append(dict(device=device,case=case,sanitized=san,exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr,command=cmd,binary_sha256=digest))
    (out/'result.json').write_text(json.dumps(dict(runs=records,hardware_verified=False),indent=2)+'\n')
    print(run.stdout,end='',flush=True)
    if run.returncode:print(run.stderr,flush=True);raise SystemExit(run.returncode)
   if not a.keep_binaries:binary.unlink()
 print(str(len(records))+' Timecard lifecycle cases PASS')
if __name__=='__main__':main()

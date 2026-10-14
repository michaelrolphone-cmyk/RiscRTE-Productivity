#!/usr/bin/env python3
"""Exact Watch Points profile, actual app/adapter/scene/text hosts, synthetic devices.
Runs normal and ASan/UBSan workflows; frame bytes are hash-recorded and gzip-kept.
"""
import argparse,gzip,hashlib,json,os,shlex,subprocess
from pathlib import Path
from points_policy_cases import scenarios,tap
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['system-apps','utilities','runtime','watch','output']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--source-pins',type=Path);p.add_argument('--provider-system',type=Path);p.add_argument('--wave',action='store_true');p.add_argument('--normal-only',action='store_true');p.add_argument('--policy-only',action='store_true');p.add_argument('--legacy-only',action='store_true');p.add_argument('--no-leak-checks',action='store_true');p.add_argument('--keep-binaries',action='store_true');a=p.parse_args()
 roots={'system-apps':a.system_apps.resolve(),'utilities':a.utilities.resolve(),'runtime':a.runtime.resolve(),'watch':a.watch.resolve(),'productivity':ROOT,'generated':ROOT/'test/native_apps/points_policy_profile'};out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 pins=json.loads(a.source_pins.read_text()) if a.source_pins else {k:subprocess.check_output(['git','-C',str(v),'rev-parse','HEAD'],text=True).strip() for k,v in roots.items() if k!='generated'}
 profile=json.loads((roots['generated']/'profile.json').read_text());mapped=lambda text:text.format(**{k:str(v) for k,v in roots.items()});s=roots['system-apps'];r=roots['runtime']
 defs=list(map(mapped,profile['defines']))+['-DPOINTS_TEST_ALARM_HEADER="'+str(roots['utilities']/'lib/Alarm/include/AlarmServiceV1.h')+'"'];incs=list(map(mapped,profile['includes']));fixture=ROOT/'test/native_apps/points_policy_watch_host_test.c'
 if a.wave:defs=[x for x in defs if x!='-DPORTABLE_WATCH_COLOR_ONLY']+['-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPOINTS_WAVE_RENDER']
 s=a.provider_system.resolve() if a.provider_system else s
 service=['-I'+str(s/'Services/text_input'),'-I'+str(s/'sdk/app'),'-I'+str(s/'lib/PortableApps/include'),'-I'+str(r/'sdk/driver'),'-I'+str(s/'Services/scene_profile')]
 sources=[(s/'Services/scene_host/host.c',service+['-DSCENE_HOST_TELEMETRY=0',*([] if a.wave else ['-DSCENE_HOST_WATCH_240=1']),'-Dt5_driver_get=points_scene_driver_get']),(s/'Services/text_input/host.c',service+['-Dt5_driver_get=points_text_driver_get']),(s/'Services/scene_profile/profile.c',service+['-Dt5_driver_get=points_profile_driver_get']),(fixture,defs+incs+['-DPOINTS_WATCH_RENDER',*service]),*[(Path(mapped(x)),defs+incs) for x in profile['sources']]]
 legacy=scenarios();
 if a.wave:legacy.pop('panel-wide');legacy.pop('panel-mono')
 runs=[] if a.policy_only else [(name,None,actions) for name,actions in legacy.items()]
 if not a.legacy_only:
  name=legacy['unavailable'];key=tap(250,47,97,'text');done=tap(250,194,188,'text');save=tap(250,180,220)
  for mode in ['idle','low-battery','pause-again','pause-busy','pause-retained','resume-retained','resume-again','paused-home','missing','malformed','unsupported']:
   actions=name if mode in ['missing','malformed','unsupported'] else name+key+[(100,'text','mark',0,0),(12000 if mode=='low-battery' else 65000,'text','mark',0,0)]+done+save
   runs.append(('done-draft' if mode in ['missing','malformed','unsupported','pause-retained','resume-retained','resume-again','paused-home'] else 'save',mode,actions))
 records=[];commands=[];dependencies={};frame_sets={}
 for san in [False] if a.normal_only else [False,True]:
  build=out/('asan-ubsan' if san else 'normal');build.mkdir(exist_ok=True);objects=[]
  common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror']+(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if san else [])
  for i,(src,flags) in enumerate(sources):
   obj=build/(str(i)+'.o');dep=build/(str(i)+'.d');cmd=[*common,*flags,'-MMD','-MF',str(dep),'-c',str(src),'-o',str(obj)];commands.append(cmd);subprocess.run(cmd,check=True);objects.append(obj)
   for x in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):dependencies[x]=sha(x)
  binary=build/'points';cmd=[*common,*map(str,objects),'-o',str(binary)];commands.append(cmd);subprocess.run(cmd,check=True);binary_hash=sha(binary)
  for case,mode,actions in runs:
   if a.wave:actions=[(delay,gate,kind,(28 if x<120 else 380) if gate=='text' and y==20 else (x*410+239)//240,20 if gate=='text' and y==20 else (y*(502 if gate=='text' else 410)+239)//240) for delay,gate,kind,x,y in actions]
   label='policy-'+mode if mode else case;folder=build/label;folder.mkdir(exist_ok=True);f=folder/'actions.txt';f.write_text(''.join(' '.join(map(str,row))+'\n' for row in actions))
   env={k:v for k,v in os.environ.items() if not k.startswith('POINTS_')};env.update(POINTS_CASE=case,UBSAN_OPTIONS='halt_on_error=1')
   if a.no_leak_checks:env['ASAN_OPTIONS']='detect_leaks=0'
   if mode:env.update(POINTS_POLICY=mode,POINTS_NO_FRAMES='1')
   if case=='back-return':env['POINTS_BACK']='1'
   if case=='navigation-home':env['POINTS_HOME']='1'
   if case in ['save-delayed','rapid-repeat']:env['POINTS_DELAY']='1'
   run=subprocess.run([str(binary),str(folder),str(f)],env=env,capture_output=True,text=True,timeout=90);(folder/'run.log').write_text(run.stdout+run.stderr)
   frames=[]
   for frame in sorted(folder.glob('frame-*.ppm')):
    data=frame.read_bytes();z=frame.with_suffix('.ppm.gz');z.write_bytes(gzip.compress(data,mtime=0));assert gzip.decompress(z.read_bytes())==data;frames.append({'name':frame.name,'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'gzip_sha256':sha(z)});frame.unlink()
   frames_hash=[x['sha256'] for x in frames]
   if san:assert frame_sets.get(label)==frames_hash,(label,'normal/sanitized frame mismatch')
   else:frame_sets[label]=frames_hash
   records.append(dict(case=label,sanitized=san,exit_code=run.returncode,binary_sha256=binary_hash,frames=frames,stdout=run.stdout,stderr=run.stderr))
   evidence=dict(schema='points.modal-policy.host.v1',runs=records,commands=commands,dependencies=dependencies,pins=pins,hardware_verified=False,target_instructions_executed=False,scope='Actual Watch Points app, PortableApps adapter, scene host, shared text host and scene profile; synthetic native transports, local sleep and bounded Contexts/broadcast provider')
   (out/'result.json').write_text(json.dumps(evidence,indent=2)+'\n');print(('PASS' if run.returncode==0 else 'FAIL'),san,label,flush=True)
   if run.returncode:print(run.stdout+run.stderr,flush=True);raise SystemExit(1)
  if not a.keep_binaries:
   binary.unlink()
   for obj in objects:obj.unlink()
 print(len(records),'workflows PASS'+('; normal/sanitized frame parity verified' if not a.normal_only else '; normal-only diagnostic'))
if __name__=='__main__':main()

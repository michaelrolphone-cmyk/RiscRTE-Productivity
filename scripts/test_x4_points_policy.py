#!/usr/bin/env python3
"""Run actual X4 catalog/adapter regressions and modal checkpoint boundaries."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from test_x4_shared_text_points import CASES
ROOT=Path(__file__).resolve().parents[1]
POLICY=['detached','continuity','pending','busy','presenting','input-busy','activity','pending-home','pending-alarm','resident-poll-exit','resident-policy-exit','missing','unavailable','retained-pause','retained-resume','again-resume']
def main():
 p=argparse.ArgumentParser()
 for n in ('build','utilities','output'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--sanitize',choices=['normal','undefined','address,undefined'],default='normal')
 p.add_argument('--suite',choices=['all','regression','policy'],default='all')
 p.add_argument('--case',action='append')
 a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 receipt=json.loads((a.build/'points_in_time/x4-native-app.json').read_text());cmd=receipt['compile_command']
 flags=[s for s in cmd if s.startswith(('-D','-I'))]
 sources=[s for s in cmd if s.endswith('.c') and Path(s).name not in ('catalog.c','points_catalog_app.c')]
 san=[] if a.sanitize=='normal' else ['-fsanitize='+a.sanitize,'-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 runs=[];frames=out/'frames';frames.mkdir(exist_ok=True)
 for label,test,cases in [('regression','resident_points_test.c',CASES),('policy','points_policy_x4_test.c',POLICY)]:
  if a.suite not in ('all',label):continue
  if a.case:cases=[c for c in cases if c in a.case]
  binary=out/label
  command=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function',*san,*flags,'-DTEST_RESIDENT_CLIENT','-DTEST_DECOUPLED_UI','-I'+str(a.utilities.resolve()/'test/native_apps'),str(ROOT/'test/native_apps'/test),*sources,'-o',str(binary)]
  subprocess.run(command,check=True);(out/(label+'-compile.json')).write_text(json.dumps(command,indent=2)+'\n')
  for case in cases:
   run=subprocess.run([str(binary),case],capture_output=True,text=True,timeout=90,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','POINTS_CATALOG_FRAMES':str(frames)})
   (out/(label+'-'+case+'.log')).write_text(run.stdout+run.stderr)
   if run.returncode:raise RuntimeError(f'{label}/{case}: {run.returncode}: {run.stdout}{run.stderr}')
   runs.append(dict(suite=label,case=case,passed=True));print(label,case,'PASS',flush=True)
 evidence=dict(sanitize=a.sanitize,runs=runs,frames={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in frames.iterdir()},target_sha256=receipt['elf_sha256'],hardware_tested=False)
 (out/'qualification.json').write_text(json.dumps(evidence,indent=2)+'\n')
if __name__=='__main__':main()

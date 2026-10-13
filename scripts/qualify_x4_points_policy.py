#!/usr/bin/env python3
"""Rebuild deployed Points exactly, then qualify only its modal successor."""
import argparse,hashlib,json,os,shutil,subprocess,sys,tarfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'docs/recovery/x4-points-0617'
PINS={'system':'c4579cdb14f4ce185aed32d5de86f44a25733007','baseline_system':'5b28172e979b5f008da19e9a00a3b98e35937974','runtime':'a91edf3fada6a559ec107186dd163d0c8da10d29','baseline_runtime':'b25b1d467a557e8d693211cff2eff959eb9c79de','utilities':'35c2141c68668971dc524d98702109b00c740815','baseline_app':'d6a9657378fedcdb61d1cea83f2108799e57ccae'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser()
 for n in [*PINS,'compiler','output']:p.add_argument('--'+n.replace('_','-'),type=Path,required=True)
 p.add_argument('--sanitizer',default='address,undefined',choices=['undefined','address,undefined'])
 a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 for n,pin in PINS.items():
  path=getattr(a,n).resolve();assert subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()==pin,n
  assert not subprocess.check_output(['git','-C',str(path),'status','--porcelain','--untracked-files=no'],text=True),n
 proof=json.loads((FIXTURE/'reconciliation.json').read_text())
 for n,h in proof['baseline_fixture_hashes'].items():assert sha(FIXTURE/n)==h,n
 sdk=out/'baseline-sdk';sdk.mkdir()
 with tarfile.open(FIXTURE/'baseline-sdk-source.tar.gz') as archive:
  for m in archive.getmembers():
   assert not m.issym() and not m.islnk() and not Path(m.name).is_absolute() and '..' not in Path(m.name).parts
  archive.extractall(sdk,filter='data')
 baseline_target=out/'baseline-target';baseline_target.mkdir();shutil.copyfile(FIXTURE/'baseline-profile.json',baseline_target/'x4-native-app.json')
 env={**os.environ,'NATIVE_APP_CC':str(a.compiler.resolve())}
 for label,app,system,runtime,policy in [('baseline',a.baseline_app,a.baseline_system,a.baseline_runtime,False),('candidate',ROOT,a.system,a.runtime,True)]:
  command=[sys.executable,str(app/'scripts/build_x4_home_points.py'),'--common-system','--raster-snapshot','--baseline-target',str(baseline_target),'--baseline-sdk',str(sdk/'sdk'),'--system',str(system),'--runtime',str(runtime),'--utilities',str(a.utilities),'--output',str(out/label),'--reservation',str(app/'docs/recovery/x4-057-version-reservation.json')]
  if policy:command.append('--modal-policy')
  subprocess.run(command,env=env,check=True)
 sys.path.insert(0,str(FIXTURE));from compact_current_elf import compact
 hashes={}
 for label in ('baseline','candidate'):
  original=out/label/'points_in_time/points_in_time.elf';compact_elf=original.with_name('points_in_time.compact.elf');shutil.copyfile(original,compact_elf);compact(compact_elf,str(a.compiler.resolve()));hashes[label]=sha(compact_elf)
 assert hashes['baseline']==proof['qualified_elf_sha256']
 results={}
 for label,build,suite,sanitizer in [('baseline-normal','baseline','regression','normal'),('candidate-normal','candidate','all','normal'),('candidate-sanitized','candidate','all',a.sanitizer)]:
  subprocess.run([sys.executable,str(ROOT/'scripts/test_x4_points_policy.py'),'--build',str(out/build),'--utilities',str(a.utilities),'--output',str(out/label),'--suite',suite,'--sanitize',sanitizer],check=True)
  results[label]=json.loads((out/label/'qualification.json').read_text())
 assert results['baseline-normal']['frames']==results['candidate-normal']['frames']==results['candidate-sanitized']['frames']
 result=dict(pins=PINS,target_hashes=hashes,baseline_exact=True,frame_comparisons=len(results['baseline-normal']['frames'])*2,runs={k:len(v['runs']) for k,v in results.items()},hardware_tested=False)
 (out/'qualification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()

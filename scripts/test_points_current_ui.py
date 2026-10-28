#!/usr/bin/env python3
"""Execute current Points/scene/provider-double UI using exact target receipt inputs."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build-record',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--capture',action='store_true');p.add_argument('--scenario',type=int);a=p.parse_args()
 r=json.loads(a.build_record.read_text());out=a.output;out.mkdir(parents=True,exist_ok=True)
 for name,digest in r['consumed_source_sha256'].items():
  if sha(name)!=digest:raise ValueError('Target input changed: '+name)
 ci=r['compile_inputs'];sources=[str(ROOT/'test/native_apps/points_current_ui_test.c'),*[s for s in ci['sources'] if Path(s).name not in ('points_in_time.c','catalog.c')]]
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*ci['definitions'],*['-I'+s for s in ci['include_directories']]]
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 consumed={}
 for source in sources:
  deps=subprocess.check_output([os.environ.get('CC','cc'),*flags,'-MM',source],text=True).replace('\\\n',' ').split(':',1)[1].split()
  for name in deps:consumed[str(Path(name).resolve())]=sha(name)
 command=[os.environ.get('CC','cc'),*flags,*sources,'-o',str(out/'test')];(out/'command.json').write_text(json.dumps(command,indent=2)+'\n');subprocess.run(command,check=True)
 passed=[];frames=out/'frames';frames.mkdir(exist_ok=True)
 for w,h in [(466,466),(240,240)]:
  for n in [a.scenario] if a.scenario is not None else range(14):
   run=subprocess.run([str(out/'test'),str(w),str(h),str(n)],text=True,capture_output=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1',**({'POINTS_NATIVE_FRAMES':str(frames)} if a.capture else {})});(out/f'{w}x{h}-{n}.log').write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,end='',flush=True);run.check_returncode();passed.append([w,h,n])
 for name,digest in consumed.items():
  if sha(name)!=digest:raise ValueError('Host input changed during run: '+name)
 captures={}
 for image in frames.glob('*.ppm'):
  content=image.read_bytes();header,pixels=content.split(b'\n255\n',1);magic,dimensions=header.split(b'\n');w,h=map(int,dimensions.split())
  if magic!=b'P6' or len(pixels)!=w*h*3:raise ValueError('Incomplete capture: '+str(image))
  captures[image.name]={'sha256':sha(image),'bytes':len(content),'width':w,'height':h}
 (out/'results.json').write_text(json.dumps({'consumed_source_sha256':consumed,'captures':captures,'provider_doubles':True,'actual_runtime':False,'hardware_verified':False,'fixture_sha256':sha(ROOT/'test/native_apps/points_current_ui_test.c'),'runner_sha256':sha(__file__),'target_receipt_sha256':sha(a.build_record),'sanitized':a.sanitize,'passed':passed},indent=2)+'\n')
if __name__=='__main__':main()

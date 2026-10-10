#!/usr/bin/env python3
import argparse,hashlib,json,os,subprocess,sys,shutil
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from reader_sources import inputs,ROOT
p=argparse.ArgumentParser();p.add_argument('--upstream',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system',type=Path,required=True);p.add_argument('--png',type=Path,required=True);p.add_argument('--jpeg',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--target',action='store_true');a=p.parse_args()
sys.path.insert(0,str(a.system/'scripts'));from scene_sdk import stage_sdk
if (a.output/'sdk').exists():shutil.rmtree(a.output/'sdk')
include=stage_sdk(a.runtime,a.system,a.output/'sdk')
cpp,cc,inc=inputs(a.upstream)
cpp+=list((ROOT/'reader/port').glob('*.cpp'))+list((ROOT/'reader').glob('*.cpp'))
if a.target:cpp+=[a.runtime/'sdk/cxx/RiscCppRuntime.cpp',a.runtime/'sdk/cxx/upstream/tree.cc',a.runtime/'sdk/cxx/upstream/hash_bytes.cc']
else:cpp=[p for p in cpp if p.name!='ReaderApp.cpp']
cpp+=[a.png/'src/PNGdec.cpp',a.jpeg/'src/JPEGDEC.cpp']
inc=[ROOT/'reader/port',include,a.runtime/'sdk/cxx']+inc+[a.png/'src',a.jpeg/'src',a.upstream/'lib/uzlib/src']
cc+=list((a.upstream/'lib/uzlib/src').glob('*.c'))+list((a.png/'src').glob('*.c'))
# uzlib's public include is under src in this upstream snapshot.
common=['-Os','-fno-ivopts','-fPIC','-ffunction-sections','-fdata-sections','-DRISC_READER_VECTOR_FONTS','-DXML_DTD','-DXML_GE=1','-DXML_NS','-DHAVE_MEMMOVE','-DBYTEORDER=1234','-DXML_CONTEXT_BYTES=1024','-DXML_STATIC','-DFT_CONFIG_OPTION_DISABLE_FILE_SYSTEM','-DPNG_NO_LOGGING','-include',str(ROOT/'reader/port/Arduino.h')]
base=Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-'
cxx=os.environ.get('NATIVE_APP_CXX',str(base)+'g++') if a.target else 'g++';c=os.environ.get('NATIVE_APP_CC',str(base)+'gcc') if a.target else 'gcc'
if a.target:common+=['-mtext-section-literals','-mlongcalls','-fvisibility=hidden']
a.output.mkdir(parents=True,exist_ok=True)
dep_hash=hashlib.sha256(b''.join(p.read_bytes() for d in [ROOT/'reader',include,a.upstream,a.runtime/'sdk/cxx'] for p in sorted(d.rglob('*.h')))).digest()
def compile_one(source):
 name=hashlib.sha256(str(source.resolve()).encode()).hexdigest()[:12]+'-'+source.stem+'.o';dest=a.output/name
 flags=common[:]
 if a.target and source.suffix in ('.cpp','.cc'):flags=['-include',str(a.runtime/'sdk/cxx/TemplateConfig.h')]+flags
 if source.suffix=='.c':flags=[x for x in flags];idx=flags.index('-include');flags[idx+1]=str(ROOT/'reader/port/CCompat.h')
 cmd=[cxx if source.suffix in ('.cpp','.cc') else c,*flags,*(['-std=c++17','-fno-exceptions','-fno-rtti','-fno-threadsafe-statics'] if source.suffix in ('.cpp','.cc') else ['-std=c99']),*['-I'+str(i) for i in inc],'-c',str(source),'-o',str(dest)]
 key=hashlib.sha256((' '.join(cmd)).encode()+source.read_bytes()+dep_hash).hexdigest();stamp=dest.with_suffix('.key')
 if dest.exists() and stamp.exists() and stamp.read_text()==key:return str(dest)
 result=subprocess.run(cmd,capture_output=True,text=True)
 if result.returncode:raise RuntimeError(str(source)+'\n'+result.stderr)
 stamp.write_text(key);return str(dest)
errors=[];objects=[]
with ThreadPoolExecutor(max_workers=1 if a.target else 4) as pool:
 futures=[(s,pool.submit(compile_one,s)) for s in cpp+cc]
 for s,f in futures:
  try:objects.append(f.result())
  except Exception as e:errors.append(str(e))
(a.output/'compile-errors.log').write_text('\n\n'.join(errors))
print(f'{len(objects)} objects compiled, {len(errors)} failures')
if errors:print('\n\n'.join(errors)[:16000]);sys.exit(1)
(a.output/'objects.json').write_text(json.dumps(objects))

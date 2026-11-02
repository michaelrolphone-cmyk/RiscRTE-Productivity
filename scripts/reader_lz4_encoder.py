"""Deterministic encoder-only search; emitted standard blocks use unchanged Runtime decoder."""
import ctypes, hashlib, json, shutil, subprocess, tempfile
from pathlib import Path
PIN='5ff839680134437dbf4678f3d0c7b371d84f4964'
class Encoder:
 def __init__(self):
  self.source=Path(__file__).resolve().parents[2]/'Lz4'
  assert subprocess.check_output(['git','-C',str(self.source),'rev-parse','HEAD'],text=True).strip()==PIN
  assert not subprocess.run(['git','-C',str(self.source),'diff-index','--quiet','HEAD','--']).returncode
  self.work=tempfile.TemporaryDirectory(prefix='reader-lz4-')
  self.libs=[]
  self.records={}
  self.source_hashes={n:hashlib.sha256((self.source/'lib'/n).read_bytes()).hexdigest() for n in ('lz4.c','lz4.h','lz4hc.c','lz4hc.h')}
  for horizon in (4096,8192,16384,32768):
   folder=Path(self.work.name)/str(horizon);folder.mkdir()
   for name in self.source_hashes:shutil.copyfile(self.source/'lib'/name,folder/name)
   path=folder/'lz4hc.c';text=path.read_text();old='#define LZ4_OPT_NUM   (1<<12)'
   assert text.count(old)==1
   path.write_text(text.replace(old,'#define LZ4_OPT_NUM   '+str(horizon)))
   library=folder/'encoder.so'
   subprocess.run(['cc','-shared','-fPIC','-O2','-DLZ4HC_HEAPMODE=1',str(folder/'lz4.c'),str(path),'-o',str(library)],check=True)
   lib=ctypes.CDLL(str(library));lib.LZ4_versionNumber.restype=ctypes.c_int
   assert lib.LZ4_versionNumber()==10904
   lib.LZ4_compressBound.argtypes=[ctypes.c_int];lib.LZ4_compressBound.restype=ctypes.c_int
   lib.LZ4_compress_HC.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int];lib.LZ4_compress_HC.restype=ctypes.c_int
   lib.LZ4_decompress_safe.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int];lib.LZ4_decompress_safe.restype=ctypes.c_int
   self.libs.append((horizon,lib))
  self.cache={}
 def compress(self,data):
  sha=hashlib.sha256(data).hexdigest()
  if sha in self.cache:return self.cache[sha]
  source=ctypes.create_string_buffer(data);bound=self.libs[0][1].LZ4_compressBound(len(data));target=ctypes.create_string_buffer(bound)
  options=[]
  for horizon,lib in self.libs:
   for level in (range(3,13) if horizon==4096 else (12,)):
    size=lib.LZ4_compress_HC(source,target,len(data),bound,level);assert size>0
    block=target.raw[:size];decoded=ctypes.create_string_buffer(len(data))
    assert self.libs[0][1].LZ4_decompress_safe(block,decoded,size,len(data))==len(data)
    assert decoded.raw==data
    options.append((size,horizon,level,block))
  size,horizon,level,block=min(options,key=lambda v:v[:3])
  self.records[sha]={'bytes':len(data),'selected':{'horizon':horizon,'level':level,'stored_block_bytes':size,'block_sha256':hashlib.sha256(block).hexdigest()},'candidates':[{'horizon':h,'level':l,'stored_block_bytes':n} for n,h,l,b in options]}
  print('LOSSLESS ENCODER '+json.dumps({'sha256':sha,**self.records[sha]}),flush=True)
  self.cache[sha]=block
  return block
 def receipt(self):
  return {'source_repository':'https://github.com/lz4/lz4','source_commit':PIN,'source_sha256':self.source_hashes,'method':'Stock levels3–12 plus level12 optimal-parser horizons8192/16384/32768; minimum standard block, stock decoder byte-equality mandatory; no distance/offset/format changes','images':self.records}

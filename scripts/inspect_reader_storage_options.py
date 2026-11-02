#!/usr/bin/env python3
"""Read-only capacity diagnostics. Never emits a product candidate or changes baseline assets."""
import argparse,ctypes,hashlib,json,os,subprocess,sys,tempfile
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elf_runtime_equivalence import require_runtime_equal
p=argparse.ArgumentParser()
for n in ('system','baseline','bootfs-tools','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args()
sys.path[:0]=[str(a.system.resolve()/'scripts'),str(a.bootfs_tools.resolve())]
from assemble_shared_usb_cohort import decode_store
from read_only_spiffs import read_image
raw=(a.baseline/'product-watch-store.bin').read_bytes();files=decode_store(read_image(raw,len(raw)))
boot=json.loads(files['boot.json'])
base=Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-'
strip=str(base)+'strip';nm=str(base)+'nm'
lib=ctypes.CDLL('liblz4.so.1');lib.LZ4_compressBound.argtypes=[ctypes.c_int];lib.LZ4_compressBound.restype=ctypes.c_int
lib.LZ4_compress_HC.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int];lib.LZ4_compress_HC.restype=ctypes.c_int
def compressed(data):
 src=ctypes.create_string_buffer(data);cap=lib.LZ4_compressBound(len(data));out=ctypes.create_string_buffer(cap)
 return lib.LZ4_compress_HC(src,out,len(data),cap,12)
def protected(path):
 with path.open('rb') as stream:
  e=ELFFile(stream);ss=list(e.iter_sections())
  return {s.name:[s['sh_type'],s['sh_flags'],s['sh_addr'],s['sh_size'],s['sh_addralign'],s['sh_entsize'],ss[s['sh_link']].name if s['sh_link'] else '',hashlib.sha256(s.data() if s['sh_type']!='SHT_NOBITS' else b'').hexdigest()] for s in ss if s['sh_flags']&2 or s['sh_type'] in ('SHT_REL','SHT_RELA')}
results=[]
with tempfile.TemporaryDirectory() as td:
 for row in boot['app_capabilities']:
  if 'image_source' in row:continue
  manifest=json.loads(files[row['manifest']])
  if manifest.get('type')!='application' or manifest.get('provides'):continue
  name=str(Path(row['manifest']).parent/manifest['file_name']);data=files[name]
  if not data.startswith(b'\x7fELF'):continue
  path=Path(td)/'test.elf';path.write_bytes(data);before=protected(path)
  symbols=subprocess.check_output([nm,'-D',str(path)])
  subprocess.run([strip,'--strip-unneeded','--remove-section=.xt.prop','--remove-section=.xt.lit','--remove-section=.xt.insn','--remove-section=.comment',str(path)],check=True)
  after=path.read_bytes()
  witness=require_runtime_equal(data,after)
  derivatives=a.output.parent/'metadata-options';derivatives.mkdir(exist_ok=True)
  (derivatives/(hashlib.sha256(data).hexdigest()+'.elf')).write_bytes(after)
  (derivatives/(hashlib.sha256(data).hexdigest()+'.json')).write_text(json.dumps(witness,sort_keys=True)+chr(10))
  assert protected(path)==before and subprocess.check_output([nm,'-D',str(path)])==symbols
  results.append({'id':manifest['id'],'path':name,'before_bytes':len(data),'after_bytes':len(after),'before_lz4':compressed(data),'after_lz4':compressed(after),'saving_lz4':compressed(data)-compressed(after),'runtime_sections_dynamic_symbols_unchanged':True,'before_sha256':hashlib.sha256(data).hexdigest(),'after_sha256':hashlib.sha256(after).hexdigest()})
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps({'scope':'diagnostic only; no changed canonical app delivered; cross-product identity and cohort qualification required before any use','apps':results},indent=2)+'\n')
for row in sorted(results,key=lambda x:-x['saving_lz4']):print('METADATA OPTION '+json.dumps(row),flush=True)

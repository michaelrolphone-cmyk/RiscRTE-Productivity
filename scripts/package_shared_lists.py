#!/usr/bin/env python3
"""Canonical Lists packaging from exact published products; no app rewrite."""
import argparse, hashlib, io, json, sys, zipfile
from pathlib import Path, PurePosixPath
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
INPUTS = {
 'watch.zip':'3a5aadb0eeb5c4b37640d606978162b5c0484893aada661baca4ca39a12d66f2',
 'wave.zip':'aae8665d9240bd49dbdae89f250525ccea6cabc44215133248ee1a2c7b19d1d6',
 'x4-store.bin':'8d38a77bdb3dd1439e8736d9c0363798f20803e02b44fc1aa5dab578207c4adc',
 'x4-sd.zip':'2b6dce3efb468f3cd728b50707e0c9552d84e225b65e2b27e683d3d726bc6cef'}
TOOLS = {'current_bootfs.py':'6f0997a71fe6d2abbb97ed91e0e4180abf43b2056afbcb5b54d303437791576b',
 'read_only_spiffs.py':'e48d3b2422c60f81fdae6371ff3c5b65c5cbaa9dd250b4a2925c74c0b45f040d'}
VERSION='0.1.3'
def sha(b): return hashlib.sha256(b).hexdigest()
def file_sha(p):
 with p.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()
def encode(v): return (json.dumps(v,sort_keys=True,indent=2)+'\n').encode()
def safe(n):
 p=PurePosixPath(n)
 if not n or p.is_absolute() or '..' in p.parts or '\\' in n: raise ValueError('Unsafe path: '+n)
 return n
def unzip(source):
 with zipfile.ZipFile(source) as z:
  names=z.namelist()
  if len(names)!=len(set(names)): raise ValueError('Duplicate ZIP entries')
  for n in names: safe(n)
  return {n:z.read(n) for n in names if not n.endswith('/')}
def loaded(data):
 e=ELFFile(io.BytesIO(data))
 if e.elfclass!=32 or not e.little_endian or e['e_machine']!='EM_XTENSA' or e['e_type']!='ET_DYN': raise ValueError('Not an Xtensa module')
 names=[s.name for s in e.iter_sections()]
 if len(names)!=len(set(names)): raise ValueError('Duplicate ELF sections')
 allocated={}
 for s in e.iter_sections():
  if s['sh_flags']&2:
   allocated[s.name]=(s['sh_type'],s['sh_flags'],s['sh_addr'],s['sh_size'],s['sh_addralign'],s['sh_entsize'],s.data())
 relocations={}
 for s in e.iter_sections():
  if isinstance(s,RelocationSection):
   table=e.get_section(s['sh_link'])
   relocations[s.name]=[(r['r_offset'],r['r_info_type'],r.entry.get('r_addend'),table.get_symbol(r['r_info_sym']).name) for r in s.iter_relocations()]
 return (e['e_entry'],e['e_flags'],allocated,relocations)
def locate(files,sd,version='0.1.2'):
 boot=json.loads(files['boot.json'])
 rows=[r for r in boot['app_capabilities'] if r.get('manifest')=='lists.json']
 if not rows: raise ValueError('Lists not installed')
 routes={r.get('image_source',{}).get('path') for r in rows}
 routes.discard(None)
 if len(routes)>1: raise ValueError('Ambiguous Lists image')
 route=next(iter(routes),None)
 blob=files.get('lists.elf')
 if blob is None:
  if not route or route not in sd: raise ValueError('Lists image absent')
  blob=sd[route]
 if route and route in sd and blob!=sd[route]: raise ValueError('Conflicting Lists images')
 m=json.loads(files['lists.json'])
 if m['id']!='lists' or m['file_name']!='lists.elf' or m['version']!=version: raise ValueError('Unexpected installed Lists identity')
 return blob,route,m
def run(inputs,tools,output):
 if output.exists(): raise ValueError('Output exists')
 for n,h in INPUTS.items():
  if file_sha(inputs/n)!=h: raise ValueError('Original input differs: '+n)
 for n,h in TOOLS.items():
  if file_sha(tools/n)!=h: raise ValueError('Bootfs tool differs: '+n)
 sys.path.insert(0,str(tools.resolve()))
 from read_only_spiffs import read_image
 from current_bootfs import build
 products={}
 for lane in ('watch','wave','x4'):
  if lane=='x4':
   raw=(inputs/'x4-store.bin').read_bytes();sd=unzip(inputs/'x4-sd.zip')
  else:
   z=unzip(inputs/(lane+'.zip'));raw=z['candidate/store.bin']
   sd=unzip(io.BytesIO(z['candidate/sd-apps.zip'])) if 'candidate/sd-apps.zip' in z else {}
  files=read_image(raw,len(raw));blob,route,manifest=locate(files,sd)
  products[lane]=(raw,files,sd,blob,route,manifest)
 canonical=products['x4'][3]
 if len(canonical)!=41524 or not sha(canonical).startswith('8bdbc1a3'): raise ValueError('Original X4 evidence changed')
 ce=ELFFile(io.BytesIO(canonical))
 for n in ('.xt.prop','.xt.lit','.xtensa.info'):
  if ce.get_section_by_name(n) is None: raise ValueError('Missing original Xtensa evidence: '+n)
 for lane,(_,_,_,blob,_,_) in products.items():
  if loaded(blob)!=loaded(canonical): raise ValueError('Executable semantics differ: '+lane)
  if lane!='x4' and (len(blob)!=26352 or not sha(blob).startswith('cfbb5928')): raise ValueError('Original compact evidence changed')
 output.mkdir(parents=True)
 (output/'lists.elf').write_bytes(canonical)
 (output/'lists.full.elf').write_bytes(canonical)
 results={}
 for lane,(raw,before,sd,blob,route,manifest) in products.items():
  d=output/lane;d.mkdir()
  after=dict(before);nextmanifest={**manifest,'version':VERSION}
  after['lists.json']=encode(nextmanifest)
  if 'lists.elf' in after: after['lists.elf']=canonical
  nextsd={**sd}
  if route and route in nextsd: nextsd[route]=canonical
  changed={n for n in before if before[n]!=after[n]}
  if before.keys()!=after.keys() or not changed<={'lists.json','lists.elf'}: raise ValueError('Unrelated store mutation')
  if {n for n in sd if sd[n]!=nextsd[n]}-({route} if route else set()): raise ValueError('Unrelated SD mutation')
  image,packing=build(after,len(raw))
  if read_image(image,len(raw))!=after: raise ValueError('Store readback differs')
  (d/'store.bin').write_bytes(image)
  for which,entries in (('before',before),('after',after)):
   for n,b in entries.items():
    p=d/which/safe(n);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
  # SD is a separate product volume, never extra internal cohort inventory.
  for which,entries in (('before',sd),('after',nextsd)):
   for n,b in entries.items():
    p=d/(which+'-sd')/safe(n);p.parent.mkdir(parents=True,exist_ok=True)
    if p.exists() and p.read_bytes()!=b: raise ValueError('External path aliases store')
    p.write_bytes(b)
  if sd:
   with zipfile.ZipFile(d/'sd-apps.zip','w',compression=zipfile.ZIP_DEFLATED) as z:
    for n,b in sorted(nextsd.items()):
     info=zipfile.ZipInfo(n,date_time=(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
     z.writestr(info,b)
   if unzip(d/'sd-apps.zip')!=nextsd: raise ValueError('SD roundtrip differs')
  (d/'original-lists.elf').write_bytes(blob)
  if locate(after,nextsd,VERSION)[0]!=canonical: raise ValueError('Product identity differs')
  results[lane]={'original_sha256':sha(blob),'original_bytes':len(blob),'canonical_sha256':sha(canonical),'canonical_bytes':len(canonical),'version':VERSION,'changed_internal':sorted(changed),'changed_sd':sorted(n for n in sd if sd[n]!=nextsd[n]),'packing':packing,'allocated_sections_entry_and_relocations_identical':True,'all_grants_providers_namespaces_and_data_unchanged':True}
 receipt={'schema':1,'inputs':INPUTS,'tools':TOOLS,'canonical_source':'original X4 complete ELF, including all Xtensa metadata','canonical_sha256':sha(canonical),'canonical_bytes':len(canonical),'full_sidecar_identical':True,'products':results,'target_instructions_executed':False,'physical_testing':'not performed'}
 (output/'receipt.json').write_bytes(encode(receipt));print(json.dumps(receipt,indent=2),flush=True)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('inputs','bootfs-tools','output'): p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();run(a.inputs,a.bootfs_tools,a.output)

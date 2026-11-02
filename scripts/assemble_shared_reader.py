#!/usr/bin/env python3
"""Reader-only overlay of published Home/Contexts. Existing USB is byte-preserved."""
import argparse,copy,json,sys,zipfile,ctypes,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
for n in ('system','baseline','application','bootfs-tools','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args()
sys.path[:0]=[str(a.system.resolve()/'scripts'),str(a.bootfs_tools.resolve())]
from compose_shared_app_cohort import encode,digest,safe,write_tree
from assemble_shared_home_cohort import inventory,fingerprint,pack_candidate,providers
from assemble_shared_usb_cohort import decode_store,storage_record
from image_envelope import decode_image,MAGIC,HEADER,MAX_IMAGE
# Same already-published RTEIMG1/LZ4 block format, with a better build-time
# compressor. Runtime, canonical bytes and required decoder are unchanged.
lz4=ctypes.CDLL('liblz4.so.1')
lz4.LZ4_versionNumber.restype=ctypes.c_int
assert lz4.LZ4_versionNumber()==10904,'Pinned LZ4 1.9.4 build tool required'
lz4.LZ4_compressBound.argtypes=[ctypes.c_int];lz4.LZ4_compressBound.restype=ctypes.c_int
lz4.LZ4_compress_HC.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int]
lz4.LZ4_compress_HC.restype=ctypes.c_int
def encode_image(raw):
 assert raw.startswith(b'\x7fELF') and 52<=len(raw)<=MAX_IMAGE
 source=ctypes.create_string_buffer(raw);bound=lz4.LZ4_compressBound(len(raw));target=ctypes.create_string_buffer(bound)
 size=lz4.LZ4_compress_HC(source,target,len(raw),bound,12);assert size>0
 encoded=HEADER.pack(MAGIC,HEADER.size,1,len(raw),size,zlib.crc32(raw),0)+target.raw[:size]
 assert decode_image(encoded)==raw
 return encoded
from current_bootfs import build
from read_only_spiffs import read_image
lock=json.loads((ROOT/'reader/shared-baseline.json').read_text())
for asset in lock['assets']:
 raw=(a.baseline/asset['name']).read_bytes()
 assert len(raw)==asset['bytes'] and digest(raw)==asset['sha256'],'Published baseline changed'
elf=(a.application/'ebook_reader.elf').read_bytes()
manifest=(a.application/'ebook_reader.json').read_bytes();common=json.loads(manifest)
proof=json.loads((a.application/'build.json').read_text())
assert proof['sha256']==digest(elf) and proof['bytes']==len(elf)
assert json.loads(manifest)['version']=='0.2.0'
assert proof['exports']==sorted(['app_main','app_module_init','app_module_fini','risc_resident_app_descriptor_v1'])
assert common['requires']==[{'capability':'ui.scene','api':1}]
assert not a.output.exists();a.output.mkdir(parents=True)
results={}
for lane in ('watch','wave','x4'):
 raw=(a.baseline/f'product-{lane}-store.bin').read_bytes()
 stored=read_image(raw,len(raw));before=decode_store(stored)
 with zipfile.ZipFile(a.baseline/f'product-{lane}-sd-apps.zip') as z:
  assert len(z.namelist())==len(set(z.namelist()))
  sd={safe(n):z.read(n) for n in z.namelist()}
 out=dict(before);packed=dict(stored);after_sd=dict(sd)
 boot=json.loads(before['boot.json']);original_boot=copy.deepcopy(boot)
 available=providers(before,boot)
 assert ('ui.scene',1,0) in available
 rows=[r for r in boot['app_capabilities'] if r['manifest']=='ebook_reader.json']
 new=copy.deepcopy(common);new['runtime_lifecycle']='foreground'
 if lane=='x4':
  assert len(rows)==1 and rows[0]['image_source']=={'path':'Apps/ebook-reader/ebook_reader.elf','volume_instance':9}
  assert digest(sd['Apps/ebook-reader/ebook_reader.elf'])=='395eb0ad04b1c86b26f713b2c82e7d3f2a7a26dd802778bf4d355a5b55b351b2'
  old=json.loads(before['ebook_reader.json'])
  assert old['id']=='ebook-reader' and old['supported_file_types']==new['supported_file_types']
  row=copy.deepcopy(rows[0])
  boot['app_capabilities'][boot['app_capabilities'].index(rows[0])]=row
 else:
  assert not rows and 'ebook_reader.json' not in before and 'ebook_reader.elf' not in before
  row={'manifest':'ebook_reader.json','grants':[{'api':1,'capability':'ui.scene','instance_id':0}]}
  # These services are Runtime platform contracts on all selected full native
  # profiles, not board providers. Actual native admission checks the grants.
  row['grants'] += [{'api':1,'capability':n,'instance_id':0} for n in ('memory.heap','random.bytes','file.open')]
  if lane=='wave':
   assert ('storage.volume',1,12) in available
   row['grants'].append({'api':1,'capability':'storage.volume','instance_id':12})
   row['image_source']={'path':'Apps/ebook-reader/ebook_reader.elf','volume_instance':12}
  else:
   assert {k:v for k,v in available.items() if k[0]=='storage.volume'}=={('storage.volume',1,0):'pkg-internal/manifest.json'},'Watch user storage changed'
   # The internal installation volume is not a user book volume. Do not grant it.
  boot['app_capabilities'].append(row)
 for g in row['grants']:
  k=(g['capability'],g['api'],g['instance_id'])
  assert k in available or k in [('memory.heap',1,0),('random.bytes',1,0),('file.open',1,0)]
 # Runtime role only changes if the selected baseline actually uses resident policy.
 if 'resident_shell' in boot:
  rs=boot['resident_shell'];assert isinstance(rs.get('foreground'),list)
  oldrs=copy.deepcopy(rs)
  if 'ebook_reader.elf' in rs.get('legacy',[]):rs['legacy'].remove('ebook_reader.elf')
  if 'ebook_reader.elf' not in rs['foreground']:rs['foreground'].append('ebook_reader.elf')
  assert {k:v for k,v in rs.items() if k not in ('foreground','legacy')}=={k:v for k,v in oldrs.items() if k not in ('foreground','legacy')}
 assert {k:v for k,v in boot.items() if k not in ('app_capabilities','resident_shell')}=={k:v for k,v in original_boot.items() if k not in ('app_capabilities','resident_shell')}
 assert [r for r in boot['app_capabilities'] if r['manifest']!='ebook_reader.json']==[r for r in original_boot['app_capabilities'] if r['manifest']!='ebook_reader.json']
 out['boot.json']=encode(boot);out['ebook_reader.json']=encode(new)
 packed['boot.json']=out['boot.json'];packed['ebook_reader.json']=out['ebook_reader.json']
 if lane in ('wave','x4'):
  after_sd['Apps/ebook-reader/ebook_reader.elf']=elf
  after_sd['Apps/ebook-reader/manifest.json']=out['ebook_reader.json']
  for source in sorted((a.application/'sd').rglob('*')):
   if not source.is_file():continue
   name=source.relative_to(a.application/'sd').as_posix();data=source.read_bytes()
   assert name.startswith('fonts/')
   if name in sd:assert sd[name]==data,'Existing font asset differs: '+name
   after_sd[name]=data
 else:out['ebook_reader.elf']=elf;packed['ebook_reader.elf']=elf
 allowed={'boot.json','ebook_reader.json'}|({'ebook_reader.elf'} if lane=='watch' else set())
 allowed_sd={'Apps/ebook-reader/ebook_reader.elf','Apps/ebook-reader/manifest.json'}
 assert not(set(before)-set(out)) and not(set(sd)-set(after_sd))
 assert all(out[n]==b for n,b in before.items() if n not in allowed)
 assert all(packed[n]==b for n,b in stored.items() if n not in allowed)
 assert all(after_sd[n]==b for n,b in sd.items() if n not in allowed_sd)
 assert all(n in sd or n in allowed_sd or n.startswith('fonts/') for n in after_sd)
 # Storage representation may change only for ordinary internal application
 # images supported by the already-published Runtime decoder. Canonical bytes,
 # provider images, saved data, manifests and authority remain invariant.
 representation_changes=[]
 def fit():
  try:build(packed,len(raw));return True
  except Exception as error:
   if type(error).__name__=='SpiffsFullError':return False
   if isinstance(error,ValueError) and str(error)=='Current bootfs lacks SPIFFS free-block reserve':return False
   raise
 if not fit():
  assert lane=='watch','No unqualified decoder or geometry change'
  packed['ebook_reader.elf']=encode_image(elf)
  if not fit():
   driver_images={json.loads(out[r['manifest']])['file_name'] for r in boot.get('drivers',[])}
   choices=[]
   for app in boot['app_capabilities']:
    if 'image_source' in app:continue
    m=json.loads(out[app['manifest']])
    name=safe(str(Path(app['manifest']).parent/m['file_name']))
    if m.get('type')!='application' or m.get('provides') or name in driver_images:continue
    if name not in out or not out[name].startswith(b'\x7fELF'):continue
    encoded=encode_image(out[name]);saving=len(packed[name])-len(encoded)
    if saving>0:choices.append((-saving,name,encoded))
   for _,name,encoded in sorted(choices):
    packed[name]=encoded;representation_changes.append(name)
    if fit():break
   else:raise ValueError('Ordinary application encoding cannot preserve SPIFFS reserve: '+json.dumps({'store_bytes':len(raw),'canonical_reader_bytes':len(elf),'stored_reader_bytes':len(packed['ebook_reader.elf']),'stored_file_bytes':sum(map(len,packed.values())),'representations':representation_changes}))
 assert all(packed[n]==data for n,data in stored.items() if n not in allowed and n not in representation_changes)
 assert decode_store(packed)==out
 encoding=storage_record(out,packed)
 if encoding:encoding['selection']='Reader first, then descending positive savings among ordinary internal applications including existing envelopes, LZ4 HC 1.9.4 level12 in existing RTEIMG1 codec; stop at first fixed-size pack preserving reserve'
 path=a.output/lane
 for name,entries in [('before',before),('before-stored',stored),('before-sd',sd),('after',out),('stored',packed),('after-sd',after_sd)]:
  write_tree(path/name,entries)
 result={'inventory_sha256':fingerprint(inventory(before,sd)),'shared_elf_sha256':digest(elf),
  'installed_manifest_sha256':digest(out['ebook_reader.json']),'baseline_store_sha256':digest(raw),
  'image_storage':encoding,'changed_internal':[n for n in sorted(out) if out[n]!=before.get(n)],
  'changed_stored':[n for n in sorted(packed) if packed[n]!=stored.get(n)],
  'changed_sd':[n for n in sorted(after_sd) if after_sd[n]!=sd.get(n)],
  'unrelated_canonical_bytes_preserved':True,'unrelated_stored_bytes_preserved':not bool(representation_changes),
  'ordinary_application_representation_changes':representation_changes,
  'unrelated_sd_bytes_preserved':True,'provider_and_saved_data_ownership_preserved':True,
  'installed_policy':row,'resident_policy':boot.get('resident_shell')}
 result.update(pack_candidate(path,packed,after_sd,len(raw),build,read_image))
 results[lane]=result
 print('READER PRODUCT '+lane+' '+json.dumps(result),flush=True)
(a.output/'assembly.json').write_bytes(encode({'schema':1,'products':results,'packed':True,
 'shared_elf_sha256':digest(elf),'baseline_tag':lock['tag'],'physical_testing':False,
 'usb_candidate_included':False,'scope':'Reader only; all published Home, Contexts, Bringup and existing USB canonical bytes unchanged'}))

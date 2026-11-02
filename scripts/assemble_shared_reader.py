#!/usr/bin/env python3
"""Shared Reader overlay; explicit runtime-invariant metadata compaction preserves capacity."""
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
 candidates=[]
 for level in range(3,13):
  size=lz4.LZ4_compress_HC(source,target,len(raw),bound,level);assert size>0
  candidates.append((size,level,target.raw[:size]))
 size,level,block=min(candidates,key=lambda x:(x[0],x[1]))
 encoded=HEADER.pack(MAGIC,HEADER.size,1,len(raw),size,zlib.crc32(raw),0)+block
 assert decode_image(encoded)==raw
 return encoded
from current_bootfs import build
from read_only_spiffs import read_image
from elf_runtime_equivalence import require_runtime_equal
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
# Exact original ELF sidecars and loader-visible proofs replace byte-identity
# only for explicitly listed non-runtime metadata compactions. The deterministic
# old-hash -> new-hash map is applied to EVERY product occurrence, preserving
# already-converged application identities across all three deployments.
options=json.loads((a.baseline/'metadata-options.json').read_text())
watch_raw=(a.baseline/'product-watch-store.bin').read_bytes()
watch_before=decode_store(read_image(watch_raw,len(watch_raw)))
compactions={}
sidecars=a.output/'metadata-sidecars';sidecars.mkdir()
for option in options['apps']:
 if option['saving_lz4']<=0:continue
 original=watch_before[option['path']]
 old=digest(original);assert old==option['before_sha256']
 candidate=(a.baseline/'metadata-options'/(old+'.elf')).read_bytes()
 assert digest(candidate)==option['after_sha256'] and len(candidate)==option['after_bytes']
 witness=require_runtime_equal(original,candidate)
 assert witness==json.loads((a.baseline/'metadata-options'/(old+'.json')).read_text())
 (sidecars/(old+'.elf')).write_bytes(original)
 (sidecars/(old+'.json')).write_bytes(encode(witness))
 compactions[old]={'candidate':candidate,'proof':{'original_sha256':old,'candidate_sha256':digest(candidate),
  'original_sidecar':'metadata-sidecars/'+old+'.elf','runtime_witness':'metadata-sidecars/'+old+'.json',
  'runtime_witness_sha256':digest(encode(witness)),'watch_id':option['id']}}
assert compactions
identity_outputs={}
results={}
for lane in ('x4','wave','watch'):
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
 metadata_internal={};metadata_sd={}
 driver_images={safe(str(Path(r['manifest']).parent/json.loads(out[r['manifest']])['file_name'])) for r in boot.get('drivers',[])}
 internal_app_images=set()
 for policy in boot['app_capabilities']:
  m=json.loads(out[policy['manifest']])
  if 'image_source' not in policy and m.get('type')=='application' and not m.get('provides'):
   internal_app_images.add(safe(str(Path(policy['manifest']).parent/m['file_name'])))
 sd_app_images=set()
 for manifest_name,manifest_bytes in after_sd.items():
  if not manifest_name.startswith('Apps/') or not manifest_name.endswith('/manifest.json'):continue
  m=json.loads(manifest_bytes)
  if m.get('type')=='application' and not m.get('provides') and 'usb' not in m.get('id','').lower():
   sd_app_images.add(safe(str(Path(manifest_name).parent/m['file_name'])))
 for name,data in list(out.items()):
  if name not in internal_app_images:continue
  match=compactions.get(digest(data))
  if not match:continue
  assert name not in driver_images,'Provider alias cannot be compacted'
  assert name!='ebook_reader.elf'
  require_runtime_equal(data,match['candidate'])
  out[name]=match['candidate'];metadata_internal[name]=match['proof']
  packed[name]=encode_image(out[name]) if stored[name].startswith(MAGIC) else out[name]
 for name,data in list(after_sd.items()):
  if name not in sd_app_images:continue
  match=compactions.get(digest(data))
  if not match:continue
  assert not ('usb' in name.lower()),'Published USB remains exact'
  require_runtime_equal(data,match['candidate'])
  after_sd[name]=match['candidate'];metadata_sd[name]=match['proof']
 for original,current,eligible in ((before,out,internal_app_images),(sd,after_sd,sd_app_images)):
  for name,data in original.items():
   if name in eligible and data.startswith(bytes([127])+b'ELF') and name not in ('ebook_reader.elf','Apps/ebook-reader/ebook_reader.elf'):
    identity_outputs.setdefault(digest(data),set()).add(digest(current[name]))
 allowed={'boot.json','ebook_reader.json'}|({'ebook_reader.elf'} if lane=='watch' else set())|set(metadata_internal)
 allowed_sd={'Apps/ebook-reader/ebook_reader.elf','Apps/ebook-reader/manifest.json'}|set(metadata_sd)
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
   else:
    # Capacity diagnostic only, never a delivered image. Measure exact room for
    # this member using the same pinned builder, then restore the real bytes.
    saved=packed['ebook_reader.elf'];lo,hi=0,len(saved)
    while lo<hi:
     mid=(lo+hi+1)//2;packed['ebook_reader.elf']=bytes(mid)
     if fit():lo=mid
     else:hi=mid-1
    packed['ebook_reader.elf']=saved
    raise ValueError('Ordinary application encoding cannot preserve SPIFFS reserve: '+json.dumps({'store_bytes':len(raw),'canonical_reader_bytes':len(elf),'stored_reader_bytes':len(saved),'maximum_reader_member_bytes':lo,'shortfall_bytes':len(saved)-lo,'stored_file_bytes':sum(map(len,packed.values())),'representations':representation_changes}))

 assert all(packed[n]==data for n,data in stored.items() if n not in allowed and n not in representation_changes)
 assert decode_store(packed)==out
 encoding=storage_record(out,packed)
 if encoding:encoding['selection']='Reader first, then descending positive savings among ordinary internal applications including existing envelopes, minimum actual LZ4 HC 1.9.4 output across levels 3–12 in existing RTEIMG1 codec; stop at first fixed-size pack preserving reserve'
 path=a.output/lane
 for name,entries in [('before',before),('before-stored',stored),('before-sd',sd),('after',out),('stored',packed),('after-sd',after_sd)]:
  write_tree(path/name,entries)
 result={'inventory_sha256':fingerprint(inventory(before,sd)),'shared_elf_sha256':digest(elf),
  'installed_manifest_sha256':digest(out['ebook_reader.json']),'baseline_store_sha256':digest(raw),
  'image_storage':encoding,'changed_internal':[n for n in sorted(out) if out[n]!=before.get(n)],
  'changed_stored':[n for n in sorted(packed) if packed[n]!=stored.get(n)],
  'changed_sd':[n for n in sorted(after_sd) if after_sd[n]!=sd.get(n)],
  'unrelated_canonical_bytes_preserved':not bool(metadata_internal),'unrelated_stored_bytes_preserved':not bool(representation_changes or metadata_internal),
  'metadata_only_compaction':{'internal':metadata_internal,'sd':metadata_sd},'all_unrelated_runtime_sections_and_data_preserved':True,
  'ordinary_application_representation_changes':representation_changes,
  'unrelated_sd_bytes_preserved':not bool(metadata_sd),'unrelated_non_executable_data_bytes_preserved':True,'provider_and_saved_data_ownership_preserved':True,
  'installed_policy':row,'resident_policy':boot.get('resident_shell')}
 result.update(pack_candidate(path,packed,after_sd,len(raw),build,read_image))
 results[lane]=result
 # Preserve completed lane evidence even when a later fixed-store fit fails.
 # This partial receipt is never publishable (packed remains false).
 (a.output/'assembly.json').write_bytes(encode({'schema':1,'products':results,'packed':False,'shared_elf_sha256':digest(elf),'baseline_tag':lock['tag'],'physical_testing':False,'usb_candidate_included':False}))
 print('READER PRODUCT '+lane+' '+json.dumps(result),flush=True)
assert all(len(outputs)==1 for outputs in identity_outputs.values()),'Previously identical application ELF diverged across products'
(a.output/'assembly.json').write_bytes(encode({'schema':1,'products':results,'packed':True,
 'previously_identical_application_images_remain_identical':True,'metadata_only_compaction_proofs':{old:item['proof'] for old,item in compactions.items()},
 'shared_elf_sha256':digest(elf),'baseline_tag':lock['tag'],'physical_testing':False,
 'usb_candidate_included':False,'scope':'Complete shared Reader; unrelated executable runtime bytes and all provider/data bytes preserved. Explicit non-runtime metadata compaction has original ELF sidecars and exact loader-visible proofs, applied consistently across all product occurrences. Existing USB unchanged.'}))

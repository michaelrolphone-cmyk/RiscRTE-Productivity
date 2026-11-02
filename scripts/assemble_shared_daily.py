#!/usr/bin/env python3
"""Daily-only overlay of published Bringup233e; exact reviewed authority, fixed geometry."""
import argparse,copy,json,sys,zipfile
from pathlib import Path, PurePosixPath
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for n in ('system','baseline','application','bootfs-tools','plan','output'):
 p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args()
sys.path[:0]=[str(a.system.resolve()/'scripts'),str(a.bootfs_tools.resolve())]
from compose_shared_app_cohort import encode,digest,safe,write_tree
from assemble_shared_home_cohort import inventory,fingerprint,pack_candidate,providers,declarations,validate_grant_substitutions
from assemble_shared_usb_cohort import decode_store,storage_record
from image_envelope import encode_image,MAGIC
from image_storage import ordinary_images
from current_bootfs import build
from read_only_spiffs import read_image
lock=json.loads((ROOT/'docs/shared-daily/baseline.json').read_text())
assert lock['repository']=='michaelrolphone-cmyk/RiscRTE-Watch'
assert lock['tag']=='bringup-shared-233e43841483adf570ae833a88d9b6fc08d68f1a'
assert len(lock['assets'])==6 and len({v['name'] for v in lock['assets']})==6
for asset in lock['assets']:
 raw=(a.baseline/asset['name']).read_bytes()
 assert len(raw)==asset['bytes'] and digest(raw)==asset['sha256'],'Published baseline changed'
elf=(a.application/'daily_digest.elf').read_bytes()
manifest=(a.application/'daily_digest.json').read_bytes();common=json.loads(manifest)
proof=json.loads((a.application/'build.json').read_text())
assert elf.startswith(b'\x7fELF')
assert proof['sha256']==digest(elf) and proof['manifest_sha256']==digest(manifest)
assert set(proof['exports'])=={'app_main','risc_resident_app_descriptor_v1'}
assert common['id']=='daily-digest' and common['file_name']=='daily_digest.elf'
assert common['type']=='application' and common['version']=='0.2.0'
assert common['runtime_lifecycle']=='foreground'
assert common['requires']==[{'capability':'ui.scene','api':1}]
assert declarations(common,'optional')=={(c,1) for c in ('file.open','storage.volume','runtime.realtime','net.wifi','storage.key-value','net.http-client')}
plan=json.loads(a.plan.read_text())
assert set(plan)=={'watch','wave','x4'}
assert not a.output.exists();a.output.mkdir(parents=True)
results={}
for lane in ('watch','wave','x4'):
 raw=(a.baseline/f'product-{lane}-store.bin').read_bytes()
 stored=read_image(raw,len(raw));before=decode_store(stored)
 with zipfile.ZipFile(a.baseline/f'product-{lane}-sd-apps.zip') as z:
  assert len(z.namelist())==len(set(z.namelist()))
  sd={safe(n):z.read(n) for n in z.namelist()}
 spec=plan[lane]
 assert spec['inventory_sha256']==fingerprint(inventory(before,sd)),'Reviewed inventory changed'
 out=dict(before);packed=dict(stored);after_sd=dict(sd)
 boot=json.loads(before['boot.json']);original_boot=copy.deepcopy(boot)
 name=safe(spec['manifest']);image=safe(str(PurePosixPath(name).parent/'daily_digest.elf'))
 rows=[r for r in boot['app_capabilities'] if r['manifest']==name]
 identities=[r for r in boot['app_capabilities'] if json.loads(before[r['manifest']]).get('id')=='daily-digest']
 assert rows==spec['original_policy_rows'] and identities==rows
 assert len(rows)<=1 and len(spec['policy_rows'])==1
 row=copy.deepcopy(spec['policy_rows'][0]);assert row['manifest']==name
 route=spec['route'];target=safe(route['path'])
 assert route['storage'] in ('internal','sd')
 if rows:
  old=json.loads(before[name])
  assert old['id']=='daily-digest' and old['file_name']=='daily_digest.elf' and old['type']=='application'
  assert digest(before[name])==spec['manifest_sha256']
  new=copy.deepcopy(old)
  for key in ('requires','optional','version','architecture','entry','runtime_lifecycle'):new[key]=copy.deepcopy(common[key])
  oldrow=copy.deepcopy(rows[0]);newrow=copy.deepcopy(row)
  oldgrants=oldrow.pop('grants');newgrants=newrow.pop('grants')
  assert oldrow==newrow,'Route/namespace authority changed'
  available=providers(before,boot)
  bindings={r['manifest']:r.get('bindings',[]) for r in boot.get('drivers',[])}
  # Existing native grants need not occur in driver-only inventory. Preserve
  # them exactly; only explicit facade-backed UI substitutions may remove one.
  validate_grant_substitutions(oldgrants,newgrants,spec.get('grant_substitutions',[]),common,available,bindings)
  assert all(g in oldgrants or g=={'capability':'ui.scene','api':1,'instance_id':0} for g in newgrants),'Unexpected new authority'
  boot['app_capabilities'][boot['app_capabilities'].index(rows[0])]=row
 else:
  assert name not in before and image not in before and not spec.get('grant_substitutions')
  new=copy.deepcopy(common)
  assert route['storage']=='internal' and target==image,'New external installations require separate qualification'
  assert set(row)=={'manifest','grants'}
  boot['app_capabilities'].append(row)
 grants=row['grants'];assert len(grants)==len({encode(g) for g in grants})
 assert {'capability':'ui.scene','api':1,'instance_id':0} in grants
 assert ('ui.scene',1,0) in providers(before,boot)
 declared=declarations(common,'requires')|declarations(common,'optional')
 assert all((g['capability'],g['api']) in declared for g in grants),'Grant outside shared contract'
 for g in grants:
  if g['capability']=='storage.key-value':assert g['instance_id']==6,'Credential instance changed'
 source=row.get('image_source')
 if route['storage']=='sd':
  assert rows and source and source['path']==target and target in sd
  assert not source.get('internal_fallback'),'Dual image route needs explicit qualification'
  assert digest(sd[target])==spec['elf_sha256']
  after_sd[target]=elf
  sd_manifest=safe(str(PurePosixPath(target).parent/'manifest.json'))
  assert sd_manifest==spec['sd_manifest'] and sd_manifest in sd
  assert digest(sd[sd_manifest])==spec['sd_manifest_sha256']
  old_sd=json.loads(sd[sd_manifest])
  assert old_sd['id']=='daily-digest' and old_sd['file_name']=='daily_digest.elf'
  next_sd=copy.deepcopy(old_sd)
  for key in ('requires','optional','version','architecture','entry','runtime_lifecycle'):next_sd[key]=copy.deepcopy(common[key])
  after_sd[sd_manifest]=encode(next_sd)
 else:
  assert not source and target==image
  if rows:assert target in before and digest(before[target])==spec['elf_sha256']
  out[target]=elf;packed[target]=elf
 if 'resident_shell' in boot:
  rs=boot['resident_shell'];assert isinstance(rs.get('foreground'),list)
  previous=copy.deepcopy(rs)
  if image in rs.get('legacy',[]):rs['legacy'].remove(image)
  if image not in rs['foreground']:rs['foreground'].append(image)
  assert {k:v for k,v in rs.items() if k not in ('foreground','legacy')}=={k:v for k,v in previous.items() if k not in ('foreground','legacy')}
 assert {k:v for k,v in boot.items() if k not in ('app_capabilities','resident_shell')}=={k:v for k,v in original_boot.items() if k not in ('app_capabilities','resident_shell')}
 assert [r for r in boot['app_capabilities'] if r['manifest']!=name]==[r for r in original_boot['app_capabilities'] if r['manifest']!=name]
 out['boot.json']=encode(boot);out[name]=encode(new)
 packed['boot.json']=out['boot.json'];packed[name]=out[name]
 allowed={'boot.json',name}|({target} if route['storage']=='internal' else set())
 assert not(set(before)-set(out))
 assert set(out)-set(before)==(set() if rows else {name,image})
 assert all(out[n]==b for n,b in before.items() if n not in allowed)
 assert all(packed[n]==b for n,b in stored.items() if n not in allowed)
 assert set(after_sd)==set(sd)
 assert all(after_sd[n]==b for n,b in sd.items() if not(route['storage']=='sd' and n in (target,sd_manifest)))
 representation_changes=[]
 def fit():
  try:build(packed,len(raw));return True
  except Exception as error:
   if type(error).__name__=='SpiffsFullError':return False
   if isinstance(error,ValueError) and str(error)=='Current bootfs lacks SPIFFS free-block reserve':return False
   raise
 if not fit():
  assert lane=='watch','No unqualified decoder or geometry change'
  choices=[]
  providing_images={safe(str(PurePosixPath(r['manifest']).parent/json.loads(out[r['manifest']])['file_name'])) for r in boot['app_capabilities'] if json.loads(out[r['manifest']]).get('provides')}
  for candidate in ordinary_images(out)-providing_images:
   if candidate not in packed or packed[candidate]!=out[candidate] or not out[candidate].startswith(b'\x7fELF'):continue
   encoded=encode_image(out[candidate]);saving=len(out[candidate])-len(encoded)
   if saving>0:choices.append((candidate!=image,-saving,candidate,encoded))
  for _,__,candidate,encoded in sorted(choices):
   packed[candidate]=encoded;representation_changes.append(candidate)
   if fit():break
  else:raise ValueError('Ordinary application encoding cannot preserve SPIFFS reserve')
 assert all(packed[n]==b for n,b in stored.items() if b.startswith(MAGIC) and n not in allowed),'Existing unrelated envelope changed'
 assert all(packed[n]==b for n,b in stored.items() if n not in allowed and n not in representation_changes)
 assert decode_store(packed)==out
 encoding=storage_record(out,packed)
 if encoding:encoding['selection']='Preserve existing envelopes; Daily first then descending positive savings among raw ordinary internal applications; stop at fixed-size pack preserving reserve'
 path=a.output/lane
 for label,entries in [('before',before),('before-stored',stored),('before-sd',sd),('after',out),('stored',packed),('after-sd',after_sd)]:
  write_tree(path/label,entries)
 result={'inventory_sha256':fingerprint(inventory(before,sd)),'shared_elf_sha256':digest(elf),
  'installed_manifest_sha256':digest(out[name]),'baseline_store_sha256':digest(raw),
  'image_storage':encoding,'changed_internal':[n for n in sorted(out) if out[n]!=before.get(n)],
  'changed_stored':[n for n in sorted(packed) if packed[n]!=stored.get(n)],
  'changed_sd':[n for n in sorted(after_sd) if after_sd[n]!=sd.get(n)],
  'unrelated_canonical_bytes_preserved':True,
  'unrelated_stored_bytes_preserved':not any(n not in allowed for n in representation_changes),
  'ordinary_application_representation_changes':representation_changes,
  'unrelated_sd_bytes_preserved':True,'provider_and_saved_data_ownership_preserved':True,
  'installed_policy':row,'resident_policy':boot.get('resident_shell'),'daily_route':route,
  'sd_manifest':sd_manifest if route['storage']=='sd' else None,
  'reviewed_plan_sha256':digest(a.plan.read_bytes())}
 result.update(pack_candidate(path,packed,after_sd,len(raw),build,read_image));results[lane]=result
 print('DAILY PRODUCT '+lane+' '+json.dumps(result),flush=True)
(a.output/'assembly.json').write_bytes(encode({'schema':1,'products':results,'packed':True,
 'shared_elf_sha256':digest(elf),'baseline_tag':lock['tag'],'physical_testing':False,
 'usb_candidate_included':False,'reader_candidate_included':False,
 'scope':'Daily only; all published Home, Contexts, Bringup, USB and Reader canonical bytes unchanged'}))

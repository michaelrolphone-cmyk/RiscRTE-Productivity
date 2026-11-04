#!/usr/bin/env python3
"""Preserving shared-scene replacement over exact published product candidates.
The replacement is app-identity based, not device-layout based. Existing storage
namespaces, peer bytes, image representation and physical providers are retained.
"""
import argparse,copy,hashlib,json,struct,sys,zipfile
from pathlib import Path,PurePosixPath
ROOT=Path(__file__).resolve().parents[1]
LANES=('watch','wave','x4')
HOST_SHA='caff6c3cfde080baaeadfc9e8d13b1ef8e3eff7dc4c49a19f01aaf235494e145'
DELEGATED={'display.output','input.touch.raw','input.navigation','input.power-key',
 'power.sleep','display.power','board.battery','net.wifi','bluetooth.hci','motion.accel',
 'rtc.clock','runtime.realtime','alarm.service','telemetry.broadcast','contexts.service',
 'app.catalog','ui.text-input'}
def digest(data):return hashlib.sha256(data).hexdigest()
def encode(value):return (json.dumps(value,sort_keys=True,indent=2)+'\n').encode()
def safe(name):
 p=PurePosixPath(name)
 if not name or name=='.' or p.is_absolute() or '..' in p.parts or '\\' in name or str(p)!=name:raise ValueError('Unsafe path')
 return name
def pairs(m,key):return {(r['capability'],r['api']) for r in m.get(key,[])}
def read_zip(path):
 with zipfile.ZipFile(path) as z:
  names=z.namelist()
  if len(names)!=len(set(names)):raise ValueError('Duplicate archive entry')
  for n in names:safe(n.rstrip('/'))
  return {n:z.read(n) for n in names if not n.endswith('/')}
def write_tree(root,files):
 for n,b in files.items():
  p=root/safe(n);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
def descriptor(data):
 if data[:6]!=b'\x7fELF\x01\x01':raise ValueError('Expected little endian ELF32')
 off=struct.unpack_from('<I',data,32)[0];size,count=struct.unpack_from('<HH',data,46)
 sections=[struct.unpack_from('<10I',data,off+i*size) for i in range(count)]
 found=[]
 for sec in sections:
  if sec[1]!=2:continue
  strings=sections[sec[6]];table=data[strings[4]:strings[4]+strings[5]]
  for pos in range(sec[4],sec[4]+sec[5],sec[9]):
   name,value,length,info,other,index=struct.unpack_from('<IIIBBH',data,pos)
   if table[name:table.find(b'\0',name)]==b'risc_resident_app_descriptor_v1':
    if index>=count or length!=16:raise ValueError('Bad resident descriptor')
    target=sections[index];found.append(struct.unpack_from('<4I',data,target[4]+value-target[3]))
 if len(found)!=1:raise ValueError('Resident descriptor missing or ambiguous')
 return found[0]
def route(files,row):
 m=json.loads(files[safe(row['manifest'])])
 if 'image_source' in row:
  src=row['image_source']
  if type(src.get('volume_instance')) is not int or src['volume_instance']<=0:raise ValueError('Invalid volume authority')
  return 'sd',safe(src['path'])
 return 'internal',safe(str(PurePosixPath(row['manifest']).parent/m['file_name']))
def resident_policy(boot,files,sd,app_file):
 old=copy.deepcopy(boot.get('resident_shell'))
 if old is not None:
  if old.get('api')!=1 or old.get('host')!=boot['default_app'] or app_file not in old.get('foreground',[]) or app_file in old.get('legacy',[]):raise ValueError('Existing resident policy does not select app')
 # Only the independently published, identical shared Home is admitted here.
 hosts=[r for r in boot['app_capabilities'] if json.loads(files[r['manifest']])['file_name']==boot['default_app']]
 if len(hosts)!=1:raise ValueError('Ambiguous resident host')
 storage,path=route(files,hosts[0]);host=(sd if storage=='sd' else files)[path]
 if digest(host)!=HOST_SHA or descriptor(host)!=(1,16,1,0):raise ValueError('Unqualified resident host')
 h=json.loads(files[hosts[0]['manifest']])
 target=next(r for r in boot['app_capabilities'] if json.loads(files[r['manifest']])['file_name']==app_file)
 old_caps={(g['capability'],g['api']) for g in target['grants']}
 required={(cap,api) for cap,api in old_caps if cap in {'telemetry.broadcast','contexts.service','power.sleep','display.power','input.power-key'}}
 if any(cap=='alarm.service' for cap,api in old_caps):required.add(('alarm.service.facade',2))
 granted={(g['capability'],g['api']) for g in hosts[0]['grants']}
 if not required<=granted or not required<=pairs(h,'optional')|pairs(h,'requires'):raise ValueError('Host physical policy authority missing')
 if old is not None:return old,False
 peers=[json.loads(files[r['manifest']])['file_name'] for r in boot['app_capabilities']]
 if len(peers)!=len(set(peers)):raise ValueError('Ambiguous executable identity')
 # Every peer keeps ordinary handoff behavior. Only this converted app borrows
 # the resident host; separate foreground promotions require their own proof.
 boot['resident_shell']={'api':1,'host':boot['default_app'],'foreground':[app_file],
  'legacy':[p for p in peers if p not in (app_file,boot['default_app'])]}
 return old,True
def replace_scene_application(before,sd0,app,manifest):
 app_id=manifest['id'];elf=manifest['file_name']
 if descriptor(app)!=(1,16,2,0) or pairs(manifest,'requires')!={('ui.scene',1)}:raise ValueError('Invalid shared foreground')
 out,sd=dict(before),dict(sd0);boot=json.loads(before['boot.json'])
 matches=[r for r in boot['app_capabilities'] if json.loads(before[r['manifest']]).get('id')==app_id]
 if len(matches)!=1:raise ValueError('Ambiguous application identity')
 original=copy.deepcopy(matches[0]);row=copy.deepcopy(original);path=safe(row['manifest'])
 installed=json.loads(before[path])
 if installed.get('type')!='application' or installed.get('file_name')!=elf:raise ValueError('Application identity differs')
 original_route=route(before,original)
 for other in boot['drivers']+boot['app_capabilities']:
  if other is matches[0]:continue
  if route(before,other)==original_route:raise ValueError('Shared executable route')
  om=json.loads(before[other['manifest']])
  if ('internal',str(PurePosixPath(other['manifest']).parent/om['file_name']))==original_route:raise ValueError('Overlapping fallback')
 old_resident,enabled=resident_policy(boot,before,sd0,elf)
 storage=[copy.deepcopy(g) for g in row['grants'] if g['capability'].startswith('storage.')]
 if not any(g['capability']=='storage.app-data' and g['instance_id']==1 for g in storage) or not any(g['capability']=='storage.key-value' and g['instance_id']==1 for g in storage):raise ValueError('Saved Timecard namespace unavailable')
 if any(g['capability'] not in DELEGATED and not g['capability'].startswith('storage.') for g in row['grants']):raise ValueError('Unreviewed authority removal')
 row['grants']=copy.deepcopy(storage)
 for capability in ('ui.scene','time.civil'):
  owners=[r for r in boot['drivers'] if (capability,1) in pairs(json.loads(before[r['manifest']]),'provides')]
  if len(owners)!=1 or owners[0].get('instance_id',0)!=0:raise ValueError('Provider missing or ambiguous '+capability)
  row['grants'].append({'capability':capability,'api':1,'instance_id':0})
 derived=copy.deepcopy(installed)
 for key in ('version','entry','architecture','requires','optional'):derived[key]=copy.deepcopy(manifest.get(key,[]))
 declared=pairs(derived,'requires')|pairs(derived,'optional')
 for g in storage:
  cap=(g['capability'],g['api'])
  if cap not in declared:derived['optional'].append({'capability':cap[0],'api':cap[1]});declared.add(cap)
 if len(row['grants'])>32 or len({encode(g) for g in row['grants']})!=len(row['grants']):raise ValueError('Invalid grants')
 out[path]=encode(derived);kind,old_path=original_route;target=sd if kind=='sd' else out
 if old_path not in target:raise ValueError('Missing executable')
 changed={path,'boot.json'};sd_changed=set();removed=set();added=set();new_path=old_path
 if kind=='sd':
  folder=PurePosixPath(old_path).parent;sidecar=str(folder/'manifest.json')
  if sidecar not in sd or json.loads(sd[sidecar])!=installed:raise ValueError('Sidecar differs')
  if folder.name.startswith('image-'):
   if folder.name!='image-'+digest(sd[old_path]) or {n for n in sd if n.startswith(str(folder)+'/')}!={old_path,sidecar}:raise ValueError('Content address differs')
   new_folder=folder.parent/('image-'+digest(app));new_path=str(new_folder/elf);new_sidecar=str(new_folder/'manifest.json')
   if new_path in sd or new_sidecar in sd:raise ValueError('Content address collision')
   del sd[old_path];del sd[sidecar];removed={old_path,sidecar};added={new_path,new_sidecar}
   sd[new_path],sd[new_sidecar]=app,out[path];row['image_source']['path']=new_path
  else:sd[old_path],sd[sidecar]=app,out[path];sd_changed={old_path,sidecar}
 else:out[old_path]=app;changed.add(old_path)
 boot['app_capabilities'][boot['app_capabilities'].index(matches[0])]=row;out['boot.json']=encode(boot)
 restored=copy.deepcopy(boot);restored['app_capabilities'][restored['app_capabilities'].index(row)]=original
 if old_resident is None:restored.pop('resident_shell',None)
 else:restored['resident_shell']=old_resident
 if restored!=json.loads(before['boot.json']):raise ValueError('Unrelated authority changed')
 if [g for g in row['grants'] if g['capability'].startswith('storage.')]!=storage:raise ValueError('Storage changed')
 if set(out)!=set(before) or set(sd)!=(set(sd0)-removed)|added:raise ValueError('File set differs')
 if any(out[n]!=b for n,b in before.items() if n not in changed) or any(sd[n]!=b for n,b in sd0.items() if n not in sd_changed|removed):raise ValueError('Unrelated bytes changed')
 return out,sd,{'application':app_id,'app_sha256':digest(app),'original_app_row':original,'app_row':row,
  'app_route':{'storage':kind,'path':new_path},'storage_authority_unchanged':True,'providers_unchanged':True,
  'resident_host_enabled':enabled,'resident_policy':boot['resident_shell'],
  'changed_internal':sorted(n for n in changed if out[n]!=before[n]),'changed_sd':sorted(sd_changed),
  'removed_sd':sorted(removed),'added_sd':sorted(added),'physical_execution':False}
def run(application,baseline,system,bootfs,output):
 sys.path[:0]=[str(system/'scripts'),str(bootfs)]
 from image_envelope import MAGIC,decode_image,encode_image,receipt as envelope_receipt
 from read_only_spiffs import read_image
 from current_bootfs import build
 from assemble_shared_home_cohort import pack_candidate,inventory,fingerprint
 lock=json.loads((ROOT/'docs/shared-timecard/baseline.json').read_text())
 for r in lock['assets']:
  b=(baseline/safe(r['name'])).read_bytes()
  if len(b)!=r['size'] or digest(b)!=r['sha256']:raise ValueError('Baseline differs')
 manifest=json.loads((application/'timecard.json').read_text());app=(application/'timecard.elf').read_bytes()
 receipt=json.loads((application/'build.json').read_text())
 if receipt['sha256']!=digest(app) or receipt['size_bytes']!=len(app):raise ValueError('Application receipt differs')
 output.mkdir(parents=True,exist_ok=False);results={}
 for lane in LANES:
  raw=(baseline/(lane+'-store.bin')).read_bytes();stored0=read_image(raw,len(raw))
  before={n:decode_image(b) if b.startswith(MAGIC) else b for n,b in stored0.items()}
  sd0=read_zip(baseline/(lane+'-sd-apps.zip'));out,sd,r=replace_scene_application(before,sd0,app,manifest)
  stored={n:(encode_image(b) if stored0[n].startswith(MAGIC) else b) if b!=before[n] else stored0[n] for n,b in out.items()}
  if {n:decode_image(b) if b.startswith(MAGIC) else b for n,b in stored.items()}!=out:raise ValueError('Envelope recovery differs')
  dest=output/lane
  for label,entries in (('before',before),('before-stored',stored0),('before-sd',sd0),('after',out),('stored',stored),('after-sd',sd)):write_tree(dest/label,entries);(dest/label).mkdir(exist_ok=True)
  encoded={n:envelope_receipt(out[n],b) for n,b in stored.items() if b.startswith(MAGIC)}
  if encoded:r['image_storage']={'schema':1,'format':'RTEIMG1','images':encoded,'canonical_files':{n:digest(b) for n,b in sorted(out.items())},'stored_files':{n:digest(b) for n,b in sorted(stored.items())},'canonical_recovery_verified':True}
  r['inventory_sha256']=fingerprint(inventory(before,sd0));r.update(pack_candidate(dest,stored,sd,len(raw),build,read_image));results[lane]=r
 (output/'assembly.json').write_bytes(encode({'schema':1,'products':results,'shared_timecard':digest(app),'packed':True,'baseline_release':lock['release'],'physical_tested':False}))
 print('Shared app replacement preserves peer bytes and storage authority:',digest(app))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('application','baseline','system','bootfs-tools','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();run(a.application,a.baseline,a.system,a.bootfs_tools,a.output)

#!/usr/bin/env python3
"""Stage Lists and its shared providers against an existing Watch/X4 store.

Preserves existing apps, grants and user data. Produces a launcher catalog input
and component stage; the product's normal cohort/image builder remains required.
"""
import argparse,hashlib,json,shutil
from pathlib import Path,PurePosixPath
ROOT=Path(__file__).resolve().parents[1]
def require(ok,message):
 if not ok:raise ValueError(message)
def digest(blob):return hashlib.sha256(blob).hexdigest()
def encoded(value):return (json.dumps(value,indent=2,sort_keys=True)+'\n').encode()
def safe(name):
 require(isinstance(name,str) and name and not name.startswith('/') and '\\' not in name and all(x not in ('.','..') for x in PurePosixPath(name).parts),'Unsafe store path')
 return name

def package(folder,application=False):
 manifest=json.loads((folder/('lists.json' if application else 'manifest.json')).read_bytes());receipt=json.loads((folder/'build.json').read_bytes())
 name=safe(manifest['file_name']);require('/' not in name,'Package ELF must be local');blob=(folder/name).read_bytes()
 require((manifest['id'],manifest['version'])==(receipt['id'],receipt['version']) and receipt['sha256']==digest(blob) and receipt['size_bytes']==len(blob),'Package/receipt mismatch')
 require(blob[:7]==b'\x7fELF\x01\x01\x01' and blob[16:20]==b'\x03\x00\x5e\x00','Expected Xtensa ET_DYN')
 return manifest,blob,receipt

def compose(store,system,clocks,lists,profile,catalog,output):
 require(not output.exists(),'Choose a new output directory')
 config=json.loads(profile.read_bytes());require(config.get('schema')=='nova7.lists.deployment.v1' and config['target'] in ('watch','x4'),'Unknown deployment profile')
 original={}
 for p in store.rglob('*'):
  require(not p.is_symlink(),'Store links are not supported')
  if p.is_file():original[safe(p.relative_to(store).as_posix())]=p.read_bytes()
 result=dict(original);boot=json.loads(result['boot.json']);namespace=config['app_data_namespace'];require(type(namespace) is int and namespace>0,'Invalid namespace')
 require(not any(row.get('manifest')=='lists.json' for row in boot['app_capabilities']),'Lists already installed; select an explicit upgrade recipe')
 for row in boot['app_capabilities']:
  for grant in row.get('grants',[]):require(not(grant.get('capability') in ('storage.app-data','storage.shared-data') and grant.get('instance_id')==namespace),'Lists namespace belongs to another app')
 for row in boot['drivers']:
  for binding in row.get('app_data',[]):require(binding.get('namespace')!=namespace,'Lists namespace belongs to a provider')
 records=[]
 def install_provider(folder,capability,bindings=None):
  manifest,blob,receipt=package(folder);require({'capability':capability,'api':1} in manifest['provides'],'Unexpected provider contract')
  found=[]
  for row in boot['drivers']:
   old=json.loads(result[safe(row['manifest'])])
   if {'capability':capability,'api':1} in old.get('provides',[]):found.append((row,old))
  require(len(found)<=1,'Ambiguous existing provider '+capability)
  if found:
   row,old=found[0];require(old['id']==manifest['id'],'Different provider selected for '+capability)
   require(tuple(map(int,old['version'].split('.')))<=tuple(map(int,manifest['version'].split('.'))),'Provider downgrade rejected')
   name=row['manifest'];folder_name=str(PurePosixPath(name).parent)
  else:
   folder_name=manifest['id'];name=folder_name+'/manifest.json';row={'manifest':name};boot['drivers'].append(row)
  result[name]=encoded(manifest);result[folder_name+'/'+manifest['file_name']]=blob
  if bindings:row['key_value']=bindings
  for p in (folder/'licenses').rglob('*'):
   if p.is_file():result[folder_name+'/licenses/'+p.relative_to(folder/'licenses').as_posix()]=p.read_bytes()
  records.append(receipt)
 install_provider(system/'scene-host','ui.scene')
 install_provider(system/config['presentation'],'ui.presentation-profile')
 bindings=None
 if config['target']=='x4':
  # Follow the already admitted timezone preference, rather than guessing a
  # storage namespace or silently granting write authority.
  candidates=[]
  for row in boot['drivers']:
   for b in row.get('key_value',[]):
    if b.get('key')=='time_zone':candidates.append(b['namespace'])
  require(candidates and len(set(candidates))==1,'Cannot resolve the installed timezone binding')
  bindings=[{'key':'time_zone','namespace':candidates[0],'access':'read'}]
 install_provider(clocks/config['target'],'time.civil',bindings)
 manifest,blob,receipt=package(lists/'lists',True);result['lists.json']=encoded(manifest);result['lists.elf']=blob;records.append(receipt)
 boot['app_capabilities'].append({'manifest':'lists.json','grants':[{'capability':n,'api':1,'instance_id':namespace if n=='storage.app-data' else 0} for n in ('ui.scene','storage.app-data','time.civil')]})
 if 'resident_shell' in boot:boot['resident_shell']['foreground'].append('lists.elf')
 require(len(boot['drivers'])<=config['provider_capacity'],'Product native provider capacity is insufficient')
 result['boot.json']=encoded(boot);result.pop('cohort.json',None)
 selected=json.loads(catalog.read_bytes());require(set(selected)=={'apps'},'Expected explicit installed launcher catalog')
 names=[row['file_name'] for row in selected['apps']];require(len(names)==len(set(names)) and 'lists.elf' not in names,'Catalog duplicates Lists')
 require(all(name in result for name in names),'Catalog contains an app outside this store')
 selected['apps'].append({k:manifest[k] for k in ('display_name','file_name','icon')})
 require(len(selected['apps'])<=40,'Selected launcher capacity exceeded')
 changed=[name for name in result if original.get(name)!=result[name]]
 # In particular there is no copied lists.bin or formatting operation here.
 output.mkdir(parents=True)
 for name,blob in result.items():p=output/'store'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(blob)
 (output/'launcher-catalog.json').write_bytes(encoded(selected))
 record={'schema':'nova7.lists.stage.v1','profile':config,'provider_count':len(boot['drivers']),'packages':records,'changed_files':sorted(changed),'removed_files':['cohort.json'] if 'cohort.json' in original else [],'source_boot_sha256':digest(original['boot.json']),'boot_sha256':digest(result['boot.json']),'launcher_catalog_sha256':digest(encoded(selected)),'launcher_rebuild_required':True,'native_capacity_verification_required':True,'product_cohort_binding_required':True,'flashable':False,'physical_testing':'not performed'}
 (output/'stage.json').write_bytes(encoded(record));return record
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('store','system-packages','clock-packages','lists-packages','profile','catalog','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();r=compose(a.store,a.system_packages,a.clock_packages,a.lists_packages,a.profile,a.catalog,a.output)
 print(json.dumps({'providers':r['provider_count'],'changed':r['changed_files'],'flashable':False},indent=2))

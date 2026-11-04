#!/usr/bin/env python3
"""Package a locally qualified Points source delta and exact product artifacts.

This command never publishes, pushes, or changes release channels.
"""
import argparse,hashlib,json,zipfile,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def digest(data):return hashlib.sha256(data).hexdigest()
def sha(path):return digest(path.read_bytes())
def blob(data):return hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
def archive(path,entries,modes=None):
 with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for name,data in sorted(entries.items()):
   i=zipfile.ZipInfo(name,(1980,1,1,0,0,0));i.compress_type=zipfile.ZIP_DEFLATED;i.external_attr=int((modes or {}).get(name,'100644'),8)<<16;z.writestr(i,data)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('build','normal','sanitized','assembly','assembly-tests','admission','admission-sanitized','output'):p.add_argument('--'+k,type=Path,required=True)
 for k in ('runtime','system','utilities'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
 build=json.loads((a.build/'build.json').read_text());elf=a.build/'points_in_time.elf'
 if sha(elf)!=build['sha256']:raise ValueError('ELF differs')
 for key,path in [('source_lock_sha256',ROOT/'sdk/points-scene-sources.json'),('builder_sha256',ROOT/'tools/build_points_scene.py'),('manifest_sha256',ROOT/'Apps/points_scene.json')]:
  if sha(path)!=build[key]:raise ValueError('Build input differs '+str(path))
 if set(build['product_selections'])!={'watch','wave','x4'}:raise ValueError('Product selections missing')
 for lane,row in build['product_selections'].items():
  if sha(a.build/'selections'/lane/'points_in_time.elf')!=build['sha256'] or row['elf_sha256']!=build['sha256'] or sha(a.build/'selections'/lane/'points_in_time.json')!=row['manifest_sha256']:raise ValueError('Selection differs '+lane)
 for filename,value in build['dependency_sha256'].items():
  if sha(Path(filename))!=value:raise ValueError('Source differs '+filename)
 for directory,sanitized in [(a.normal,False),(a.sanitized,True)]:
  q=json.loads((directory/'qualification.json').read_text())
  if q['status']!='passed' or q['sanitizers']!=sanitized or q['canonical_elf_sha256']!=build['sha256'] or q['build_receipt_sha256']!=sha(a.build/'build.json'):raise ValueError('Host qualification differs')
  if sha(ROOT/'tools/test_points_scene.py')!=q['runner_sha256']:raise ValueError('Host runner differs')
  for filename,value in q['dependency_sha256'].items():
   if sha(Path(filename))!=value:raise ValueError('Test source differs '+filename)
 assembly=json.loads((a.assembly/'assembly.json').read_text());tests=json.loads(a.assembly_tests.read_text())
 if assembly['shared_points']!=build['sha256'] or tests['app_sha256']!=build['sha256'] or not tests['passed']:raise ValueError('Assembly qualification differs')
 for key,path in [('assembler_sha256',ROOT/'tools/assemble_points_scene.py'),('test_sha256',ROOT/'test/test_assemble_points_scene.py'),('baseline_lock_sha256',ROOT/'docs/shared-points/baseline.json')]:
  if sha(path)!=tests.get(key):raise ValueError('Assembly test input differs '+str(path))
 for lane,row in assembly['products'].items():
  if row['app_sha256']!=build['sha256'] or sha(a.assembly/lane/'store.bin')!=row['store_sha256'] or sha(a.assembly/lane/'sd-apps.zip')!=row['sd_archive_sha256']:raise ValueError('Product differs '+lane)
 for directory,sanitized in [(a.admission,False),(a.admission_sanitized,True)]:
  q=json.loads((directory/'receipt.json').read_text())
  if q['sanitized']!=sanitized or set(q['products'])!={'watch','wave','x4'}:raise ValueError('Admission qualification differs')
  if q['prepare_only'] or q['product_binding']['assembly_sha256']!=sha(a.assembly/'assembly.json'):raise ValueError('Admission assembly differs')
  # Exact inventories bind canonical, stored and SD bytes to the actual run.
  for lane,row in q['products'].items():
   for which,expected in q['candidate_input_files'][lane].items():
    if any(p.is_symlink() for p in (a.assembly/lane/which).rglob('*')):raise ValueError('Symlink in admission tree')
    actual={p.relative_to(a.assembly/lane/which).as_posix():sha(p) for p in (a.assembly/lane/which).rglob('*') if p.is_file()}
    if actual!=expected:raise ValueError('Admission inventory differs '+lane+'/'+which)
    key='after_sd' if which=='after-sd' else which
    bound=digest((json.dumps(actual,sort_keys=True,separators=(',',':'))+'\n').encode())
    if bound!=q['product_binding']['products'][lane][key+'_tree_sha256']:raise ValueError('Admission tree binding differs')
   if q['product_binding']['products'][lane]['inventory_sha256']!=assembly['products'][lane]['inventory_sha256']:raise ValueError('Baseline inventory binding differs')
   route=assembly['products'][lane]['app_route'];which='after-sd' if route['storage']=='sd' else 'after'
   if q['candidate_input_files'][lane][which][route['path']]!=build['sha256']:raise ValueError('Admission ELF differs '+lane)
   results=row['results']
   if len(results)!=1 or results[0]['input']!='after':raise ValueError('Admission was not executed')
   r=results[0]['result']
   if not r['prepared'] or not r['cohort_validated'] or not r['elf_count'] or r['hardware_calls'] or r['storage_calls'] or (lane=='x4' and not r.get('retained_sd_serial_graph_bound')):raise ValueError('Admission failed '+lane)
 tree=json.loads((ROOT/'docs/shared-points/productivity-base-tree.json').read_text());base={r['path']:r for r in tree['tree'] if r['type']=='blob'}
 files={};modes={};changes=[]
 for path in ROOT.rglob('*'):
  if not path.is_file():continue
  rel=path.relative_to(ROOT)
  if any(x in ('build','.git','__pycache__') for x in rel.parts) or path.suffix=='.pyc':continue
  data=path.read_bytes();name=rel.as_posix();files['Productivity/'+name]=data;modes['Productivity/'+name]=base.get(name,{}).get('mode','100644')
  if name not in base or blob(data)!=base[name]['sha']:
   changes.append({'path':name,'mode':base.get(name,{}).get('mode','100644'),'action':'update' if name in base else 'add','base_blob':base.get(name,{}).get('sha'),'sha256':digest(data),'bytes':len(data)})
 publication={'schema':1,'repository':'michaelrolphone-cmyk/RiscRTE-Productivity','base_commit':'bb8d0d99b3a857103d2b01e6b4c756b913aa14d3','base_tree':tree['sha'],'branch_suggestion':'work/shared-points-20261104','changes':changes,'deletions':[],'published':False,'dependency_publication':{'repository':'michaelrolphone-cmyk/RiscRTE-Utilities','commit':'e817612532beb613376d69a502cd1e11b94ac8c4','status':'published'}}
 (a.output/'publication-plan.json').write_text(json.dumps(publication,indent=2)+'\n')
 dependency_inventory={}
 for role,pin in build['source_pins'].items():
  repository=getattr(a,role.lower());root=a.build/'sources'/role
  raw=subprocess.check_output(['git','-C',str(repository),'ls-tree','-r','-z',pin['commit']])
  tracked={}
  for entry in raw.split(b'\0'):
   if not entry:continue
   meta,name=entry.split(b'\t',1);mode,kind,oid=meta.decode().split();name=name.decode()
   if kind!='blob':continue
   path=root/name
   if path.is_symlink() or not path.is_file():raise ValueError('Unsupported/missing archived dependency '+role+'/'+name)
   data=path.read_bytes()
   if blob(data)!=oid:raise ValueError('Dependency differs from pinned Git tree '+role+'/'+name)
   tracked[name]={'git_blob':oid,'sha256':digest(data),'mode':mode}
   files['Dependencies/'+role+'/'+name]=data;modes['Dependencies/'+role+'/'+name]=mode
  actual={path.relative_to(root).as_posix() for path in root.rglob('*') if path.is_file() and '__pycache__' not in path.parts and path.suffix!='.pyc'}
  if actual!=set(tracked):raise ValueError('Untracked archived dependency files '+role)
  dependency_inventory[role]={'commit':pin['commit'],'files':tracked}
 files['dependency-archive-inventory.json']=(json.dumps(dependency_inventory,indent=2,sort_keys=True)+'\n').encode()
 archive(a.output/'source.zip',files,modes)
 evidence={}
 def add(path,name):evidence[name]=path.read_bytes()
 add(elf,'application/points_in_time.elf');add(a.build/'build.json','application/build.json')
 for path in (a.build/'selections').rglob('*'):
  if path.is_file():add(path,'application/selections/'+path.relative_to(a.build/'selections').as_posix())
 for label,directory in [('host-normal',a.normal),('host-sanitized',a.sanitized),('admission-normal',a.admission),('admission-sanitized',a.admission_sanitized)]:
  for path in directory.rglob('*'):
   if path.is_file() and path.suffix in ('.json','.log','.txt','.cmake'):add(path,label+'/'+path.relative_to(directory).as_posix())
 add(a.assembly/'assembly.json','assembly/assembly.json');add(a.assembly_tests,'assembly/tests.json')
 for lane in assembly['products']:
  for filename in ('store.bin','sd-apps.zip'):add(a.assembly/lane/filename,'products/'+lane+'/'+filename)
 archive(a.output/'qualification.zip',evidence)
 result={'schema':1,'status':'locally-qualified-publication-pending','canonical_elf_sha256':build['sha256'],'application_bytes':elf.stat().st_size,'source_published':False,'hardware_tested':False,'files':{p.name:{'sha256':sha(p),'bytes':p.stat().st_size} for p in a.output.iterdir() if p.is_file()},'sources':build['source_pins'],'production_changes':len(changes),'assembly_tests':tests}
 (a.output/'points-delivery.json').write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
 print(json.dumps({'files':result['files'],'changed_source_files':len(changes),'elf':build['sha256']}))
if __name__=='__main__':main()

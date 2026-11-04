#!/usr/bin/env python3
"""Exercise general replacement invariants against all selected real products."""
import argparse,copy,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import assemble_timecard_scene as a
def run(assembly,application):
 app=(application/'timecard.elf').read_bytes();m=json.loads((application/'timecard.json').read_text());cases=[]
 def reject(label,files,sd,manifest=m,elf=app):
  try:a.replace_scene_application(files,sd,elf,manifest)
  except (ValueError,KeyError):cases.append(label);return
  raise AssertionError('Accepted invalid '+label)
 for lane in a.LANES:
  def tree(p):return {f.relative_to(p).as_posix():f.read_bytes() for f in p.rglob('*') if f.is_file()}
  before=tree(assembly/lane/'before');sd=tree(assembly/lane/'before-sd')
  after,new_sd,receipt=a.replace_scene_application(before,sd,app,m)
  assert after==tree(assembly/lane/'after') and new_sd==tree(assembly/lane/'after-sd')
  boot=json.loads(before['boot.json']);old=[r for r in boot['app_capabilities'] if r['manifest']=='timecard.json'][0]
  row=receipt['app_row'];assert [g for g in row['grants'] if g['capability'].startswith('storage.')]==[g for g in old['grants'] if g['capability'].startswith('storage.')]
  assert after['cohort.json']==before['cohort.json']
  for mutate,label in [
   (lambda b:b['app_capabilities'].append(copy.deepcopy(old)),'duplicate app'),
   (lambda b:next(r for r in b['app_capabilities'] if r['manifest']=='timecard.json')['grants'].append({'capability':'unreviewed.secret','api':1,'instance_id':0}),'unknown authority'),
   (lambda b:next(r for r in b['app_capabilities'] if r['manifest']=='timecard.json')['grants'].__setitem__(slice(None),[g for g in old['grants'] if g['capability']!='storage.app-data']),'missing data namespace'),
   (lambda b:b['drivers'].append(copy.deepcopy(next(r for r in b['drivers'] if 'ui-scene' in r['manifest']))),'duplicate scene provider'),
   (lambda b:b.__setitem__('resident_shell',{'api':1,'host':b['default_app'],'foreground':['settings.elf'],'legacy':['timecard.elf']}),'wrong foreground policy')]:
   changed=copy.deepcopy(boot);mutate(changed);files=dict(before);files['boot.json']=a.encode(changed);reject(lane+'/'+label,files,sd)
  files=dict(before);files['default.elf']=before['default.elf']+b'corrupt';reject(lane+'/wrong host',files,sd)
  bad=copy.deepcopy(m);bad['requires'].append({'capability':'display.output','api':1});reject(lane+'/physical requirement',before,sd,bad)
  altered=bytearray(app);altered[0]=0;reject(lane+'/invalid ELF',before,sd,elf=bytes(altered))
  if lane!='watch':
   missing=dict(sd);del missing[old['image_source']['path']];reject(lane+'/missing selected image',before,missing)
  cases.append(lane+'/positive preservation')
 for path in ('../bad','/bad','a/../b','a\\b','.'):
  try:a.safe(path)
  except ValueError:cases.append('unsafe path '+path);continue
  raise AssertionError(path)
 print(json.dumps({'tests':len(cases),'cases':cases,'result':'passed'},indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--assembly',type=Path,required=True);p.add_argument('--application',type=Path,required=True);v=p.parse_args();run(v.assembly,v.application)

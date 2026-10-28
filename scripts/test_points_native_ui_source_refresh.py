#!/usr/bin/env python3
"""Preserve complete selector-off Points RTC/UTC direct/snapshot translations."""
import argparse,hashlib,json,os,re,subprocess
from pathlib import Path
from build_points_native_ui import ROOT,PROFILE,MANIFEST,DEFINES,stage_headers,validate
BASELINE='652fe571bba7f0cd4750af82f4e8b255a1934756'
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('system-apps','runtime','utilities','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 profile=json.loads(PROFILE.read_text());refresh=profile['source_refresh'];assert refresh['baseline_commit']==BASELINE
 roots={'productivity':ROOT,'system':a.system_apps.resolve(),'runtime':a.runtime.resolve(),'utilities':a.utilities.resolve()}
 validate(profile,json.loads(MANIFEST.read_text()),roots)
 includes=stage_headers(out,roots);prior=refresh['previous_sha256'];assert set(prior)=={'Apps/points_in_time.c','Apps/points_native_time.h','Apps/points_native_ui.inc'}
 staged=out/'prior'
 for name,digest in profile['application_source_sha256'].items():
  current=(ROOT/name).read_bytes();assert hashlib.sha256(current).hexdigest()==digest,name
  data=subprocess.check_output(['git','-C',str(ROOT),'show',BASELINE+':'+name]) if name in prior else current
  if name in prior:assert hashlib.sha256(data).hexdigest()==prior[name],name
  target=staged/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
 rows=[]
 for utc in (False,True):
  for snapshot in (False,True):
   defines=[x for x in DEFINES if x!='ALARM_NATIVE_UTC']+(['ALARM_NATIVE_UTC'] if utc else ['PORTABLE_RTC_WALL_TIME'])+(['PORTABLE_RASTER_SNAPSHOT'] if snapshot else [])
   assert 'POINTS_SERVICE_CONTEXT' not in defines and 'PORTABLE_NATIVE_SCENE' not in defines
   tokens=[]
   for root in (staged,ROOT):
    command=[os.environ.get('CC','cc'),'-E','-P',*['-D'+d for d in defines],*['-I'+str(d) for d in [*includes,root/'lib/NativeApps/include']],str(root/'Apps/points_in_time.c')]
    source=subprocess.check_output(command,text=True);tokens.append('\n'.join(re.findall(r'''"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_0-9]+|[^\s]''',source)))
    dependencies=subprocess.check_output([*command[:1],'-MM',*command[3:]],text=True)
    assert 'native_scroll_labels.h' not in dependencies and 'PointsServiceContext.h' not in dependencies
   assert tokens[0]==tokens[1],('Frozen Points translation changed',utc,snapshot)
   rows.append({'utc':utc,'snapshot':snapshot,'defines':defines,'tokens_sha256':hashlib.sha256(tokens[0].encode()).hexdigest()})
 (out/'receipt.json').write_text(json.dumps({'passed':True,'baseline':BASELINE,'compositions':rows,'scope':'Host preprocessor token parity and absent unselected dependency proof; not target byte/runtime/hardware acceptance.'},indent=2)+'\n');print('PASS four complete frozen Points translations and absent current-only dependencies')
if __name__=='__main__':main()

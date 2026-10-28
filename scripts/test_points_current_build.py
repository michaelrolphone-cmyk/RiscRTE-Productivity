#!/usr/bin/env python3
"""Current Points contract regressions; optionally inspect an actual target receipt."""
import argparse,copy,hashlib,importlib.util,json,subprocess,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('points_build',ROOT/'scripts/build_points_current.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
RECORD=None
class Contract(unittest.TestCase):
 def test_current_selection(self):
  d=set(m.DEFINES)
  self.assertTrue({'POINTS_SERVICE_CONTEXT','ALARM_SERVICE_TAGGED_V2','ALARM_SERVICE_CLOCK_EDIT_V2','ALARM_NATIVE_UTC','PORTABLE_POINTS_NATIVE_UI','PORTABLE_NATIVE_SCENE','PORTABLE_RF_NATIVE_CANVAS','PORTABLE_NATIVE_CANVAS','PORTABLE_QUICK_PENDING_FRAME','PORTABLE_QUICK_ATOMIC_PREFERENCES','PORTABLE_PREFERENCE_STORE','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_SAFE_VIEWPORT'}<=d)
  self.assertFalse(any(x.startswith(('PORTABLE_RASTER_SNAPSHOT','POINTS_CATALOG_SERVICE','POINTS_IN_TIME_SERVICE')) for x in d))
  self.assertIn('POINTS_RETURN_APP="springboard.elf"',d);self.assertIn('PORTABLE_HOME_APP="default.elf"',d)
  self.assertNotIn('PortableNativeTimeSource.c',m.HELPERS) # app-owned reader, not duplicate global owner
  self.assertEqual(len(m.HELPERS),len(set(m.HELPERS)))
 def test_manifest(self):
  r=m.contract();self.assertEqual(r['optional'],[{'capability':'board.battery','api':1}])
  self.assertNotIn(('runtime.realtime.control',1),m.REQUIRES)
  self.assertNotIn(('storage.bound-key-value',2),m.REQUIRES)
  self.assertIn(('storage.preferences',1),m.REQUIRES);self.assertIn(('alarm.service',2),m.REQUIRES)
 def test_manifest_mutations(self):
  good=m.contract();mutations=[]
  for key,value in [('id','other'),('file_name','other.elf'),('entry','main'),('display_name','other'),('icon','other'),('type','driver'),('architecture','arm'),('unexpected_metadata',True)]:
   bad=copy.deepcopy(good);bad[key]=value;mutations.append((key,bad))
  for cap in ['runtime.realtime.control','storage.bound-key-value','storage.app-data']:
   bad=copy.deepcopy(good);bad['requires'].append({'capability':cap,'api':1});mutations.append((cap,bad))
  for label,rows in [('missing',good['requires'][:-1]),('duplicate',good['requires']+[good['requires'][0]])]:
   bad=copy.deepcopy(good);bad['requires']=rows;mutations.append((label,bad))
  bad=copy.deepcopy(good);bad['optional']=[];mutations.append(('battery',bad))
  bad=copy.deepcopy(good);bad['requires'][0]['api']=1;mutations.append(('v1 service',bad))
  with tempfile.TemporaryDirectory(prefix='points-contract-') as td:
   p=Path(td)/'manifest.json'
   for label,bad in mutations:
    with self.subTest(mutation=label),patch.object(m,'MANIFEST',p):
     p.write_text(json.dumps(bad))
     with self.assertRaises(ValueError):m.contract()
 def test_full_revision_required(self):
  self.assertEqual(m.revision('a'*40),'a'*40)
  for bad in ['main','a'*39,'a'*41,'A'*40,'z'*40]:
   with self.subTest(value=bad),self.assertRaises(argparse.ArgumentTypeError):m.revision(bad)
 def test_wrong_source_rejected_before_staging(self):
  with tempfile.TemporaryDirectory(prefix='points-source-pin-') as td:
   root=Path(td);a=argparse.Namespace(system_apps=root/'system',runtime=root/'runtime',utilities=root/'utilities',output_dir=root/'output',system_revision='a'*40,utilities_revision='b'*40)
   for heads in [('c'*40,),('a'*40,'c'*40)]:
    with self.subTest(heads=heads),patch.object(m,'git',side_effect=heads),self.assertRaisesRegex(ValueError,'Exact explicitly selected source'):m.build(a,argparse.ArgumentParser())
    self.assertFalse(a.output_dir.exists())
 def test_frozen_sources_unchanged(self):
  paths=['scripts/build_points_in_time.py','scripts/build_points_native_ui.py','scripts/build_points_native_utc.py','Apps/native/points_native_ui.json','Apps/native/points_utc.json']
  for name in paths:
   with self.subTest(path=name):self.assertEqual((ROOT/name).read_bytes(),subprocess.check_output(['git','-C',ROOT,'show','HEAD:'+name]))
 def test_actual_target_receipt(self):
  if RECORD is None:self.skipTest('--build-record not supplied; no target receipt claim')
  r=json.loads(RECORD.read_text());self.assertFalse(r['clock_edit_authority']);self.assertFalse(r['service_occurrence_access']);self.assertTrue(r['service_projection_context'])
  self.assertEqual(r['grant_bindings'],{'storage.key-value':[1,5],'storage.preferences':[1]})
  self.assertEqual(r['points_config_namespace'],5);self.assertFalse(r['hardware_verified']);self.assertFalse(r['product_recipe'])
  self.assertEqual(r['manifest'],m.contract());self.assertEqual(set(r['exports']),m.EXPORTS);self.assertLessEqual(set(r['imports']),m.IMPORTS)
  system=Path(next(x for x in r['compile_inputs']['sources'] if x.endswith('/lib/PortableApps/src/adapter.c'))).parents[3]
  sources=list(map(Path,r['compile_inputs']['sources']))
  self.assertEqual(sum(x==ROOT/'Apps/points_in_time.c' for x in sources),1)
  for helper in m.HELPERS:self.assertEqual(sum(x==system/'lib/PortableApps/src'/helper for x in sources),1)
  self.assertFalse(any(x.name=='PortableNativeTimeSource.c' for x in sources))
  deps=r['consumed_source_sha256'];self.assertTrue(any(x.endswith('/PointsServiceContext.h') for x in deps));self.assertTrue(any(x.endswith('/PortablePointsNative.h') for x in deps))
  for path,digest in deps.items():self.assertEqual(hashlib.sha256(Path(path).read_bytes()).hexdigest(),digest,path)
  sdk=RECORD.parent/'foreground-idle-sdk/include'
  utilities=Path(r['compile_inputs']['include_directories'][1])
  for name in ('PointsServiceContext.h','AlarmServiceExtensionV2.h','PointsRecords.h'):
   self.assertEqual((sdk/name).read_bytes(),(utilities/name).read_bytes())
  for name in ('PortableNativeScene.h','PortablePointsNative.h'):
   self.assertEqual((sdk/name).read_bytes(),(system/'lib/PortableApps/include'/name).read_bytes())
  self.assertFalse(json.loads((RECORD.parent/'tagged-alarm-sdk.json').read_text())['clock_edit_authority'])
  self.assertTrue(r['loader_validation']);self.assertFalse(r['foreground_idle']['clock_edit_authority'])
  self.assertEqual(hashlib.sha256((RECORD.parent/'points_in_time.elf').read_bytes()).hexdigest(),r['elf_sha256'])
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build-record',type=Path);a,rest=p.parse_known_args();RECORD=a.build_record
 unittest.main(argv=[__file__,*rest])

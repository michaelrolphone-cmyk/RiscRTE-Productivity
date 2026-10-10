import hashlib,json,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class NativePointsProfile(unittest.TestCase):
 def test_default_catalog_bytes_preserved(self):
  expected=json.loads((ROOT/'tests/preserved-profile-baseline.json').read_text())['sha256']
  for name in ('Apps/points_in_time.json','productivity-manifest.json'):
   self.assertEqual(hashlib.sha256((ROOT/name).read_bytes()).hexdigest(),expected[name],name)
 def test_distinct_opt_in_native_authority(self):
  m=json.loads((ROOT/'Apps/native/points_utc.json').read_text())
  self.assertEqual(m['version'],'0.6.2')
  self.assertEqual([(x['capability'],x['api']) for x in m['requires']],[('display.output',1),('input.touch.raw',1),('input.navigation',1),('runtime.realtime',1),('storage.key-value',1),('alarm.service',2)])
  text=(ROOT/'scripts/build_points_native_utc.py').read_text()
  for flag in ('ALARM_NATIVE_UTC','ALARM_SERVICE_TAGGED_V2','PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_NATIVE_CUSTODY_FENCE'):
   self.assertIn('-D'+flag,text)
 def test_frozen_contract(self):
  p=json.loads((ROOT/'sdk/points-native-utc-sources.json').read_text())
  self.assertEqual(p['timezone_entries'],419);self.assertEqual(p['service_bound_key_count'],9)
  self.assertFalse(p['app_meta_service_bound']);self.assertFalse(p['hardware_verified'])
  self.assertEqual(p['app_storage'],[{'instance':5,'access':'read-write','keys':['points_utc_cfg','points_utc_meta']},{'instance':1,'access':'read-only','keys':['time_zone','time_format']}])
  self.assertEqual(p['runtime_sha'],'30dcec5ce6ce33223f2b203a2399283e1f758567')
  self.assertEqual(p['system_sha'],p['adapter_sha'])
  self.assertFalse(p['system_public'])
  for field in ('adapter_sha','utilities_sha'):
   self.assertRegex(p[field] or '',r'^[0-9a-f]{40}$')
if __name__=='__main__':unittest.main()

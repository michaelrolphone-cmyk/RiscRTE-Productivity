import argparse,contextlib,io,unittest
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import native_paper_transitions as profile
class IdleProfile(unittest.TestCase):
 def args(self,values):
  p=argparse.ArgumentParser();profile.options(p);return p,p.parse_args(values)
 def test_partial_helper_rejected(self):
  for option in ('source','sdk','runtime-sdk'):
   p,a=self.args(['--paper-transitions','--motion-system','/unused','--x4-idle-'+option,'/unused'])
   with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):profile.select(a,p,Path('/unused'),[])
 def test_idle_requires_paper_motion(self):
  p,a=self.args(sum((['--x4-idle-'+n,'/unused'] for n in ('source','sdk','runtime-sdk')),[]))
  with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):profile.select(a,p,Path('/unused'),[])
 def test_default_preserves_all_inputs(self):
  p,a=self.args([]);m={'requires':[]};inc=[Path('/unused')]
  self.assertEqual(profile.configure_idle(a,p,Path('/unused'),Path('/unused'),inc,['-DBASE'],m),(inc,['-DBASE'],[],None));self.assertEqual(m,{'requires':[]})
 def test_versions_separate_from_watch_and_prior_profiles(self):
  self.assertEqual(profile.IDLE_PIN['versions'],{'points_in_time':'0.6.7','timecard':'0.2.9'})
  self.assertNotEqual(profile.IDLE_PIN['versions'],profile.SCROLL_PIN['versions'])
if __name__=='__main__':unittest.main()

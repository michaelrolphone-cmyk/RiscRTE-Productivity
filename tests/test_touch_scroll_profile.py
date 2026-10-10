"""Fail closed when the selected profile or its frozen adapter is incomplete."""
import argparse
import contextlib
import io
import unittest
from pathlib import Path
from unittest.mock import patch
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import native_paper_transitions as profile
class TouchScrollProfile(unittest.TestCase):
    def parser(self):
        parser=argparse.ArgumentParser();profile.options(parser);return parser
    def test_requires_explicit_paper_motion(self):
        parser=self.parser();args=parser.parse_args(['--touch-scrolling'])
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
            profile.select(args,parser,Path('/unused'),[],{})
    def test_wrong_adapter_rejected(self):
        parser=self.parser();args=parser.parse_args(['--touch-scrolling','--paper-transitions','--motion-system','/unused'])
        with patch.object(profile.subprocess,'check_output',return_value='wrong\n'),self.assertRaises(ValueError):
            profile.select(args,parser,Path('/unused'),[],{})
    def test_default_is_unchanged(self):
        parser=self.parser();args=parser.parse_args([]);flags=['-DNATIVE'];manifest={'version':'0.6.2'}
        adapter,actual,got,receipt=profile.select(args,parser,Path('/unused'),flags,manifest)
        self.assertEqual((adapter,actual,got,receipt),(Path('/unused'),flags,manifest,None))
    def test_reserved_versions(self):
        self.assertEqual(profile.SCROLL_PIN['versions'],{'points_in_time':'0.6.6','timecard':'0.2.6'})
        self.assertEqual(profile.BROADCAST_PIN['versions'],{'points_in_time':'0.6.4','timecard':'0.2.4'})
if __name__=='__main__':unittest.main()

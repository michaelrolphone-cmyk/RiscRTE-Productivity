#!/usr/bin/env python3
"""Selector-off C token parity; does not relax frozen SDK/build validators."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
S=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser()
 for n in ['baseline','system-apps','sdk','output']:p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();profiles={'legacy':[],'native-time':['TIMECARD_NATIVE_TIME','TIMECARD_APP_DATA','TIMECARD_PAPER','PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_NOVA_UI','ALARM_SERVICE_TAGGED_V2'],'native-ui':['TIMECARD_NATIVE_TIME','TIMECARD_APP_DATA','TIMECARD_PAPER','PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_NOVA_UI','ALARM_SERVICE_TAGGED_V2','PORTABLE_TIMECARD_NATIVE_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_APP_OWNS_TOUCH_CHROME']}
 # Preserve complete string/character literal tokens; ignore only whitespace.
 token=re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_][A-Za-z_0-9]*|[0-9]+(?:\.[0-9]*)?(?:[A-Za-z_0-9]+)?|[^\s]')
 rows={}
 for name,defines in profiles.items():
  bodies=[]
  for root in [a.baseline,S]:
   cmd=['cc','-E','-P',*['-D'+f for f in defines],'-I'+str(a.sdk),'-I'+str(a.system_apps/'lib/NativeApps/include'),'-I'+str(root/'lib/NativeApps/include'),'-I'+str(a.system_apps/'Apps'),str(root/'Apps/timecard_portable.c')]
   bodies.append(token.findall(subprocess.check_output(cmd,text=True)))
  assert bodies[0]==bodies[1],name
  rows[name]={'tokens':len(bodies[1]),'sha256':hashlib.sha256(json.dumps(bodies[1]).encode()).hexdigest()}
 receipt={'passed':True,'scope':'Same SDK headers, current selector off: exact preprocessed C tokens, whitespace ignored but string/character literals preserved. Not a rebuild of frozen binary recipes.','profiles':rows}
 a.output.write_text(json.dumps(receipt,indent=2)+'\n');print('PASS historical selector-off token parity: '+', '.join(rows))
if __name__=='__main__':main()

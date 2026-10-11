#!/usr/bin/env python3
"""Selected resident Points catalog and native civil Timecard; no default changes."""
import argparse
import json
from pathlib import Path
import sys
from build_timecard_native_time import DEFINES as TIMECARD
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--utilities',type=Path,required=True)
    # Parse only this path before loading the explicit selected build helper.
    preliminary,_=p.parse_known_args()
    sys.path.insert(0,str(preliminary.utilities.resolve()/'scripts'))
    import resident_client_build as resident
    resident.options(p);p.add_argument('--app',choices=['points_in_time','timecard'],action='append');a=p.parse_args()
    c=resident.prepare(a,p,a.utilities.resolve());records={}
    base=['-DPORTABLE_STAGE_LOGS','-DPORTABLE_NOVA_UI','-DPORTABLE_ALARM_CLIENT','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_HOME_APP="default.elf"','-DPORTABLE_APP_LAUNCH_GUARD','-DPORTABLE_BLE_BROADCAST','-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF','-DPORTABLE_PAPER_PREFERENCES','-DPORTABLE_UNPADDED_HOURS']
    shared=[('display.output',1,3),('input.touch.raw',1,4),('input.navigation',1,6),('runtime.realtime',1,0),('storage.key-value',1,1),('alarm.service',2,0),('board.battery',1,7),('telemetry.broadcast',1,0)]
    for name in a.app or ['points_in_time','timecard']:
        if name=='points_in_time':
            source='Apps/points_catalog_app.c';version='0.6.11';defines=base+['-DALARM_NATIVE_UTC','-DPOINTS_RETURN_APP="springboard.elf"']
            grants=shared+[('storage.key-value',1,5),('storage.app-data',1,5)]
        else:
            source='Apps/timecard_portable.c';version='0.2.10'
            defines=base+[x for x in TIMECARD if x!='-DPORTABLE_QUICK_ACTIONS']+['-DPORTABLE_TOUCH_SCROLL','-DPORTABLE_APP_TOUCH_SCROLL','-DPORTABLE_PRODUCTIVITY_SCROLL']
            grants=shared+[('storage.app-data',1,1)]
        records[name]=resident.build(c,ROOT,name,version,defines,[ROOT/source],[dict(capability=k,api=v,instance_id=i) for k,v,i in grants],dict(native_time=True,telemetry_default='off',idle_policy='resident-host',app_source=source,scrolling='completed-frame paper lists',storage='points.catalog namespace5' if name=='points_in_time' else 'civil timecard.json namespace1'))
    resident.write(c['out']/'cohort-receipt.json',dict(schema=1,role='foreground',quick_render_total=0,built=records,hardware_verified=False,installable=False))
if __name__=='__main__':main()

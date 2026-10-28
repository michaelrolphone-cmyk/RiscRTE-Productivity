#!/usr/bin/env python3
"""Small current-profile recipe/SDK staging checks; no target compilation."""
import argparse,copy,importlib.util,json,tempfile
from pathlib import Path
S=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('current',S/'scripts/build_timecard_current.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def main():
 p=argparse.ArgumentParser()
 for n in ['system-apps','runtime','utilities','drivers']:p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();profile,manifest=m.contract()
 original=copy.deepcopy(manifest);projected=m.runtime_manifest(manifest)
 assert manifest==original and 'category' in manifest and 'category' not in projected
 assert projected=={k:v for k,v in manifest.items() if k!='category'}
 assert projected['requires']==manifest['requires'] and projected['optional']==manifest['optional']
 # Check the projection against the actual pinned Runtime schema, not a
 # permissive fixture or a copied expected subset alone.
 runtime_source=(a.runtime/'src/bootstrap/Runtime.cpp').read_text()
 import re
 schema=re.search(r'keys\(manifest,\{([^}]+)\},\{([^}]+)\}\)',runtime_source)
 assert schema is not None
 actual_keys=set(re.findall(r'"([a-z_]+)"',schema.group(1)+schema.group(2)))
 assert m.RUNTIME_MANIFEST_KEYS==actual_keys and set(projected)<=actual_keys
 try:m.runtime_manifest(dict(manifest,unexpected_metadata=True))
 except ValueError:pass
 else:raise AssertionError('Unknown manifest field silently dropped')
 assert manifest['optional']==[{'capability':'board.battery','api':1}]
 assert not any(x['capability']=='board.battery' for x in manifest['requires'])
 assert 'TIMECARD_CURRENT_PROFILE' in m.DEFINES and 'PORTABLE_NATIVE_SCENE' in m.DEFINES
 assert not any(x.startswith(('PORTABLE_RASTER_SNAPSHOT','PORTABLE_RETURN_APP=')) for x in m.DEFINES)
 assert 'TIMECARD_RETURN_APP="springboard.elf"' in m.DEFINES and 'PORTABLE_HOME_APP="default.elf"' in m.DEFINES
 dynamic='\n'.join(['         U memcpy']+['00000100 T '+n for n in sorted(m.EXPORTS)])
 symbols='\n'.join('1: 00000010 20 FUNC LOCAL DEFAULT 3 '+n for n in m.PRIVATE)
 assert m.check_symbols(dynamic,symbols)[0]=={'memcpy'}
 for bad in [(dynamic+'\n         U unexpected',symbols),(dynamic,symbols.replace('LOCAL','GLOBAL')),(dynamic,symbols.replace(' 3 ',' UND '))]:
  try:m.check_symbols(*bad)
  except ValueError:pass
  else:raise AssertionError('Invalid ABI admitted')
 with tempfile.TemporaryDirectory(prefix='timecard-current-recipe-') as td:
  args=argparse.Namespace(system_apps=a.system_apps,runtime=a.runtime,utilities=a.utilities,foreground_idle_helper_dir=a.drivers/'components/portable-light-sleep',foreground_idle_drivers_sdk_dir=a.drivers/'sdk/driver',foreground_idle_fallback_seconds=15,output_dir=Path(td)/'recipe',compiler=None)
  out,system,toolchain,selected,sources,flags,record=m.prepare(args,p)
  assert selected==projected and record['manifest']==selected
  assert record['runtime_sdk_identity']==record['foreground_idle']['runtime_sdk_identity']
  assert record['runtime_sdk_identity']['checkout_clean'] is True
  assert record['runtime_sdk_identity']['canonical_commit']==m.RUNTIME
  assert record['runtime_sdk_identity']['checkout_commit']==m.git(a.runtime,'rev-parse','HEAD')
  assert record['source_manifest']==original and record['catalog_metadata']=={k:original[k] for k in ('display_name','icon','category')}
  assert record['grant_bindings']=={'storage.key-value':[1],'storage.app-data':[1],'storage.preferences':[1]}
  assert not record['clock_edit_authority'] and record['foreground_idle']['alarm_sleep']['api']==2
  assert all(x['capability']!='runtime.realtime.control' for x in selected['requires'])
  sdk=out/'foreground-idle-sdk/include'
  assert (sdk/'RiscPreferenceStoreV1.h').read_bytes()==(a.runtime/'sdk/app/RiscPreferenceStoreV1.h').read_bytes()
  assert 'RISC_APP_DATA_BUSY' in (sdk/'RiscAppDataV1.h').read_text()
  assert (sdk/'PortableNativeScene.h').read_bytes()==(system/'lib/PortableApps/include/PortableNativeScene.h').read_bytes()
  assert '-DPORTABLE_FOREGROUND_ALARM_SLEEP' in flags and '-DALARM_SERVICE_CLOCK_EDIT_V2' in flags
  assert len([x for x in sources if x.name=='timecard_portable.c'])==1
  assert all(sum(x.name==n for x in sources)==1 for n in ('quick_actions.c','quick_render.c','quick_session.c'))
  assert '-DPORTABLE_QUICK_NATIVE_GEOMETRY' in flags
  assert '-DPORTABLE_QUICK_MIN_CONTENT=160' in flags
  assert '-DPORTABLE_QUICK_PENDING_FRAME' in flags and profile['quick_preference_store']['completed_pending_frame_before_submit'] is True
  assert record['compile_inputs']['include_directories'][1]==str(system/'lib/NativeApps/include')
  assert record['compile_inputs']['sources']==[str(x) for x in sources]
  assert record['compile_inputs']['source_sha256']==record['source_sha256']
  assert record['compile_inputs']['definitions']==record['build_defines']
  assert '-DPORTABLE_PREFERENCE_STORE' in flags and '-DPORTABLE_QUICK_ATOMIC_PREFERENCES' in flags
  assert '-DPORTABLE_QUICK_SAFE_CONTENT' in flags and '-DPORTABLE_QUICK_CAPABILITY_SAFE' in flags
  assert '-DPORTABLE_QUICK_ACTIONS' in flags and '-DPORTABLE_WATCH_NATIVE_HEIGHT' in flags
  assert not record['read_only_preferences'] and record['timecard_time_format_read_only']
  assert json.loads((out/'tagged-alarm-sdk.json').read_text())['clock_edit_authority'] is False
  print('PASS current Timecard contract, canonical SDK, Quick shared preferences/private AppData namespace1, routes and ABI-negative gates; no compile')
if __name__=='__main__':main()

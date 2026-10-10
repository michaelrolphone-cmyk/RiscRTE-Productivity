#!/usr/bin/env python3
"""Explicit native UTC Points development artifact; default builds are unchanged."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
import native_paper_transitions as motion
from build_points_in_time import IMPORTS,EXPORTS
ROOT=Path(__file__).resolve().parents[1]
PIN=json.loads((ROOT/'sdk/points-native-utc-sources.json').read_text())
HELPERS=('PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')
DEFINES=['-DPORTABLE_STAGE_LOGS','-DALARM_NATIVE_UTC','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_NOVA_UI','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_DISPLAY_ROTATION=90','-DPOINTS_RETURN_APP="springboard.elf"','-DPORTABLE_HOME_APP="default.elf"','-DPORTABLE_QUICK_ACTIONS']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(p,*args):return subprocess.check_output(['git','-C',str(p),*args],text=True).strip()
def run(args):subprocess.run(list(map(str,args)),check=True,timeout=120)
def exact(path,pin):
 if not pin or git(path,'rev-parse','HEAD')!=pin or git(path,'status','--porcelain','--untracked-files=all'):raise ValueError('Clean exact dependency required: '+str(pin))
def verify(system,adapter,utilities,runtime):
 exact(system,PIN['system_sha']);exact(adapter,PIN['adapter_sha']);exact(utilities,PIN['utilities_sha']);exact(runtime,PIN['runtime_sha'])
 for base,key in [(system,'system_sources'),(runtime,'runtime_sources'),(utilities,'utilities_sources')]:
  for name,digest in PIN[key].items():
   if sha(base/name)!=digest:raise ValueError('Frozen input changed: '+name)
 if 'ALARM_SERVICE_TAGGED_V2' not in (adapter/'lib/PortableApps/include/PortableAlarmClient.h').read_text():raise ValueError('Shared adapter has no explicit API 2 alarm client')
 if not (adapter/'lib/PortableApps/include/PortableNativeTimeToolbar.h').exists():raise ValueError('Shared native toolbar contract missing')
def stage_headers(out,system,adapter,utilities,runtime):
 # Symlink external headers. Never copy unpublished System implementation into
 # Productivity. Canonical SDK and tagged service headers win quoted includes.
 stage=out/'sdk';inc=stage/'include';inc.mkdir(parents=True,exist_ok=True)
 headers={p.name:p for p in (adapter/'lib/PortableApps/include').glob('*.h')}
 for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h'):headers[name]=runtime/'sdk/app'/name
 for name in ('AlarmServiceV1.h','AlarmServiceV2.h'):headers[name]=utilities/'lib/Alarm/include'/name
 for name,path in headers.items():
  target=inc/name
  if target.is_symlink():target.unlink()
  elif target.exists():raise ValueError('Refuse replacing non-symlink SDK input: '+str(target))
  target.symlink_to(path)
 target=stage/'time'
 if target.is_symlink():target.unlink()
 target.mkdir(parents=True,exist_ok=True)
 for source in (system/'lib/PortableApps/time').rglob('*'):
  if not source.is_file():continue
  link=target/source.relative_to(system/'lib/PortableApps/time');link.parent.mkdir(parents=True,exist_ok=True)
  if link.is_symlink():link.unlink()
  elif link.exists():raise ValueError('Refuse replacing non-symlink dependency: '+str(link))
  link.symlink_to(source)
 return [inc,utilities/'lib/Alarm/include',runtime/'sdk/app',adapter/'lib/NativeApps/include',adapter/'Apps']
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('system-apps','adapter-system','utilities','runtime'):p.add_argument('--'+name,type=Path,required=True)
 motion.options(p)
 p.add_argument('--output-dir',type=Path,default=None);a=p.parse_args()
 system,adapter,utilities,runtime=(x.resolve() for x in (a.system_apps,a.adapter_system,a.utilities,a.runtime));verify(system,adapter,utilities,runtime)
 manifest=json.loads((ROOT/'Apps/native/points_utc.json').read_text());assert manifest['version']==PIN['version']
 baseline_adapter=adapter
 adapter,defines,manifest,motion_receipt=motion.select(a,p,adapter,DEFINES,manifest)
 version=manifest['version']
 out=(a.output_dir or ROOT/'dist'/('points-native-utc-paper-transitions' if a.paper_transitions else 'points-native-utc')).resolve();out.mkdir(parents=True,exist_ok=True);includes=stage_headers(out,system,adapter,utilities,runtime)
 assert [(x['capability'],x['api']) for x in manifest['requires']]==[('display.output',1),('input.touch.raw',1),('input.navigation',1),('runtime.realtime',1),('storage.key-value',1),('alarm.service',2)]+([('telemetry.broadcast',1)] if a.ble_broadcast else [])
 includes,defines,idle_sources,idle_receipt=motion.configure_idle(a,p,adapter,out,includes,defines,manifest)
 cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
 if not cc:raise ValueError('Set NATIVE_APP_CC to the existing pinned GCC8.4 compiler')
 compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
 if '8.4.0' not in compiler or '2021r2-patch5' not in compiler:raise ValueError('Pinned GCC8.4 compiler required')
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 mapping=out/'exports.map';mapping.write_text('{ global: '+ '; '.join(sorted(EXPORTS))+'; local: *; };\n')
 sources=[ROOT/'Apps/points_in_time.c',adapter/'lib/PortableApps/src/adapter.c',catalog,*[system/'lib/PortableApps/src'/x for x in HELPERS],*[adapter/'lib/PortableApps/src'/x for x in ('quick_actions.c','quick_render.c','quick_session.c')]]
 sources+=idle_sources
 elf=out/manifest['file_name']
 compile_command=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*defines,*['-I'+str(x) for x in includes],*sources,'-lgcc','-o',elf]
 run(compile_command)
 symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
 imports={x.split()[-1] for x in symbols.splitlines() if ' U ' in ' '+x};exports={x.split()[-1] for x in symbols.splitlines() if len(x.split())>=3 and x.split()[-2] in ('T','D','B','R')}
 if not imports<=IMPORTS or exports!=EXPORTS:raise ValueError(('Unexpected import/export',imports-IMPORTS,exports))
 data=elf.read_bytes()
 if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Expected Xtensa ELF32 ET_DYN')
 validator=out/'validate-elf';run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator]);run([validator,elf])
 elf.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
 sdk_headers={name:sha(includes[0]/name) for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','AlarmServiceV1.h','AlarmServiceV2.h')}
 receipt={'schema':1,'app':'points_in_time','version':version,'source_repo':'michaelrolphone-cmyk/RiscRTE-Productivity','source_revision':git(ROOT,'rev-parse','HEAD'),'system_source_revision':git(adapter,'rev-parse','HEAD'),'runtime_source_revision':PIN['runtime_sha'],'alarm_source_revision':PIN['utilities_sha'],'alarm_api':2,'time_policy':'native-realtime-iana','elf_sha256':sha(elf),'elf_bytes':len(data),'requires':manifest['requires'],'sdk_sha256':sdk_headers}
 if idle_receipt:
  receipt.update(idle_policy=idle_receipt,build_defines=defines)
  receipt['sdk_sha256'].update(idle_receipt['sdk_sha256'])
 if motion_receipt:receipt['paper_transitions']=motion_receipt
 if a.touch_scrolling:receipt.update(touch_scrolling=motion_receipt['touch_scrolling'],build_defines=defines)
 if a.ble_broadcast:receipt.update(ble_broadcast=motion_receipt['ble_broadcast'],build_defines=defines)
 (out/'x4-native-app.json').write_text(json.dumps(receipt,indent=2)+'\n')
 source_names=['Apps/points_in_time.c','Apps/points_native_time.h','Apps/points_writer.h','Apps/points_nova7.inc','Apps/points_nova7_picker.inc','Apps/points_watch_keyboard.h','Apps/points_paper.inc','Apps/native/points_utc.json','scripts/build_points_native_utc.py','sdk/points-native-utc-sources.json']
 adapter_sources={str(x.relative_to(adapter)):sha(x) for folder in ('lib/PortableApps','lib/NativeApps/include','Apps') for x in (adapter/folder).rglob('*') if x.is_file()}
 evidence={'schema':1,'purpose':'local-native-utc-points-development-not-install-catalog','version':version,'source_sha':git(ROOT,'rev-parse','HEAD'),'working_tree_dirty':bool(git(ROOT,'status','--porcelain')),'pins':{k:PIN[k] for k in ('system_sha','adapter_sha','adapter_public','utilities_sha','runtime_sha')},'defines':defines,'compiler':compiler,'compile_command':list(map(str,compile_command)),'sha256':sha(elf),'size_bytes':len(data),'imports':sorted(imports),'exports':sorted(exports),'source_sha256':{x:sha(ROOT/x) for x in source_names},'adapter_sha256':adapter_sources,'hardware_verified':False}
 if a.touch_scrolling:
  evidence['touch_scrolling']=motion_receipt['touch_scrolling']
  evidence['source_sha256'].update({str(x.relative_to(ROOT)):sha(x) for x in [ROOT/'sdk/native-touch-scroll-sources.json',*ROOT.glob('Apps/*scroll*')] if x.is_file()})
 if idle_receipt:
  evidence['idle_policy']=idle_receipt
  evidence['source_sha256']['sdk/native-idle-sources.json']=sha(ROOT/'sdk/native-idle-sources.json')
 evidence['test_source_sha256']={str(x.relative_to(ROOT)):sha(x) for base in ('test/native_apps','tests','scripts') for x in (ROOT/base).rglob('*') if x.is_file() and x.suffix in ('.py','.c','.h','.sh')}
 evidence['runtime_sha256']=PIN['runtime_sources'];evidence['utilities_sha256']=PIN['utilities_sources'];evidence['system_sha256']=PIN['system_sources']
 evidence['source_sha256'].update({name:sha(ROOT/name) for name in ('scripts/native_paper_transitions.py','sdk/paper-transitions-sources.json','sdk/native-broadcast-sources.json')})
 if motion_receipt:
  evidence['paper_transitions']=motion_receipt
  evidence['dependency_paths']={'system':str(system),'adapter':str(baseline_adapter),'motion':str(adapter),'runtime':str(runtime),'utilities':str(utilities)}
 (out/'build-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
 for repo,name in ((ROOT,'Productivity'),(system,'System-Apps'),(utilities,'Utilities'),(runtime,'Runtime')):
  dest=out/'licenses'/name;dest.mkdir(parents=True,exist_ok=True);shutil.copyfile(repo/'LICENSE',dest/'LICENSE')
 for folder in ('fonts','paper_fonts','settings_fonts','quick_fonts','time'):
  for source in (system/'lib/PortableApps'/folder).rglob('*'):
   if source.is_file() and ('LICENSE' in source.name or 'OFL' in source.name or source.name.endswith('SOURCES.json') or source.name=='TIMEZONE_PROVENANCE.json'):
    dest=out/'licenses/System-Apps'/folder/source.relative_to(system/'lib/PortableApps'/folder);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,dest)
 print('Native Points '+version+': exact dependencies, native-only manifest, target ELF, imports/exports and licenses verified')
if __name__=='__main__':main()

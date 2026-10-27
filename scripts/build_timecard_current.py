#!/usr/bin/env python3
"""Explicit current Badge Timecard composition; frozen builders are unchanged.

Builds one app, never a firmware, grant binding, installation or publication.
The deployment must independently admit the exact private AppData instance.
"""
import argparse,hashlib,importlib,json,os,re,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PROFILE=ROOT/'sdk/timecard-current-profile.json'
MANIFEST=ROOT/'Apps/native/timecard_current.json'
RUNTIME='273bdbccf65647a803ab0586a418f8283732baec'
ALARM='bbafc2c3aebe8c3b9805639f873341e31296c8fb'
MODEL='c17b0fed28eb44d2397c740ef56c8f07bc51c3f707d3080ec9ad5849e25a31a9'
DEFINES=['TIMECARD_CURRENT_PROFILE','TIMECARD_NATIVE_TIME','TIMECARD_PAPER','TIMECARD_APP_DATA','TIMECARD_APP_DATA_INSTANCE=1u',
 'PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_PENDING_FRAME','PORTABLE_QUICK_NATIVE_GEOMETRY','PORTABLE_QUICK_SAFE_CONTENT','PORTABLE_QUICK_MIN_CONTENT=160','PORTABLE_QUICK_CAPABILITY_SAFE','PORTABLE_QUICK_ATOMIC_PREFERENCES','PORTABLE_PREFERENCE_STORE','PORTABLE_TIMECARD_NATIVE_UI','PORTABLE_NATIVE_SCENE','PORTABLE_NOVA_UI','PORTABLE_RF_NATIVE_CANVAS','PORTABLE_APP_OWNS_TOUCH_CHROME',
 'PORTABLE_WATCH_NATIVE_HEIGHT','PORTABLE_WATCH_COLOR_ONLY','PORTABLE_WATCH_SAFE_INSETS','PORTABLE_SAFE_VIEWPORT','PORTABLE_FORCE_FULL_FRAMES',
 'PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_INPUT_NAVIGATION','PORTABLE_ALARM_CLIENT','ALARM_NATIVE_UTC',
 'ALARM_SERVICE_TAGGED_V2','ALARM_SERVICE_CLOCK_EDIT_V2','PORTABLE_ALARM_TERMINAL_RETENTION','PORTABLE_DISPLAY_ROTATION=0','PORTABLE_TOUCH_ROTATION=0',
 'TIMECARD_RETURN_APP="springboard.elf"','PORTABLE_HOME_APP="default.elf"']
HELPERS=['PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c','PortableNativeTimeSource.c']
IMPORTS={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','free','strcpy'}
EXPORTS={'app_main','app_module_init','app_module_fini'}
PRIVATE={'portable_native_scene_'+n for n in ['available','begin','layout','present','ready','repaint','back','contact_read']}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(p,*args):return subprocess.check_output(['git','-C',str(p),*args],text=True).strip()
def capture(cmd):return subprocess.check_output(list(map(str,cmd)),text=True)
def require(value,message):
 if not value:raise ValueError(message)
def contract():
 profile=json.loads(PROFILE.read_text());manifest=json.loads(MANIFEST.read_text())
 require(profile['authoritative_model_sha256']==MODEL and sha(ROOT/'Apps/timecard.c')==MODEL,'Authoritative ledger changed')
 require(profile['runtime_sdk']==RUNTIME and profile['alarm_sdk']==ALARM,'Canonical SDK identity changed')
 require(manifest['file_name']=='timecard.elf' and manifest['entry']=='app_main' and manifest['version']==profile['version'],'Manifest identity mismatch')
 maintained=json.loads((ROOT/'Apps/timecard.json').read_text())
 require(all(manifest[k]==maintained[k] for k in ['display_name','icon','category']),'Maintained catalog identity changed')
 needs={(x['capability'],x['api']) for x in manifest['requires']}
 require(len(needs)==len(manifest['requires']) and needs=={('display.output',1),('input.touch.raw',1),('input.navigation',1),('runtime.realtime',1),('storage.key-value',1),('storage.app-data',1),('storage.preferences',1),('alarm.service',2),('power.sleep',1),('display.power',1),('input.power-key',1)},'Unexpected capability contract')
 require(manifest.get('optional')==[{'capability':'board.battery','api':1}],'Historical Quick telemetry must remain optional API1')
 require(not profile['clock_edit_authority'] and not profile['preferences']['writes'] and profile['quick_preference_store']['writes'] and profile['quick_actions_selected'],'Quick workflow and shared preference writes required; no clock editor authority')
 require(profile['storage']['instance']==profile['preferences']['instance']==profile['quick_preference_store']['instance']==1,'Exact independent namespace1 grants required')
 require(profile['quick_preference_store'].get('completed_pending_frame_before_submit') is True,'Copied Quick writes require completed pending feedback')
 return profile,manifest

# Runtime273's app schema excludes catalog-only category. Keep the maintained
# source metadata intact; fail closed for unrecognized future fields.
RUNTIME_MANIFEST_KEYS=frozenset(('type','id','version','architecture','file_name','entry','requires','description','display_name','icon','supported_file_types','runtime_lifecycle','launcher_visible','min_firmware_version','optional'))
CATALOG_ONLY_KEYS=frozenset(('category',))
def runtime_manifest(source):
 require(set(source)<=RUNTIME_MANIFEST_KEYS|CATALOG_ONLY_KEYS,'Unsupported current manifest field')
 return {key:value for key,value in source.items() if key in RUNTIME_MANIFEST_KEYS}

def check_symbols(dynamic,symbols):
 imports={row.split()[-1] for row in dynamic.splitlines() if len(row.split())>=2 and row.split()[-2] in ('U','w','v')}
 exports={row.split()[-1] for row in dynamic.splitlines() if len(row.split())>=3 and row.split()[-2]!='U'}
 require(imports<=IMPORTS and exports==EXPORTS,'Unexpected import/export closure '+repr((imports,exports)))
 private={}
 for name in PRIVATE:
  rows=[row.split() for row in symbols.splitlines() if row.split()[-1:]==[name]]
  require(len(rows)==1 and len(rows[0])>=8 and rows[0][3]=='FUNC' and rows[0][4]=='LOCAL' and rows[0][6]!='UND','Scene hook must be local defined function: '+name)
  private[name]={'binding':rows[0][4],'visibility':rows[0][5]}
 return imports,exports,private

def prepare(args,parser):
 profile,source_manifest=contract();manifest=runtime_manifest(source_manifest);system=args.system_apps.resolve();runtime=args.runtime.resolve();utilities=args.utilities.resolve()
 # Preserve canonical SDK bytes while allowing the selected product Runtime
 # implementation to advance. The shared validator rejects dirty checkouts,
 # non-descendants and changed/removed canonical contracts; additions are not
 # staged implicitly by the foreground helper.
 sys.path.insert(0,str(system/'scripts'))
 runtime_identity=importlib.import_module('runtime_sdk_identity').verify_runtime_sdk(runtime,RUNTIME)
 # Provenance basis is not an ancestry requirement: recovered identical trees
 # may have shallow/no parent history. Exact consumed hashes pin the build.
 scene_header=(system/'lib/PortableApps/include/PortableNativeScene.h').read_text()
 facade=(system/'lib/PortableApps/include/PortableTimecardNative.h').read_text()
 for name in PRIVATE:
  require(name+'(' in scene_header and name in facade,'Missing compatible private scene facade: '+name)
 for rel in ['lib/PortableApps/include/PortableNativeScene.h','lib/PortableApps/include/PortableTimecardNative.h','lib/PortableApps/src/native_scene.inc']:
  require((system/rel).is_file(),'Missing scene dependency: '+rel)
 sys.path.insert(0,str(system/'scripts'))
 idle=importlib.import_module('portable_foreground_idle_build');toolchain=importlib.import_module('native_toolchain')
 out=args.output_dir.resolve()
 if out.exists():raise ValueError('Use a fresh output directory; previous evidence is preserved')
 for root in [ROOT,system,runtime,utilities]:
  if out==root or out in root.parents or (root!=ROOT and out.is_relative_to(root)):raise ValueError('Output cannot replace or modify dependency source roots')
 out.mkdir(parents=True)
 # The existing helper's historical option name selects descriptor suffix
 # recognition only here. No realtime control grant is requested or emitted.
 args.foreground_idle_runtime_dir=runtime;args.navigation=True;args.alarm_client=True
 args.tagged_alarm_utilities=utilities;args.foreground_alarm_sleep=True;args.clock_edit_authority=True
 includes,extra,sleep_sources=idle.configure(args,parser,system,out)
 args.foreground_idle_receipt['alarm_descriptor_clock_edit_feature_recognized']=True
 args.foreground_idle_receipt['clock_edit_authority']=False
 tagged=json.loads((out/'tagged-alarm-sdk.json').read_text());tagged['clock_edit_authority']=False;tagged['clock_edit_feature_recognized']=True
 (out/'tagged-alarm-sdk.json').write_text(json.dumps(tagged,indent=2)+'\n')
 (out/'licenses/tagged-alarm/SOURCES.json').write_text(json.dumps(tagged,indent=2)+'\n')
 (out/'licenses/foreground-idle/SOURCES.json').write_text(json.dumps(args.foreground_idle_receipt,indent=2)+'\n')
 # One canonical include identity; the app's quoted native bridge header must
 # resolve Runtime273's AppData table, not the old copied Productivity SDK.
 extra_headers=['PaperPresentation.h','PaperBattery.h']
 for n in extra_headers:shutil.copyfile(system/'Apps'/n,includes/n)
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 exports=out/'exports.map';exports.write_text('{ global: '+ '; '.join(sorted(EXPORTS))+'; local: *; };\n')
 sources=[ROOT/'Apps/timecard_portable.c',system/'lib/PortableApps/src/adapter.c',*[system/'lib/PortableApps/src'/n for n in ['quick_actions.c','quick_render.c','quick_session.c']],*[system/'lib/PortableApps/src'/n for n in HELPERS],*sleep_sources,catalog]
 defs=list(dict.fromkeys(['-D'+n for n in DEFINES]+extra))
 incs=[includes,system/'lib/NativeApps/include',ROOT/'lib/NativeApps/include',system/'Apps']
 flags=['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror',*defs,*['-I'+str(p) for p in incs]]
 identity={n:{'path':str(p),'commit':git(p,'rev-parse','HEAD'),'dirty':bool(git(p,'status','--porcelain'))} for n,p in [('productivity',ROOT),('system',system),('runtime',runtime),('utilities',utilities)]}
 record={'schema':1,'profile':profile,'manifest':manifest,'source_manifest':source_manifest,'catalog_metadata':{k:source_manifest[k] for k in ('display_name','icon','category')},'hardware_verified':False,'product_recipe':False,'build_defines':defs,'grant_bindings':{'storage.key-value':[1],'storage.app-data':[1],'storage.preferences':[1]},
  'compile_inputs':{'sources':[str(p) for p in sources],'definitions':defs,'include_directories':[str(p) for p in incs],'source_sha256':{str(p):sha(p) for p in sources}},'private_app_data':profile['storage'],'read_only_preferences':False,'timecard_time_format_read_only':True,'shared_preference_writer':'QuickActions via storage.preferences1','key_value_read_only':True,'clock_edit_authority':False,'alarm_descriptor_clock_edit_feature_recognized':True,
  'foreground_idle':args.foreground_idle_receipt,'source_repositories':identity,'source_sha256':{str(p):sha(p) for p in sources},'builder_sha256':sha(__file__),'build_helper_sha256':{str(Path(module.__file__)):sha(module.__file__) for module in [idle,toolchain]},'profile_sha256':sha(PROFILE),'manifest_sha256':sha(MANIFEST),'maintained_catalog_sha256':sha(ROOT/'Apps/timecard.json'),
  'header_sha256':{str(p.relative_to(includes)):sha(p) for p in sorted(includes.rglob('*.h'))},'pending_product_grant_binding':'Deployment-owned explicit AppData instance1; never diagnostic instance61 or wildcard0'}
 for name,root in [('productivity',ROOT),('system',system)]:
  for source in [root/'LICENSE',*sorted((root/'lib/PortableApps').rglob('*'))]:
   if source.is_file() and (source.name.startswith('LICENSE') or 'OFL' in source.name or source.name.endswith('SOURCES.json') or source.name=='TIMEZONE_PROVENANCE.json'):
    destination=out/'licenses'/name/source.relative_to(root);destination.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,destination)
 record['runtime_sdk_identity']=runtime_identity
 return out,system,toolchain,manifest,sources,flags,record

def build(args,parser):
 out,system,toolchain,manifest,sources,flags,record=prepare(args,parser)
 cc=toolchain.compiler_path(args.compiler);banner=capture([cc,'--version']).splitlines()[0]
 if not re.search(r'\b15\.2(?:\.0)?\b',banner):raise ValueError('Current profile requires ESP32-S3 GCC15.2; got '+banner)
 record['compiler']=banner;record['compiler_path']=cc
 consumed={}
 for source in sources:
  dependencies=capture([cc,*flags,'-MM',source]).replace('\\\n',' ').split(':',1)[1].split()
  for path in dependencies:
   p=Path(path).resolve();consumed[str(p)]=sha(p)
 record['consumed_source_sha256']=consumed
 elf=out/'timecard.elf';command=[cc,*flags,'-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(out/'exports.map'),*map(str,sources),'-lgcc','-o',str(elf)]
 record['compile_command']=command;(out/'compile-command.json').write_text(json.dumps(command,indent=2)+'\n')
 subprocess.run(command,check=True)
 prefix=cc.removesuffix('gcc');dynamic=capture([prefix+'nm','-D',elf]);symbols=capture([prefix+'readelf','--wide','--symbols',elf])
 imports,exports,private=check_symbols(dynamic,symbols)
 data=elf.read_bytes()
 if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Expected little-endian ELF32 Xtensa ET_DYN')
 # Same production loader validator used by preserved Productivity targets.
 validator=out/'validate-elf';subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
 record['loader_validation']=capture([validator,elf]).strip()
 for name,digest in consumed.items():
  if sha(name)!=digest:raise ValueError('Source changed while building: '+name)
 contract();record.update({'elf_sha256':sha(elf),'elf_bytes':len(data),'imports':sorted(imports),'exports':sorted(exports),'private_hooks':private})
 (out/'dynamic-symbols.txt').write_text(dynamic);(out/'symbols.txt').write_text(symbols)
 (out/'timecard.json').write_text(json.dumps(manifest,indent=2)+'\n');(out/'timecard-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
 print('PASS current Timecard target/loader/import closure; product binding and hardware remain unverified')

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['system-apps','runtime','utilities','foreground-idle-helper-dir','foreground-idle-drivers-sdk-dir','output-dir']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--foreground-idle-fallback-seconds',type=int,default=15);p.add_argument('--compiler')
 a=p.parse_args();build(a,p)
if __name__=='__main__':main()

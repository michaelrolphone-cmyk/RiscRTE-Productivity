#!/usr/bin/env python3
"""Explicit current Points app composition; frozen native builders stay unchanged.

Produces an app and exact compiler-input evidence, not an installation/release.
The product owns namespace admission and the mixed alarm-service selection.
"""
import argparse,hashlib,importlib,json,os,re,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
MANIFEST=ROOT/'Apps/native/points_current.json'
RUNTIME_SDK='273bdbccf65647a803ab0586a418f8283732baec'
DEFINES=['PORTABLE_POINTS_NATIVE_UI','PORTABLE_NATIVE_SCENE','PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_RF_NATIVE_CANVAS',
 'PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_WATCH_NATIVE_HEIGHT','PORTABLE_WATCH_COLOR_ONLY','PORTABLE_WATCH_SAFE_INSETS','PORTABLE_SAFE_VIEWPORT',
 'PORTABLE_FORCE_FULL_FRAMES','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_INPUT_NAVIGATION',
 'PORTABLE_ALARM_CLIENT','ALARM_NATIVE_UTC','ALARM_SERVICE_TAGGED_V2','ALARM_SERVICE_CLOCK_EDIT_V2','PORTABLE_ALARM_TERMINAL_RETENTION','POINTS_SERVICE_CONTEXT',
 'PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_PENDING_FRAME','PORTABLE_QUICK_NATIVE_GEOMETRY','PORTABLE_QUICK_SAFE_CONTENT','PORTABLE_QUICK_CAPABILITY_SAFE',
 'PORTABLE_QUICK_ATOMIC_PREFERENCES','PORTABLE_PREFERENCE_STORE','PORTABLE_DISPLAY_ROTATION=0','PORTABLE_TOUCH_ROTATION=0',
 'POINTS_RETURN_APP="springboard.elf"','PORTABLE_HOME_APP="default.elf"']
HELPERS=['PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c','quick_actions.c','quick_render.c','quick_session.c']
PRIVATE={'portable_native_scene_'+name for name in ('available','begin','layout','present','ready','repaint','back','contact_read')}
REQUIRES={('display.output',1),('input.touch.raw',1),('input.navigation',1),('runtime.realtime',1),('storage.key-value',1),('storage.preferences',1),('alarm.service',2),('power.sleep',1),('display.power',1),('input.power-key',1)}
IMPORTS={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','free','strcpy'}
EXPORTS={'app_main','app_module_init','app_module_fini'}
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def capture(command):return subprocess.check_output(list(map(str,command)),text=True)
def git(path,*args):return capture(['git','-C',path,*args]).strip()
def require(ok,message):
 if not ok:raise ValueError(message)
def revision(value):
 if not re.fullmatch('[0-9a-f]{40}',value):raise argparse.ArgumentTypeError('Full lowercase source commit required')
 return value

def contract():
 manifest=json.loads(MANIFEST.read_text());maintained=json.loads((ROOT/'Apps/points_in_time.json').read_text())
 require(set(manifest)=={'type','id','version','architecture','file_name','entry','display_name','icon','requires','optional'},'Unsupported Points manifest fields')
 require(manifest.get('type')=='application' and manifest.get('architecture')=='xtensa-esp32s3','Points target type/architecture changed')
 require(manifest['id']=='points_in_time' and manifest['file_name']=='points_in_time.elf' and manifest['entry']=='app_main','Points identity changed')
 require(all(manifest[k]==maintained[k] for k in ('display_name','icon')),'Maintained launcher identity changed')
 needs={(row['capability'],row['api']) for row in manifest['requires']}
 require(needs==REQUIRES and len(needs)==len(manifest['requires']),'Points capability closure changed')
 require(manifest.get('optional')==[{'capability':'board.battery','api':1}],'Battery remains optional')
 return manifest

def build(a,parser):
 manifest=contract();system=a.system_apps.resolve();runtime=a.runtime.resolve();utilities=a.utilities.resolve();out=a.output_dir.resolve()
 for root,pin in ((system,a.system_revision),(utilities,a.utilities_revision)):
  require(git(root,'rev-parse','HEAD')==pin,'Exact explicitly selected source required: '+str(root))
 sys.path.insert(0,str(system/'scripts'))
 identity=importlib.import_module('runtime_sdk_identity').verify_runtime_sdk(runtime,RUNTIME_SDK)
 idle=importlib.import_module('portable_foreground_idle_build');toolchain=importlib.import_module('native_toolchain')
 facade=(system/'lib/PortableApps/include/PortablePointsNative.h').read_text()
 require('PortableNativeScene.h' in facade,'Current Points requires generalized scene façade, not an old private owner')
 require(not out.exists(),'Fresh output required')
 for root in (ROOT,system,runtime,utilities):require(out!=root and out not in root.parents and not out.is_relative_to(root),'Output must be outside source roots')
 out.mkdir(parents=True)
 a.foreground_idle_runtime_dir=runtime;a.navigation=True;a.alarm_client=True;a.tagged_alarm_utilities=utilities;a.foreground_alarm_sleep=True;a.clock_edit_authority=True
 include,extra,sleep_sources=idle.configure(a,parser,system,out)
 a.foreground_idle_receipt['alarm_descriptor_clock_edit_feature_recognized']=True;a.foreground_idle_receipt['clock_edit_authority']=False
 # Descriptor recognition is not a realtime-control grant or editor call.
 for name in ('tagged-alarm-sdk.json','licenses/tagged-alarm/SOURCES.json'):
  p=out/name;r=json.loads(p.read_text());r['clock_edit_authority']=False;r['clock_edit_feature_recognized']=True;p.write_text(json.dumps(r,indent=2)+'\n')
 (out/'licenses/foreground-idle/SOURCES.json').write_text(json.dumps(a.foreground_idle_receipt,indent=2)+'\n')
 for name in ('PaperPresentation.h','PaperBattery.h'):shutil.copyfile(system/'Apps'/name,include/name)
 # New context headers are explicit inputs; do not reuse an older staged copy.
 for name in ('PointsServiceContext.h','AlarmServiceExtensionV2.h','PointsRecords.h'):
  shutil.copyfile(utilities/'lib/Alarm/include'/name,include/name)
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 mapping=out/'exports.map';mapping.write_text('{ global: '+ '; '.join(sorted(EXPORTS))+'; local: *; };\n')
 sources=[ROOT/'Apps/points_in_time.c',system/'lib/PortableApps/src/adapter.c',*[system/'lib/PortableApps/src'/name for name in HELPERS],*sleep_sources,catalog]
 definitions=list(dict.fromkeys(['-D'+value for value in DEFINES]+extra))
 includes=[include,utilities/'lib/Alarm/include',system/'lib/NativeApps/include',ROOT/'lib/NativeApps/include',system/'Apps']
 flags=['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror',*definitions,*['-I'+str(path) for path in includes]]
 cc=toolchain.compiler_path(a.compiler);banner=capture([cc,'--version']).splitlines()[0]
 require('esp-15.2.0_20251204' in banner and capture([cc,'-dumpfullversion']).strip()=='15.2.0','Pinned GCC15.2 required')
 consumed={}
 for source in sources:
  dependencies=capture([cc,*flags,'-MM',source]).replace('\\\n',' ').split(':',1)[1].split()
  for path in dependencies:p=Path(path).resolve();consumed[str(p)]=sha(p)
 elf=out/'points_in_time.elf';command=[cc,*flags,'-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),*map(str,sources),'-lgcc','-o',str(elf)]
 subprocess.run(command,check=True)
 dynamic=capture([cc.removesuffix('gcc')+'nm','-D',elf]);symbols=capture([cc.removesuffix('gcc')+'readelf','--wide','--symbols',elf])
 imports={line.split()[-1] for line in dynamic.splitlines() if len(line.split())>=2 and line.split()[-2] in ('U','w','v')}
 exports={line.split()[-1] for line in dynamic.splitlines() if len(line.split())>=3 and line.split()[-2]!='U'}
 require(imports<=IMPORTS and exports==EXPORTS,'Unexpected dynamic closure '+repr((imports,exports)))
 for name in PRIVATE:
  rows=[line.split() for line in symbols.splitlines() if line.split()[-1:]==[name]]
  require(len(rows)==1 and rows[0][3]=='FUNC' and rows[0][4]=='LOCAL' and rows[0][6]!='UND','Missing private scene function: '+name)
 data=elf.read_bytes();require(data[:7]==b'\x7fELF\x01\x01\x01' and data[16:20]==b'\x03\x00\x5e\x00','Expected little-endian Xtensa ET_DYN')
 validator=out/'validate-elf';subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(runtime/'lib/elf_loader/include'),str(runtime/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
 validation=capture([validator,elf]).strip()
 for path,digest in consumed.items():require(sha(path)==digest,'Source changed during build: '+path)
 contract()
 roots={'productivity':ROOT,'system':system,'runtime':runtime,'utilities':utilities}
 record={'schema':1,'profile':'points-current-badge','version':manifest['version'],'manifest':manifest,'runtime_sdk_identity':identity,'compiler':banner,'compiler_sha256':sha(Path(cc).resolve()),'compile_command':command,'build_defines':definitions,'compile_inputs':{'sources':list(map(str,sources)),'definitions':definitions,'include_directories':list(map(str,includes))},'source_repositories':{name:{'commit':git(root,'rev-parse','HEAD'),'dirty':bool(git(root,'status','--porcelain'))} for name,root in roots.items()},'consumed_source_sha256':consumed,'builder_sha256':sha(__file__),'manifest_sha256':sha(MANIFEST),'foreground_idle':a.foreground_idle_receipt,'grant_bindings':{'storage.key-value':[1,5],'storage.preferences':[1]},'points_config_namespace':5,'service_occurrence_access':False,'clock_edit_authority':False,'service_projection_context':True,'quick_pending_frame_required':True,'elf_sha256':sha(elf),'elf_bytes':len(data),'imports':sorted(imports),'exports':sorted(exports),'loader_validation':validation,'hardware_verified':False,'product_recipe':False}
 (out/'points_in_time.json').write_text(json.dumps(manifest,indent=2)+'\n');(out/'points-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
 (out/'compile-command.json').write_text(json.dumps(command,indent=2)+'\n');(out/'dynamic-symbols.txt').write_text(dynamic);(out/'symbols.txt').write_text(symbols)
 for name,root in roots.items():
  for source in [root/'LICENSE',*sorted((root/'lib/PortableApps').rglob('*'))]:
   if source.is_file() and (source.name.startswith('LICENSE') or 'OFL' in source.name or source.name.endswith('SOURCES.json') or source.name=='TIMEZONE_PROVENANCE.json'):
    destination=out/'licenses'/name/source.relative_to(root);destination.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,destination)
 print('PASS current Points app/loader; product binding and hardware remain unverified')
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('system-apps','runtime','utilities','foreground-idle-helper-dir','foreground-idle-drivers-sdk-dir','output-dir'):p.add_argument('--'+name,type=Path,required=True)
 for name in ('system-revision','utilities-revision'):p.add_argument('--'+name,type=revision,required=True)
 p.add_argument('--alarm-ready-drain',action='store_true')
 p.add_argument('--foreground-idle-fallback-seconds',type=int,default=15);p.add_argument('--compiler');a=p.parse_args();build(a,p)
if __name__=='__main__':main()

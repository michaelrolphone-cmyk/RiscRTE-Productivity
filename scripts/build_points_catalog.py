#!/usr/bin/env python3
"""Build explicit storage-scaled Points 0.6.11. No legacy recipe is changed."""
import argparse,hashlib,json,os,re,shutil,subprocess
from pathlib import Path
from points_catalog_sdk import stage_headers
from build_points_in_time import IMPORTS,EXPORTS
ROOT=Path(__file__).resolve().parents[1]
HELPERS=('PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def git(path,*args):return subprocess.check_output(['git','-C',str(path),*args],text=True).strip()
def run(cmd,cwd=None):subprocess.run(list(map(str,cmd)),cwd=cwd,check=True,timeout=180)
def exact(path,revision):
    if not re.fullmatch('[0-9a-f]{40}',revision):raise ValueError('Full selected dependency revision required')
    if git(path,'rev-parse','HEAD')!=revision or git(path,'status','--porcelain','--untracked-files=all'):
        raise ValueError('Clean exact selected dependency required: '+str(path)+' @ '+revision)

def defines_for(target):
    flags=['-DPORTABLE_TEXT_INPUT_CLIENT','-DPORTABLE_APP_LAUNCH_GUARD','-DPORTABLE_NOVA_UI','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_ALARM_TERMINAL_RETENTION','-DPOINTS_RETURN_APP="springboard.elf"','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"']
    if target=='x4':flags+=['-DALARM_NATIVE_UTC','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_QUICK_ACTIONS']
    else:flags+=['-DPORTABLE_RTC_WALL_TIME']
    return flags

def validate_text_input_manifest(manifest):
    selected=[entry for entry in manifest.get('requires',[]) if entry.get('capability')=='ui.text-input']
    if selected!=[{'capability':'ui.text-input','api':1}]:raise ValueError('Selected catalog requires ui.text-input@1 exactly once')
    if any(entry.get('capability')=='ui.text-input' for entry in manifest.get('optional',[])):raise ValueError('Native loader does not admit optional text input')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--target',choices=['watch','x4'],required=True)
    for name in ('adapter-system','presentation-system','utilities','runtime'):
        p.add_argument('--'+name,type=Path,required=True);p.add_argument('--'+name+'-revision',required=True)
    p.add_argument('--ble-broadcast',action='store_true');p.add_argument('--contexts',action='store_true')
    p.add_argument('--watch-firmware',type=Path);p.add_argument('--watch-firmware-revision')
    p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args()
    deps={name:getattr(a,name.replace('-','_')).resolve() for name in ('adapter-system','presentation-system','utilities','runtime')}
    for name,path in deps.items():exact(path,getattr(a,name.replace('-','_')+'_revision'))
    adapter,presentation,utilities,runtime=(deps[n] for n in ('adapter-system','presentation-system','utilities','runtime'))
    if 'PORTABLE_ALARM_TERMINAL_RETENTION' not in (adapter/'lib/PortableApps/include/PortableAlarmClient.h').read_text():raise ValueError('Selected terminal alarm custody adapter required')
    if a.contexts and a.target!='watch':p.error('Contexts is selected only on Watch')
    if a.contexts and not a.watch_firmware:p.error('Watch Contexts requires the exact local sleep firmware source')
    if a.watch_firmware:
        if a.target!='watch' or not a.watch_firmware_revision:p.error('Watch firmware requires Watch target and exact revision')
        a.watch_firmware=a.watch_firmware.resolve();exact(a.watch_firmware,a.watch_firmware_revision);deps['watch-firmware']=a.watch_firmware
    out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    includes=stage_headers(out,adapter,presentation,utilities,runtime)
    manifest=json.loads((ROOT/f'Apps/native/points_catalog_{a.target}.json').read_text());assert manifest['version']=='0.6.11'
    validate_text_input_manifest(manifest)
    defines=defines_for(a.target)
    if a.ble_broadcast:defines+=['-DPORTABLE_BLE_BROADCAST'];manifest['requires'].append({'capability':'telemetry.broadcast','api':1})
    if a.contexts:defines+=['-DPORTABLE_CONTEXTS_CLIENT'];manifest['requires'].append({'capability':'contexts.service','api':1})
    if a.watch_firmware:
        defines+=['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_APP_SLEEP_LOCAL','-DWATCH_ALARM_SLEEP_RESUME','-DPORTABLE_LOW_BATTERY','-DPORTABLE_QUICK_RADIOS']
        manifest['requires'] += [{'capability':'net.wifi','api':1},{'capability':'bluetooth.hci','api':1}]
        includes += [a.watch_firmware/'sdk/driver',a.watch_firmware/'include',a.watch_firmware/'apps/clock']
    cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:raise ValueError('Set NATIVE_APP_CC to pinned Xtensa GCC8.4.0 2021r2-patch5')
    compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
    if '8.4.0' not in compiler or '2021r2-patch5' not in compiler:raise ValueError('Pinned GCC8.4 compiler required')
    catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    mapping=out/'exports.map';mapping.write_text('{ global: '+'; '.join(sorted(EXPORTS))+'; local: *; };\n')
    sources=[ROOT/'Apps/points_catalog_app.c',adapter/'lib/PortableApps/src/adapter.c',catalog]
    if a.watch_firmware:
        sources += [a.watch_firmware/'apps/clock/portable_sleep.c',*[adapter/'lib/PortableApps/src'/name for name in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]]
    if a.target=='x4':sources += [presentation/'lib/PortableApps/src'/name for name in HELPERS]+[adapter/'lib/PortableApps/src'/name for name in ('quick_actions.c','quick_render.c','quick_session.c')]
    elf=out/manifest['file_name']
    command=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-fstack-usage','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*defines,*['-I'+str(x) for x in includes],*sources,'-lgcc','-o',elf]
    run(command,cwd=out)
    symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
    imports={x.split()[-1] for x in symbols.splitlines() if ' U ' in ' '+x}
    exports={x.split()[-1] for x in symbols.splitlines() if len(x.split())>=3 and x.split()[-2] in ('T','D','B','R')}
    if not imports<=(IMPORTS|{'memmove','memchr','strncmp'}) or exports!=EXPORTS:raise ValueError(('Unexpected target ABI',imports-IMPORTS-{'memmove','memchr','strncmp'},exports))
    data=elf.read_bytes()
    if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Expected Xtensa ELF32 ET_DYN')
    validator=out/'validate-elf';run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator]);run([validator,elf])
    elf.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
    source_files=['Apps/points_catalog_app.c','Apps/points_catalog_controller.h','Apps/points_catalog_view.inc','Apps/points_catalog_paper.inc','lib/PointsCatalog/fonts/text.inc','lib/PointsCatalog/fonts/SOURCES.json','scripts/generate_points_catalog_fonts.py',f'Apps/native/points_catalog_{a.target}.json','scripts/build_points_catalog.py','scripts/points_catalog_sdk.py']
    stacks=[]
    for path in out.glob('*.su'):
        for line in path.read_text().splitlines():
            fields=line.split('\t')
            if len(fields)>=3:stacks.append({'function':fields[0],'bytes':int(fields[1]),'kind':fields[2]})
    stacks.sort(key=lambda row:row['bytes'],reverse=True)
    if any(row['bytes']>=16384 for row in stacks):raise ValueError('Individual stack frame exceeds selected app stack')
    receipt={'schema':1,'app':'points_in_time','version':'0.6.11','target':a.target,'source_revision':git(ROOT,'rev-parse','HEAD'),'working_tree_dirty':bool(git(ROOT,'status','--porcelain')),'selected_source':'Apps/points_catalog_app.c','dependency_revisions':{n:git(path,'rev-parse','HEAD') for n,path in deps.items()},'defines':defines,'compile_command':list(map(str,command)),'compiler':compiler,'sha256':sha(elf),'size_bytes':len(data),'imports':sorted(imports),'exports':sorted(exports),'requires':manifest['requires'],'source_sha256':{n:sha(ROOT/n) for n in source_files},'sdk_sha256':{p.name:sha(p) for p in includes[0].glob('*.h')},'stack_frames':stacks,'hardware_verified':False,'time_domain':'native UTC' if a.target=='x4' else 'raw RTC','app_data':{'namespace':5,'file':'points.catalog','access':'read-write','atomic_replace':True},'legacy_key_value':{'namespace':5,'access':'read-only','keys':['points_utc_cfg','points_utc_meta'] if a.target=='x4' else ['points_cfg','points_meta']},'preferences':{'namespace':1,'access':'read-only','keys':['time_format','time_zone'] if a.target=='x4' else ['time_format']},'capacity':'Selected storage backend byte/volume limits; no fixed application event/type count','sd_migration':False}
    (out/'build-evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
    for name,path in [('Productivity',ROOT),('System-Apps',adapter),('Utilities',utilities),('Runtime',runtime)]:
        dest=out/'licenses'/name;dest.mkdir(parents=True,exist_ok=True);shutil.copyfile(path/'LICENSE',dest/'LICENSE')
    for folder in ('fonts','paper_fonts','settings_fonts','quick_fonts','time'):
        for path in (adapter/'lib/PortableApps'/folder).rglob('*'):
            if path.is_file() and ('LICENSE' in path.name or 'OFL' in path.name or path.name.endswith('SOURCES.json')):
                dest=out/'licenses/System-Apps'/folder/path.relative_to(adapter/'lib/PortableApps'/folder);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,dest)
    font_dest=out/'licenses/PointsCatalog-fonts';font_dest.mkdir(parents=True,exist_ok=True)
    for path in (ROOT/'lib/PointsCatalog/fonts').glob('LICENSE*'):shutil.copyfile(path,font_dest/path.name)
    shutil.copyfile(ROOT/'lib/PointsCatalog/fonts/SOURCES.json',font_dest/'SOURCES.json')
    print(f"Points 0.6.11 {a.target}: target ELF/imports/exports validated; largest individual stack frame {stacks[0]['bytes'] if stacks else 'unreported'} bytes; no hardware verification")
if __name__=='__main__':main()

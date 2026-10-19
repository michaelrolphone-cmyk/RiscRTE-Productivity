#!/usr/bin/env python3
"""Build a frozen Lists/scene/civil software closure; never compose firmware."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from build_lists_scene import load, run as build_lists
ROOT=Path(__file__).resolve().parents[1]
PROFILE=ROOT/'profiles/native-lists-qualification.json'

def git(path,*args):
    return subprocess.check_output(['git','-C',str(path),*args],text=True).strip()

def validate(runtime,system,profile,root=ROOT):
    if profile.get('schema')!=1 or profile.get('id')!='native-lists-qualification':
        raise ValueError('Unsupported Lists qualification profile')
    if profile.get('host_profile')!='color-dynamic':
        raise ValueError('Qualification requires the native color-dynamic host')
    if profile.get('sizes')!=['160x160','240x240','320x180','410x502','502x410']:
        raise ValueError('Incomplete native size matrix')
    if profile.get('capabilities')!=['storage.app-data@1','time.civil@1','ui.scene@1']:
        raise ValueError('Lists capability closure changed')
    if profile.get('compiler')!='esp-15.2.0_20251204':
        raise ValueError('Qualification compiler changed')
    for name,path in (('runtime',runtime),('system',system)):
        if git(path,'rev-parse','HEAD')!=profile[name]['commit']:
            raise ValueError(name+' checkout is not the exact qualified commit')
        if git(path,'rev-parse','HEAD^{tree}')!=profile[name]['tree']:
            raise ValueError(name+' checkout tree does not match qualification')
        if git(path,'status','--porcelain','--untracked-files=no'):
            raise ValueError(name+' checkout has modified tracked source')
    required={'Apps/lists_scene.c','Apps/ListsScene.c','Apps/ListsScene.h','Apps/lists_scene.json',
              'lib/Lists/ListsModel.c','lib/Lists/ListsModel.h','lib/Lists/ListsStore.c','lib/Lists/ListsStore.h',
              'deployment/lists/watch.json','deployment/lists/x4.json'}
    if set(profile.get('preserved_source_sha256',{}))!=required:
        raise ValueError('Missing preserved semantic/storage/deployment source')
    for path,expected in profile['preserved_source_sha256'].items():
        if hashlib.sha256((root/path).read_bytes()).hexdigest()!=expected:
            raise ValueError('Preserved source changed: '+path)
    manifest=json.loads((root/'Apps/lists_scene.json').read_text())
    requires=sorted(r['capability']+'@'+str(r['api']) for r in manifest['requires'])
    if requires!=profile['capabilities']:
        raise ValueError('Application capability manifest differs')
    return {'system':profile['system'],'runtime':profile['runtime'],
            'preserved_source_sha256':profile['preserved_source_sha256']}

def verify_compiler(system,expected):
    builder=load('lists_qualification_scene_builder',system/'scripts/scene_build.py')
    compiler=builder.compiler_path()
    version=subprocess.check_output([compiler,'--version'],text=True).splitlines()[0]
    if expected not in version:
        raise ValueError('Use the exact SDK 6.1 compiler: '+expected)
    macros=subprocess.check_output([compiler,'-dM','-E','-xc','/dev/null'],text=True).splitlines()
    if '#define __XTENSA_EL__ 1' not in macros or '#define __XCHAL_HAVE_FP 1' not in macros:
        raise ValueError('Compiler is not the little-endian ESP32-S3 frontend')
    return version

def run(args):
    runtime,system=args.runtime.resolve(),args.system_apps.resolve()
    profile=json.loads(PROFILE.read_text())
    receipt=validate(runtime,system,profile)
    if args.verify_only:
        print('Exact Lists semantic sources, System/Runtime pins and native profile verified.')
        return
    version=verify_compiler(system,profile['compiler'])
    output=args.output.resolve()
    if output.exists():raise FileExistsError('Choose a new qualification output')
    output.mkdir(parents=True)
    build_lists(runtime,system,output/'lists')
    load('lists_scene_services',system/'scripts/build_scene_services.py').run(runtime,output/'scene',profile['host_profile'])
    load('lists_civil_services',system/'scripts/build_civil_clock.py').run(runtime,output/'civil')
    receipt.update(physical_testing='not performed',product_recipe='not produced',
                   host_profile=profile['host_profile'],compiler=version,
                   application_commit=git(ROOT,'rev-parse','HEAD'),
                   application_tree_status=git(ROOT,'status','--porcelain','--untracked-files=no'),
                   elf_sha256={str(p.relative_to(output)):hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in sorted(output.rglob('*.elf'))})
    (output/'qualification.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Frozen Lists application and five providers passed ELF/import/export validation.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('runtime','system-apps','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--verify-only',action='store_true')
    run(p.parse_args())

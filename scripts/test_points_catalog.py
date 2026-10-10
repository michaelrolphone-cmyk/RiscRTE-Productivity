#!/usr/bin/env python3
"""Production catalog model/controller and real Watch/X4 renderer qualification."""
import argparse,json,os,subprocess
from pathlib import Path
from points_catalog_sdk import stage_headers
from build_points_catalog import defines_for,HELPERS,sha,git,validate_text_input_manifest
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('utilities','runtime','presentation-system'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--scene',choices=['editor-navigation','text-input','text-retention','text-abandoned','text-alarm','text-frame','text-runtime-1','text-runtime-2','text-runtime-3','text-runtime-4','text-runtime-5','text-runtime-6','types-scroll'],help='Run one focused actual-renderer scenario in both sanitizer modes')
    p.add_argument('--expect-fast-paper',action='store_true')
    p.add_argument('--raster-snapshot',action='store_true',help='Exercise the production asynchronous recorder and replay path')
    p.add_argument('--watch-system',type=Path);p.add_argument('--x4-system',type=Path)
    p.add_argument('--output-dir',type=Path,default=ROOT/'build/points-catalog')
    a=p.parse_args()
    if not(a.watch_system or a.x4_system):p.error('At least one real selected adapter is required')
    base=a.output_dir.resolve();base.mkdir(parents=True,exist_ok=True)
    deps={n:getattr(a,n).resolve() for n in ('utilities','runtime','presentation_system')}
    for target in ('watch','x4'):validate_text_input_manifest(json.loads((ROOT/f'Apps/native/points_catalog_{target}.json').read_text()))
    cc=os.environ.get('CC','cc');passed=[]
    for target,adapter in [('watch',a.watch_system),('x4',a.x4_system)]:
        if adapter is None:continue
        adapter=adapter.resolve();out=base/target;out.mkdir(exist_ok=True);frames=out/'frames';frames.mkdir(exist_ok=True)
        includes=stage_headers(out,adapter,deps['presentation_system'],deps['utilities'],deps['runtime'])
        inc=['-I'+str(path) for path in includes]
        sources=[ROOT/'test/native_apps/points_catalog_app_test.c',adapter/'lib/PortableApps/src/adapter.c']
        if target=='x4':sources += [deps['presentation_system']/'lib/PortableApps/src'/name for name in HELPERS]+[adapter/'lib/PortableApps/src'/name for name in ('quick_actions.c','quick_render.c','quick_session.c')]
        for sanitize in [False,True]:
            flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if sanitize else []
            for name,files,defines in [('controller',[ROOT/'test/native_apps/points_catalog_controller_test.c'],[]),('app-renderer',sources,defines_for(target)),('app-background',sources,[*defines_for(target),'-DPC_FAKE_BACKGROUND'])]:
                if a.scene and name!='app-renderer':continue
                if a.raster_snapshot and name.startswith('app-'):defines=[*defines,'-DPORTABLE_RASTER_SNAPSHOT']
                if a.expect_fast_paper and target=='x4':defines=[*defines,'-DTEST_EXPECT_FAST_PAPER']
                binary=out/f'{name}-{int(sanitize)}'
                subprocess.run([cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*defines,*inc,*map(str,files),'-o',str(binary)],check=True)
                subprocess.run([str(binary),*([a.scene] if a.scene else [])],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','POINTS_CATALOG_FRAMES':str(frames)})
                if name.startswith('app-') and not a.scene:
                    scenarios=['background-context','background-broadcast','background-open','background-close'] if name=='app-background' else ['acquire-loss','initial-draw-retention','replace-retention','projection-retention','read-retention','resolve-read-retention','release-retention','home-uncertain','old-service','service-busy','controls','editor-navigation','text-input','text-retention','text-abandoned','text-alarm','text-frame','text-runtime-1','text-runtime-2','text-runtime-3','text-runtime-4','text-runtime-5','text-runtime-6']
                    if name=='app-renderer' and target=='x4':scenarios += ['types-scroll','geometry','reference']
                    for scenario in scenarios:
                        subprocess.run([str(binary),scenario],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','POINTS_CATALOG_FRAMES':str(frames)})
                passed.append(f'{target}/{name}/'+('ASan-UBSan' if sanitize else 'normal'))
        try:
            from PIL import Image
            for path in frames.glob('*.pbm'):Image.open(path).rotate(-90,expand=True).save(path.with_suffix('.png'))
            for path in frames.glob('*.ppm'):Image.open(path).save(path.with_suffix('.png'))
        except ImportError:pass
    receipt={'tests':passed,'leak_sanitizer':'Disabled: this executor ptrace prevents LeakSanitizer; ASan and UBSan remain enabled','dependency_revisions':{n:git(path,'rev-parse','HEAD') for n,path in deps.items()},'adapter_revisions':{n:git(path.resolve(),'rev-parse','HEAD') for n,path in [('watch',a.watch_system),('x4',a.x4_system)] if path},'production_source_sha256':{str(p.relative_to(ROOT)):sha(p) for p in sorted((ROOT/'Apps').glob('points_catalog*'))},'hardware_verified':False}
    (base/'test-evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Selected Points catalog: '+', '.join(passed))
if __name__=='__main__':main()

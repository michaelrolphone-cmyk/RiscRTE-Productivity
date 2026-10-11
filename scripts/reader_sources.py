"""Pinned upstream materialization. Patches are reviewed separately from the imported engine."""
import hashlib,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
LIBS='Epub EpdFont GfxRenderer Txt ZipFile LibraryIndex InflateReader FsHelpers Serialization Utf8 XmlParserUtils MiniBidi Memory expat miniz uzlib JpegToBmpConverter PngToBmpConverter'.split()
def stage(source,output,apply_patches=True):
 source,output=source.resolve(),output.resolve()
 if source in output.parents or output==source or output in source.parents or output==ROOT or output in ROOT.parents:raise ValueError('Staging output aliases source checkout')
 lock=json.loads((ROOT/'reader/upstream.json').read_text())
 actual=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
 if actual!=lock['commit']:raise ValueError('CrossPoint source differs from upstream lock')
 if subprocess.run(['git','-C',str(source),'diff-index','--quiet','HEAD','--']).returncode:raise ValueError('CrossPoint tracked source is modified')
 sdk=source/'freeink-sdk'
 if subprocess.check_output(['git','-C',str(sdk),'rev-parse','HEAD'],text=True).strip()!=lock['sdk_commit']:raise ValueError('FreeInk SDK differs from upstream lock')
 if subprocess.run(['git','-C',str(sdk),'diff-index','--quiet','HEAD','--']).returncode:raise ValueError('FreeInk tracked source is modified')
 if output.exists():shutil.rmtree(output)
 output.mkdir(parents=True)
 for lib in LIBS:shutil.copytree(source/'lib'/lib,output/'lib'/lib)
 (output/'src').mkdir()
 for name in ['fontIds.h','BookmarkEntry.h','ReaderFontSizes.h','ReaderFontSizes.cpp']:
  shutil.copyfile(source/'src'/name,output/'src'/name)
 shutil.copytree(sdk/'libs/font/FreeInkFont',output/'freeink-font')
 shutil.copytree(sdk/'libs/book/ContentProtection/include',output/'protection')
 for patch in lock['patches'] if apply_patches else []:
  p=ROOT/patch
  if not p.is_file() or not p.stat().st_size:raise ValueError('Missing recorded upstream patch')
  subprocess.run(['git','apply',str(p.resolve())],cwd=output,check=True)
 return output

def inputs(upstream):
 cpp=[];cc=[]
 for lib in LIBS:
  cpp+=list((upstream/'lib'/lib).rglob('*.cpp'))
  if lib in ('uzlib','MiniBidi'):cc+=list((upstream/'lib'/lib).glob('*.c'))
 cpp=[p for p in cpp if p.name not in ('ContentProtection.cpp','BookKey.cpp','ObfuscationUtils.cpp','PersistableStore.cpp')]
 cc += [upstream/'lib/expat'/n for n in ('xmlparse.c','xmlrole.c','xmltok.c')]
 cc += [upstream/'lib/miniz/src/miniz_impl.c']
 cpp += [upstream/'src/ReaderFontSizes.cpp']
 cpp += [upstream/'freeink-font/src'/n for n in ('FtFont.cpp','Gpos.cpp','Gsub.cpp')]
 cc += list((upstream/'freeink-font/src/freetype').glob('*.c'))+[upstream/'freeink-font/src/FontAlloc.c']
 inc=[upstream/'src',upstream/'lib/uzlib/src']+[upstream/'lib'/lib for lib in LIBS]+[upstream/'lib',upstream/'lib/Epub/Epub',upstream/'lib/miniz/src',upstream/'lib/miniz/third_party',upstream/'freeink-font/include',upstream/'freeink-font/third_party/freetype/include',upstream/'protection']
 return cpp,cc,inc

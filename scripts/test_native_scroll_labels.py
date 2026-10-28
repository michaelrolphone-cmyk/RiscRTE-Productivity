#!/usr/bin/env python3
"""Compile both shared label selections, preserving frozen presentation bytes."""
import os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='native-scroll-labels-') as tmp:
 root=Path(tmp);source=root/'test.c';source.write_text('#include <assert.h>\n#include <string.h>\n#include "native_scroll_labels.h"\nint main(void){assert(!strcmp(PRODUCTIVITY_NATIVE_SCROLL_UP_LABEL,UP));assert(!strcmp(PRODUCTIVITY_NATIVE_SCROLL_DOWN_LABEL,DOWN));return 0;}\n')
 for label,defs in [('current',['-DPORTABLE_NATIVE_SCENE','-DUP="UP"','-DDOWN="DN"']),('frozen',['-DUP="^"','-DDOWN="v"'])]:
  exe=root/label;subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'Apps'),*defs,str(source),'-o',str(exe)],check=True);subprocess.run([exe],check=True)
 for name in ('points_native_ui.inc','timecard_native_ui.inc'):
  body=(ROOT/'Apps'/name).read_text();assert '#include "native_scroll_labels.h"' in body;assert '0,PRODUCTIVITY_NATIVE_SCROLL_UP_LABEL,false' in body and '0,PRODUCTIVITY_NATIVE_SCROLL_DOWN_LABEL,false' in body
print('PASS current/frozen shared native scroll labels and both app consumers')

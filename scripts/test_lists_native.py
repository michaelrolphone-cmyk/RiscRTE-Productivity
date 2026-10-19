#!/usr/bin/env python3
"""Qualify shipped Lists semantics, shared renderer/input and native custody.

Capability-selected color sizes. Host evidence only; no hardware or publication.
Capture hashes retain every frame; only bounded representative PNGs are kept.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SIZES = ('160x160', '240x240', '320x180', '410x502', '502x410')
CASES = ('controls', 'keyboard', 'pending-keyboard', 'modal', 'scroll', 'checkpoint',
         'retained-before', 'retained-poll', 'retained-next', 'retained-snapshot',
         'retained-nav_poll', 'retained-acquire', 'retained-submit', 'retained-status',
         'retained-unsubscribe')
KEEP = {'home', 'list', 'task', 'keyboard', 'confirm', 'alert', 'time', 'completed'}


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def build(runtime, system, temporary, sanitized, host_source=None):
    include = load('scene_sdk_native', system / 'scripts/scene_sdk.py').stage_sdk(runtime, system, temporary)
    flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
             '-I' + str(include), '-I' + str(system / 'test/scene')]
    if sanitized:
        flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer', '-no-pie']
    cc = os.environ.get('CC', 'cc')
    source = ROOT / 'test/lists/native_render_test.c'
    probe = temporary / 'probe.o'
    subprocess.run([cc, *flags, '-DLISTS_NATIVE_HOST_PROBE',
                    '-DSCENE_HOST_SOURCE="' + str(host_source or system / 'Services/scene_host/host.c') + '"',
                    '-c', str(source), '-o', str(probe)], check=True)
    exe = temporary / 'native-render'
    subprocess.run([cc, *flags, str(source), str(ROOT / 'Apps/ListsScene.c'),
                    str(ROOT / 'lib/Lists/ListsModel.c'), str(probe), '-o', str(exe)], check=True)
    return exe


def keyboard_geometry(file):
    # Find the actual long horizontal field borders in the C-rendered raster.
    # Individual key borders are short. The field must end above the key grid.
    from PIL import Image
    im = Image.open(file).convert('RGB')
    width, height = im.size
    first_key = 84 * height // 240
    accent = (24, 226, 255)  # RGB565 0x1f1f, exact framebuffer expansion
    border_rows = []
    for y in range(first_key):
        run = longest = 0
        for x in range(width):
            run = run + 1 if im.getpixel((x, y)) == accent else 0
            longest = max(longest, run)
        if longest > width // 2:
            border_rows.append(y)
    assert border_rows, 'Keyboard field border is missing'
    groups = []
    for y in border_rows:
        if not groups or y != groups[-1][-1] + 1:
            groups.append([])
        groups[-1].append(y)
    assert len(groups) == 2, (file, 'Keyboard field bottom is obscured by keys', groups, first_key)
    field_top, field_bottom = groups[0][0], groups[-1][-1]
    assert field_bottom + 2 < first_key, (file, 'Keyboard field overlaps keys', field_bottom, first_key)
    # Blank raster rows separate the painted title/cancel, value, and key areas.
    for y in (field_top - 2, field_bottom + 2):
        assert all(im.getpixel((x, y)) == (0, 0, 0) for x in range(width)), (file, 'Title/value/key areas overlap', y)
    # The field's white text stays inside its actual accent-colored border.
    value_pixels = [(x, y) for y in range(first_key) for x in range(width)
                    if im.getpixel((x, y)) == (255, 255, 255)]
    assert value_pixels and all(field_top < y < field_bottom for x, y in value_pixels), (file, 'Value text leaves its field')



def modal_geometry(file):
    from PIL import Image
    im = Image.open(file).convert('RGB')
    width, height = im.size
    for y in range(height):
        for x in (0, 1, width - 2, width - 1):
            assert im.getpixel((x, y)) == (0, 0, 0), (file, 'Modal is clipped by a display edge', x, y)
    for x in range(width):
        for y in (0, 1, height - 2, height - 1):
            assert im.getpixel((x, y)) == (0, 0, 0), (file, 'Modal is clipped by a display edge', x, y)


def compact(path, keep):
    result = {}
    for file in sorted(path.glob('*.ppm')):
        result[file.name] = hashlib.sha256(file.read_bytes()).hexdigest()
        if keep and file.stem in KEEP:
            from PIL import Image
            Image.open(file).save(file.with_suffix('.png'))
        file.unlink()
    return result


def run(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    evidence = {'physical_testing': 'not performed', 'sources': {}, 'scenarios': [],
                'frame_sha256': {}, 'normal_sanitized_equal': False,
                'sanitizers': 'AddressSanitizer and UndefinedBehaviorSanitizer; LeakSanitizer disabled because the executor uses ptrace'}
    for name, source in [('productivity', ROOT), ('system', args.system_apps), ('runtime', args.runtime)]:
        evidence['sources'][name] = {
            'commit': subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip(),
            'status': subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True).splitlines()}
    evidence['source_sha256'] = {}
    for prefix, source, paths in [
        ('productivity', ROOT, ('test/lists/native_render_test.c', 'scripts/test_lists_native.py',
                              'Apps/ListsScene.c', 'Apps/ListsScene.h', 'lib/Lists/ListsModel.c')),
        ('system', args.system_apps, ('Services/scene_host/host.c', 'Services/scene_host/components.inc',
                                    'Services/scene_host/keyboard.inc', 'test/scene/host_test.c'))]:
        for name in paths:
            evidence['source_sha256'][prefix + '/' + name] = hashlib.sha256((source / name).read_bytes()).hexdigest()
    failures = []
    for sanitized in ((False, True) if args.sanitize else (False,)):
        flavor = 'sanitized' if sanitized else 'normal'
        with tempfile.TemporaryDirectory(prefix='lists-native-') as temporary:
            exe = build(args.runtime, args.system_apps, Path(temporary), sanitized)
            for size in args.sizes:
                for case in args.only or CASES:
                    key = f'{flavor}/{size}/{case}'
                    out = output / key
                    out.mkdir(parents=True, exist_ok=True)
                    result = subprocess.run([str(exe), size, str(out), case], text=True,
                        capture_output=True, timeout=60, env={**os.environ,
                        'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1'})
                    (out / 'run.log').write_text(result.stdout + result.stderr)
                    error = None
                    if not result.returncode and case in ('keyboard', 'modal'):
                        try:
                            if case == 'keyboard':
                                keyboard_geometry(out / 'keyboard.ppm')
                            else:
                                modal_geometry(out / 'confirm.ppm')
                                modal_geometry(out / 'alert.ppm')
                        except AssertionError as exc:
                            error = str(exc)
                    passed = not result.returncode and not error
                    evidence['scenarios'].append({'case': key, 'passed': passed, 'exit_code': result.returncode, 'error': error})
                    evidence['frame_sha256'][key] = compact(out, not sanitized)
                    print(key, 'PASS' if passed else 'FAIL', flush=True)
                    if not passed:
                        print(result.stdout + result.stderr + (error or ''), flush=True)
                        failures.append(key)
    if args.sanitize:
        for key, hashes in evidence['frame_sha256'].items():
            if key.startswith('normal/'):
                assert hashes == evidence['frame_sha256']['sanitized/' + key[7:]], ('Sanitizer frame parity', key)
        evidence['normal_sanitized_equal'] = True
    evidence['failures'] = failures
    (output / 'results.json').write_text(json.dumps(evidence, indent=2) + '\n')
    assert not failures, failures
    print(f'{len(evidence["scenarios"])} native Lists renderer cases PASS; hardware untested')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime', 'system-apps', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true', help='Run normal plus ASan/UBSan and compare capture hashes')
    parser.add_argument('--only', nargs='+', choices=CASES)
    parser.add_argument('--sizes', nargs='+', choices=SIZES, default=SIZES)
    args = parser.parse_args()
    args.runtime = args.runtime.resolve()
    args.system_apps = args.system_apps.resolve()
    run(args)

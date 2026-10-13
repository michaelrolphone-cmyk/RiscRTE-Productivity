#!/usr/bin/env python3
"""Actual Lists app_main with real Watch/X4 scene + shared text hosts.

Normal and ASan/UBSan host evidence only; no target or physical-device claims.
Local ptrace runs can set ASAN_OPTIONS=detect_leaks=0; CI keeps default LSan.
Raw frames and compiled binaries are temporary; only hashes and a few PNGs stay.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from build_lists_scene import stage
from test_lists_scene import verify_tabs

ROOT = Path(__file__).resolve().parents[1]
SELECTED = json.loads((ROOT/'sdk/lists-shared-input-sources.json').read_text())
PINS = {'system_apps': '21f640e460b4b8776f61207329f30e40adb41ec8',
        'runtime': '5561cc7bb09d1e2b03aafaecb8c95f0470f351f6',
        'x4_runtime': '5561cc7bb09d1e2b03aafaecb8c95f0470f351f6'}
COMMON = ['features', 'storage', 'storage-retry', 'reminders', 'reminder-done']
PARITY_EDITS = ['policy', 'policy-repeat', 'policy-busy', 'done', 'cancel', 'limit', 'empty', 'layers', 'save-failure', 'save-unknown']
EDITS = ['no-resident', 'back', 'header-back', 'home', 'header-home', 'native-home', 'delayed', 'frozen', 'frozen-home', 'frozen-back', 'hardware', 'stale', 'burst', 'frozen-burst']
FAULTS = ['missing', 'old-host', 'open-unavailable', 'open-busy', 'open-invalid', 'open-retained', 'open-token',
          'acquire-dirty', 'poll-again', 'poll-retained', 'poll-error', 'poll-size', 'poll-revision', 'poll-flags',
          'poll-home-state', 'poll-state', 'poll-unterminated', 'poll-control', 'close-retained', 'close-error',
          'release-fail', 'release-dirty', 'resume-fault', 'loss-acquire', 'loss-open', 'loss-poll', 'loss-close',
          'loss-release', 'loss-attention', 'loss-resume', 'loss-post-resume', 'home-close', 'home-release',
          'home-resume', 'home-post-resume', 'save-loss', 'retry', 'reopen',
          'policy-home', 'policy-exit', 'policy-poll-exit', 'policy-retained', 'policy-poll-retained',
          'policy-reply-alternate', 'policy-busy-sleep', 'policy-busy-malformed', 'policy-malformed',
          'policy-gap-touch', 'policy-gap-touch-held', 'policy-gap-cancel', 'policy-gap-home',
          'policy-gap-native-home', 'policy-gap-native-cancel', 'policy-gap-usb-cancel', 'policy-gap-usb-home',
          'policy-gap-usb', 'policy-gap-usb-held', 'policy-sleep-touch', 'policy-sleep-held',
          'policy-sleep-usb', 'policy-sleep-usb-held', 'policy-sleep-home', 'policy-sleep-exit',
          'policy-touch', 'policy-touch-queued', 'policy-usb', 'policy-usb-queued', 'policy-presenting',
          'no-checkpoint', 'old-checkpoint', 'null-focus-pause', 'checkpoint-enable-unavailable', 'checkpoint-enable-busy', 'checkpoint-enable-error',
          'checkpoint-snapshot-busy', 'checkpoint-snapshot-size', 'checkpoint-snapshot-flags', 'checkpoint-snapshot-paused',
          'checkpoint-pause-error', 'checkpoint-resume-error', 'checkpoint-pause-again', 'home-checkpoint-pause-again']


POLICY_BOUNDARIES = ['policy-reply-alternate', 'policy-busy-sleep', 'policy-busy-malformed', 'policy-malformed',
                    'policy-gap-touch', 'policy-gap-touch-held', 'policy-gap-cancel', 'policy-gap-home',
                    'policy-gap-native-home', 'policy-gap-native-cancel', 'policy-gap-usb-cancel', 'policy-gap-usb-home',
                    'policy-gap-usb', 'policy-gap-usb-held', 'policy-sleep-touch', 'policy-sleep-held',
                    'policy-sleep-usb', 'policy-sleep-usb-held', 'policy-sleep-home', 'policy-sleep-exit',
                    'policy-sleep-wake-back', 'policy-sleep-wake-home',
                    'policy-sleep-wake-native-back', 'policy-sleep-wake-usb-cancel', 'policy-request-reset', 'policy-sleep-delay', 'policy-gap-delay-50', 'policy-gap-delay-250', 'policy-no-marker', 'policy-marker-only', 'policy-poll-busy-marker',
                    'policy-gap-done', 'policy-gap-usb-done', 'policy-sleep-wake-done', 'policy-sleep-wake-usb-done', 'policy-poll-extra-marker', 'policy-poll-malformed', 'policy-marker-violation']
FAULTS = [case for case in FAULTS if case not in POLICY_BOUNDARIES]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pin(path):
    return {'commit': subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip(),
            'tree': subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD^{tree}'], text=True).strip(),
            'working_tree_status': subprocess.check_output(['git', '-C', str(path), 'status', '--porcelain'], text=True).splitlines()}


def build(runtime, system, tmp, profile, shared, sanitized, baseline=None, native_height=False):
    include = stage(runtime, system, tmp / 'sdk', shared_input=True)
    flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-I'+str(include),
             '-I'+str(system/'test/scene'), '-I'+str(system/'Services/text_input')]
    if sanitized:
        flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
    if profile == 'watch':
        flags += ['-DSCENE_HOST_TELEMETRY=0', '-DSCENE_HOST_WATCH_240='+('0' if native_height else '1')]
        if native_height: flags += ['-DLISTS_TEST_NATIVE_HEIGHT']
    else:
        generated = tmp / 'font-generation'
        font_dir = generated / 'Services/scene_host/fonts'
        font_dir.mkdir(parents=True)
        (generated/'scripts').mkdir()
        for source in (system/'Services/scene_host/fonts').glob('*.ttf'):
            shutil.copyfile(source, font_dir/source.name)
        generator = generated/'scripts/generate_telemetry_fonts.py'
        shutil.copyfile(system/'scripts/generate_telemetry_fonts.py', generator)
        subprocess.run([sys.executable, str(generator)], check=True)
        assert sha(font_dir/'telemetry.inc') == '2d7501e42d6223ebbc191817715431182c435fea944a1a02166a0748a577eaf1'
        flags += ['-I'+str(generated/'Services/scene_host')]
    cc = os.environ.get('CC', 'cc')
    sources = []
    source_root = baseline if not shared and baseline else ROOT
    if source_root != ROOT:
        flags += ['-DLISTS_APP_SOURCE="'+str(source_root/'Apps/lists_scene.c')+'"']
    if shared:
        obj = tmp/'text.o'
        subprocess.run([cc, *flags, '-Dt5_driver_get=lists_text_driver_get', '-c', str(system/'Services/text_input/host.c'), '-o', str(obj)], check=True)
        sources += [str(obj)]
        flags += ['-DPORTABLE_LISTS_SHARED_TEXT_INPUT']
    sources += [str(source_root/name) for name in ('Apps/ListsScene.c', 'lib/Lists/ListsModel.c', 'lib/Lists/ListsStore.c')]
    exe = tmp/'app'
    subprocess.run([cc, *flags, '-DSCENE_HOST_SOURCE="'+str(system/'Services/scene_host/host.c')+'"',
                    str(ROOT/'test/lists/shared_host_probe.c'), str(ROOT/'test/lists/shared_input_app_test.c'), *sources, '-o', str(exe)], check=True)
    return exe


def compact(frames, keep):
    result = {}
    for file in sorted(frames.iterdir()):
        if file.suffix not in ('.ppm', '.bin'):
            continue
        result[file.name] = sha(file)
        if file.suffix == '.ppm' and file.stem in keep:
            from PIL import Image
            Image.open(file).save(file.with_suffix('.png'))
        file.unlink()
    return result


def regression(runtime, system, tmp, sanitized):
    include = stage(runtime, system, tmp/'sdk', shared_input=False)
    flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-I'+str(include)]
    if sanitized:
        flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
    cc = os.environ.get('CC', 'cc')
    sources = [str(ROOT/name) for name in ('Apps/ListsScene.c', 'lib/Lists/ListsModel.c', 'lib/Lists/ListsStore.c')]
    for suite, extra in [('model', []), ('app', [str(ROOT/'Apps/lists_scene.c')]), ('shared_reducer', ['-DPORTABLE_LISTS_SHARED_TEXT_INPUT'])]:
        exe = tmp/suite
        subprocess.run([cc, *flags, *sources, *extra, str(ROOT/f'test/lists/{suite}_test.c'), '-o', str(exe)], check=True)
        for args in ([[]] if suite != 'app' else [['clean'], ['close-loss'], ['store-loss']]):
            subprocess.run([str(exe), *args], check=True)


def reducer_parity(runtime, system, tmp, sanitized, baseline):
    include = stage(runtime, system, tmp/'sdk', shared_input=True)
    flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-I'+str(include)]
    if sanitized:
        flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
    outputs = []
    for shared, source in [(False, baseline or ROOT), (True, ROOT)]:
        exe = tmp/('shared' if shared else 'legacy')
        subprocess.run([os.environ.get('CC', 'cc'), *flags, '-I'+str(source/'Apps'),
                        *(['-DPORTABLE_LISTS_SHARED_TEXT_INPUT'] if shared else []),
                        str(ROOT/'test/lists/review_reducer_parity.c'), str(source/'Apps/ListsScene.c'),
                        str(source/'lib/Lists/ListsModel.c'), '-o', str(exe)], check=True)
        outputs.append(subprocess.check_output([str(exe)]))
    assert outputs[0] == outputs[1], 'Reducer state parity changed'
    digest = hashlib.sha256(outputs[0]).hexdigest()
    assert digest == 'f0288169a9951405056de33188054258a3c46595ce4b3e9b320211621a2e763d'
    print('750 exact predecessor/shared reducer states match', flush=True)
    return {'states': 750, 'sha256': digest}


def summarize_input_timing(scenarios):
    groups = {}
    for case in scenarios:
        for sample in case.get('input_timing', {}).get('policy_samples', []):
            key = '/'.join((case['flavor'], case['profile'], case['implementation'],
                            'sleep' if sample['focus_changed'] else 'routine',
                            str(sample['intentional_sleep_ms']), str(sample['simulated_transport_delay_ms'])))
            group = groups.setdefault(key, {'samples': 0, 'touch_gaps_ms': [], 'usb_gaps_ms': [], 'fixture_wall_ns': []})
            group['samples'] += 1
            group['fixture_wall_ns'].append(sample['fixture_callback_wall_ns'])
            for source in ('touch', 'usb'):
                if sample[source+'_poll_resumed']:
                    gap = sample[source+'_poll_after_ms'] - sample[source+'_poll_before_ms']
                    assert gap >= sample['intentional_sleep_ms'] + sample['simulated_transport_delay_ms']
                    group[source+'_gaps_ms'].append(gap)
    for group in groups.values():
        for name in ('touch_gaps_ms', 'usb_gaps_ms', 'fixture_wall_ns'):
            values = group[name]
            group[name] = {'minimum': min(values), 'maximum': max(values)} if values else None
    return {'scope': 'Transport doubles only. Virtual poll gaps include fixture scheduling. Physical sampling pauses during synchronous policy. Callback wall time includes instrumentation and is not device latency.',
            'group_key': 'flavor/profile/implementation/routine-or-focus-change/intentional-sleep-ms/simulated-transport-delay-ms',
            'groups': groups}


def run(args):
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    paths = {name: getattr(args, name).resolve() for name in PINS}
    source_pins = {name: pin(path) for name, path in paths.items()}
    for name, expected in PINS.items():
        assert source_pins[name]['commit'] == expected, f'{name} must use {expected}'
        assert not subprocess.check_output(['git', '-C', str(paths[name]), 'status', '--porcelain', '--untracked-files=no'], text=True).strip(), f'{name} has modified tracked sources'
    baseline = args.baseline_productivity.resolve() if args.baseline_productivity else None
    if baseline:
        for name in ('lib/Lists/ListsModel.c', 'lib/Lists/ListsModel.h', 'lib/Lists/ListsStore.c', 'lib/Lists/ListsStore.h'):
            assert (ROOT/name).read_bytes() == (baseline/name).read_bytes(), name+' changed'
        source_pins['baseline_productivity'] = pin(baseline)
        assert source_pins['baseline_productivity']['commit'] == SELECTED['source_baseline'], 'Incorrect predecessor commit'
        assert not subprocess.check_output(['git', '-C', str(baseline), 'status', '--porcelain', '--untracked-files=no'], text=True).strip(), 'Predecessor has modified tracked sources'
    source_pins['productivity'] = pin(ROOT)
    names = ['Apps/lists_scene.c', 'Apps/ListsScene.c', 'Apps/ListsScene.h', 'Apps/lists_text_input.inc',
             'scripts/build_lists_scene.py', 'scripts/test_lists_shared_input.py', 'test/lists/shared_input_app_test.c', 'test/lists/shared_input_fixture.h',
             'test/lists/shared_input_fixture.inc', 'test/lists/shared_host_probe.c', 'test/lists/shared_reducer_test.c', 'test/lists/review_reducer_parity.c', '.github/workflows/lists-shared-input.yml', 'sdk/lists-shared-input-sources.json', 'sdk/lists-selected-profile/RiscSceneResidentV1.h']
    evidence = {'physical_testing': 'not performed',
                'runtime_transport': 'Host transport double. Resident-shell cases exercise the optional supported owner contract; no-resident cases exercise its absence, as on the selected Watch boot.', 'sources': source_pins,
                'source_sha256': {name: sha(ROOT/name) for name in names},
                'sanitizer_options': os.environ.get('ASAN_OPTIONS', 'default'), 'scenarios': [], 'parity': {},
                'system_source_sha256': {name: sha(paths['system_apps']/name) for name in (
                    'Services/text_input/host.c', 'Services/text_input/checkpoint.inc', 'Services/scene_host/host.c',
                    'sdk/app/RiscTextEntryV1.h', 'sdk/app/RiscSceneCheckpointV1.h', 'sdk/app/RiscSceneLifecycleV1.h',
                    'lib/PortableApps/include/PortableTextInputClient.h', 'lib/PortableApps/include/PortableTextInputHandoff.h',
                    'lib/PortableApps/include/PortableTextInputCheckpointClient.h',
                    'lib/PortableApps/include/PortableTextInputResidentCheckpoint.h',
                    'lib/PortableApps/include/PortableResidentPolicyInput.h')}}
    common = COMMON + [f'edit-{reason}-{mode}' for reason in range(4) for mode in PARITY_EDITS]
    for sanitized in ([False, True] if args.sanitize else [False]):
        flavor = 'sanitized' if sanitized else 'normal'
        if not args.only:
            with tempfile.TemporaryDirectory(prefix='lists-existing-') as directory:
                regression(paths['runtime'], paths['system_apps'], Path(directory), sanitized)
                evidence['parity'][flavor+'/reducer'] = reducer_parity(paths['runtime'], paths['system_apps'], Path(directory), sanitized, baseline)
            evidence['scenarios'].append({'flavor': flavor, 'suite': 'existing model/app lifecycle', 'result': 'PASS'})
        for profile in args.profiles:
            parity = {}
            for shared in ([True] if args.only else [False, True]):
                kind = 'shared' if shared else 'legacy'
                with tempfile.TemporaryDirectory(prefix='lists-shared-') as directory:
                    tmp = Path(directory)
                    runtime = paths['x4_runtime' if profile == 'paper' else 'runtime']
                    exe = build(runtime, paths['system_apps'], tmp, profile, shared, sanitized, baseline, args.native_height)
                    frames = out/flavor/profile/kind
                    frames.mkdir(parents=True, exist_ok=True)
                    cases = list(common)
                    if shared:
                        cases += [f'edit-{reason}-{mode}' for reason in range(4) for mode in EDITS]
                        cases += [f'edit-2-{mode}' for mode in FAULTS]
                        cases += [f'edit-{reason}-{mode}' for reason in range(4) for mode in POLICY_BOUNDARIES]
                        cases += [f'edit-{reason}-{mode}' for reason in range(4)
                                  for site in ('checkpoint-enable', 'checkpoint-snapshot', 'checkpoint-pause', 'checkpoint-resume')
                                  for base_mode in ('loss-'+site, site+'-retained', 'home-'+site)
                                  for mode in (base_mode, base_mode+'-sleep')]
                        cases += [f'edit-{reason}-{mode}' for reason in range(4)
                                  for site in ('scene-snapshot', 'scene-close', 'scene-open', 'scene-configure')
                                  for mode in ('loss-'+site, site+'-retained')]
                    if args.only:
                        cases = args.only
                    for case in cases:
                        result = subprocess.run([str(exe), profile, str(frames), case], timeout=30, text=True, capture_output=True)
                        if result.returncode:
                            print(result.stdout, end='', file=sys.stderr)
                            print(result.stderr, end='', file=sys.stderr)
                            result.check_returncode()
                        print(result.stdout, end='', flush=True)
                        evidence['scenarios'].append({'flavor': flavor, 'profile': profile, 'implementation': kind, 'case': case, 'result': 'PASS'})
                        timing = frames/(profile+'-'+case+'.timing.json')
                        if timing.exists():
                            evidence['scenarios'][-1]['input_timing'] = json.loads(timing.read_text())
                            timing.unlink()
                    if not args.only:
                        verify_tabs(frames/(profile+'-list.ppm'), 2 if profile == 'paper' else 1, 0)
                        verify_tabs(frames/(profile+'-list-all.ppm'), 2 if profile == 'paper' else 1, 1)
                    keep = {profile+'-'+name for name in ('home', 'task', 'keyboard', 'unavailable-text')} if not sanitized else set()
                    parity[kind] = compact(frames, keep)
            if not args.only:
                for file, digest in parity['legacy'].items():
                    assert parity['shared'].get(file) == digest, f'Non-keyboard frame/storage parity failure: {flavor}/{profile}/{file}'
                evidence['parity'][flavor+'/'+profile] = {'matched': list(parity['legacy']), 'sha256': parity['legacy']}
                print(f'{flavor}/{profile}: {len(parity["legacy"])} exact frame/storage hashes match', flush=True)
    for name, digest in evidence['source_sha256'].items():
        assert sha(ROOT/name) == digest, f'Productivity source changed during test run: {name}'
    for name, digest in evidence['system_source_sha256'].items():
        assert sha(paths['system_apps']/name) == digest, f'System source changed during test run: {name}'
    evidence['input_timing_summary'] = summarize_input_timing(evidence['scenarios'])
    (out/'results.json').write_text(json.dumps(evidence, indent=2)+'\n')
    print(f'{len(evidence["scenarios"])} Lists scenarios PASS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime', 'x4-runtime', 'system-apps', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--baseline-productivity', type=Path, help='Exact predecessor checkout for flag-off comparisons')
    parser.add_argument('--native-height', action='store_true', help='Qualify Watch workflows at 410x502 with the native-size scene profile')
    parser.add_argument('--sanitize', action='store_true', help='Run both normal and ASan/UBSan suites')
    parser.add_argument('--only', nargs='+', help='Selected shared cases for development')
    parser.add_argument('--profiles', nargs='+', choices=('watch', 'paper'), default=['watch', 'paper'])
    run(parser.parse_args())

#!/usr/bin/env python3
"""Compact Timecard controller, production text-host/adapter and parity qualification.

Provider boundaries are deterministic fixtures. No target execution, hardware,
full image creation or publication is performed. Leak detection is left at the
platform default unless --no-leak-checks is explicitly selected (local ptrace).
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PINS = {
    'system': '21f640e460b4b8776f61207329f30e40adb41ec8',
    'utilities': 'fc0bdbbbc600ce891e60c44d091fce0586ade7d8',
    'runtime': '5561cc7bb09d1e2b03aafaecb8c95f0470f351f6',
}
SHARED = ['-DPORTABLE_TIMECARD_SHARED_TEXT_INPUT', '-DPORTABLE_TEXT_INPUT_CLIENT',
          '-DPORTABLE_CHECKED_STORAGE_RUNTIME', '-DPORTABLE_APP_HOME_GUARD']
CONTROLLER_CASES = ['fields', 'validation', 'invalid-cookie', 'input-fence', 'cancel', 'reload', 'storage-retry',
                    'pending', 'poll-again', 'home', 'unavailable', 'busy',
                    'malformed', 'poll-failure', 'close-retained', 'release-failure',
                    'resume-failure', 'storage-preservation']
WATCH_FAILURE_CASES = ['stat', 'read', 'replace', 'rtc', 'rtc.clock',
                       'storage.key-value', 'storage.app-data', 'release',
                       'background-contexts-true', 'background-contexts-false',
                       'background-broadcast-true', 'background-broadcast-false',
                       'prefs-context', 'prefs-unknown', 'prefs-busy']
WATCH_HOST_CASES = ['fields', 'cancel', 'home', 'invalid', 'limit', 'busy',
                    'unavailable', 'retained', 'alarm', 'raw-touch', 'parity']
HOST_CASES = ['fields', 'retry-frame', 'busy-input', 'stale-burst', 'limit', 'hid', 'home',
              'cancel', 'unavailable', 'poll-retained', 'close-retained',
              'release-retained', 'entrypoint', 'entrypoint-home']
NATIVE_CASES = [str(i) for i in (0, 1, 2, 4, 5, 6, 7, *range(11, 23),
                                *range(26, 32), *range(33, 37))]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--system-apps', type=Path, required=True)
    p.add_argument('--utilities', type=Path, required=True)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--output', type=Path, default=ROOT / 'build/timecard-shared-input')
    p.add_argument('--no-leak-checks', action='store_true', help='Local executor ptrace compatibility only')
    p.add_argument('--normal-only', action='store_true')
    a = p.parse_args()
    s, u, r = (x.resolve() for x in (a.system_apps, a.utilities, a.runtime))
    for name, path in [('system', s), ('utilities', u), ('runtime', r)]:
        actual = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
        if subprocess.check_output(['git', '-C', str(path), 'status', '--porcelain', '--untracked-files=no'], text=True).strip():
            raise ValueError(f'{name} dependency has modified tracked sources')
        if actual != PINS[name]:
            raise ValueError(f'{name} must use exact published input {PINS[name]}, got {actual}')
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    common_includes = [s/'lib/PortableApps/include', s/'lib/NativeApps/include',
                       s/'lib/PortableApps/src', s/'Apps', s/'sdk/app',
                       u/'lib/Alarm/include', u/'lib/Contexts/include', u/'Apps', u/'test/native_apps', r/'sdk/app',
                       ROOT/'lib/NativeApps/include', ROOT/'lib/PortableTimecard/include',
                       s/'Services/text_input']
    includes = ['-I'+str(x) for x in common_includes]
    watch_defines = ['-DPORTABLE_NOVA_UI', '-DPORTABLE_APP_OWNS_TOUCH_CHROME']
    x4_defines = json.loads((ROOT/'test/native_apps/timecard_x4_product_defines.json').read_text())
    watch_product_defines = json.loads((ROOT/'test/native_apps/timecard_watch_product_defines.json').read_text())
    watch_product_defines += ['-DTIMECARD_TEST_ALARM_HEADER=\"'+str(u/'lib/Alarm/include/AlarmServiceV1.h')+'\"',
                              '-Wl,--wrap=t5_app_get_api', '-include',
                              str(ROOT/'test/native_apps/timecard_watch_alarm_profile.h')]
    watch_helpers = [s/'lib/PortableApps/src'/name for name in
                     ('adapter.c', 'quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c')]
    test_defines = ['-DTEST_RESIDENT_CLIENT', '-DTEST_PRODUCTIVITY_SCROLL_FIXTURE',
                    '-DTEST_TIMECARD_SETTLE_DRAWS']
    helpers = [s/'lib/PortableApps/src'/name for name in
               ('adapter.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c',
                'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c',
                'PortableNativeTimeSource.c')]
    runs, commands, parity = [], [], []
    (out/'frames').mkdir(exist_ok=True)
    env = {**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1', 'TIMECARD_RETRY_FRAMES': str(out/'frames')}
    if a.no_leak_checks:
        env['ASAN_OPTIONS'] = 'detect_leaks=0'

    def compile_binary(name, fixture, defines, extra_sources, sanitized):
        binary = out/(name+('-san' if sanitized else '-normal'))
        flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                 '-Wno-unused-function', '-Wno-misleading-indentation']
        if sanitized:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                      '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        cmd = [os.environ.get('CC', 'cc'), *flags, *defines, *includes,
               str(ROOT/'test/native_apps'/fixture), *map(str, extra_sources), '-o', str(binary)]
        subprocess.run(cmd, check=True, timeout=180)
        commands.append({'name': name, 'sanitized': sanitized, 'command': cmd})
        return binary

    for sanitized in (False,) if a.normal_only else (False, True):
        for name, fixture, cases, defines, sources in [
            ('controller', 'timecard_shared_input_test.c', CONTROLLER_CASES,
             [*watch_defines, *SHARED], []),
            ('watch-failure', 'timecard_shared_watch_failure_test.c', WATCH_FAILURE_CASES,
             [*watch_defines, *SHARED], []),
            ('watch-host', 'timecard_shared_watch_host_test.c', WATCH_HOST_CASES,
             watch_product_defines, [*watch_helpers, s/'Services/text_input/host.c']),
            ('native-host', 'timecard_shared_host_test.c', [*HOST_CASES, *NATIVE_CASES],
             [*x4_defines, *SHARED, *test_defines], [*helpers, s/'Services/text_input/host.c']),
        ]:
            binary = compile_binary(name, fixture, defines, sources, sanitized)
            for case in cases:
                result = subprocess.run([str(binary), case], check=True, capture_output=True,
                                        text=True, timeout=30, env=env)
                print(result.stdout, end='', flush=True)
                runs.append({'suite': name, 'case': case, 'sanitized': sanitized,
                             'passed': True, 'stdout': result.stdout.strip()})
        adapter_parity = []
        for enabled in (False, True):
            selected = [f for f in watch_product_defines
                        if enabled or f != '-DPORTABLE_TIMECARD_SHARED_TEXT_INPUT']
            binary = compile_binary(f'watch-adapter-parity-{int(enabled)}',
                                    'timecard_shared_watch_host_test.c', selected,
                                    [*watch_helpers, s/'Services/text_input/host.c'], sanitized)
            result = subprocess.run([str(binary), 'parity'], check=True, capture_output=True,
                                    text=True, timeout=30, env=env)
            adapter_parity.append(result.stdout)
        if adapter_parity[0] != adapter_parity[1]:
            raise AssertionError(f'Watch production adapter pixel/ledger parity changed: {adapter_parity}')
        parity.append({'device': 'watch-production-adapter', 'sanitized': sanitized,
                       'identical': True, 'actual_submitted_frames': adapter_parity[0].splitlines()})
        print('Watch complete adapter submitted RGB565 frame/ledger parity PASS', flush=True)
        for device, defines, sources in [('watch', watch_defines, []),
                                         ('x4', [*x4_defines, *test_defines], helpers)]:
            outputs = []
            for enabled in (False, True):
                binary = compile_binary(f'parity-{device}-{int(enabled)}',
                                        'timecard_nonkeyboard_parity_test.c',
                                        [*defines, *(SHARED if enabled else [])], sources, sanitized)
                result = subprocess.run([str(binary)], check=True, capture_output=True,
                                        text=True, timeout=30, env=env)
                outputs.append(result.stdout)
            if outputs[0] != outputs[1]:
                raise AssertionError(f'{device} non-keyboard render/storage parity changed: {outputs}')
            parity.append({'device': device, 'sanitized': sanitized, 'identical': True,
                           'frames_and_ledger': outputs[0].splitlines()})
            print(f'{device} seven non-keyboard frames and ledger byte parity PASS', flush=True)
    paths = [ROOT/'scripts/test_timecard_shared_input.py',
             *ROOT.glob('test/native_apps/timecard_shared*'),
             ROOT/'test/native_apps/timecard_nonkeyboard_parity_test.c',
             ROOT/'test/native_apps/timecard_x4_product_defines.json',
             ROOT/'test/native_apps/timecard_watch_product_defines.json',
             ROOT/'test/native_apps/timecard_watch_alarm_profile.h',
             ROOT/'test/native_apps/timecard_adapter_fixture.h',
             ROOT/'test/native_apps/timecard_portable_test.c',
             ROOT/'test/native_apps/timecard_native_time_test.c',
             *ROOT.glob('Apps/timecard*')]
    report = {'schema': 1, 'dependencies': PINS, 'runs': runs, 'parity': parity,
              'commands': commands, 'source_sha256': {str(x.relative_to(ROOT)): sha(x) for x in paths if x.is_file()},
              'x4_product_defines': x4_defines, 'watch_product_defines': watch_product_defines, 'shared_defines': SHARED,
              'asan': not a.normal_only, 'ubsan': not a.normal_only,
              'leak_detection': 'disabled explicitly for local ptrace' if a.no_leak_checks else 'platform default',
              'limits': ['Host machine execution only; target Xtensa instructions and physical hardware not executed.',
                         'Actual Timecard, PortableTextInputClient, X4 adapter and text host; Runtime, scene presentation, HID and resident dispatch boundaries are fixtures.',
                         'Watch production feature flags and adapter are exercised; board-specific sleep/navigation callbacks are fixtures, Contexts service is cleanly unavailable, and test host scene presentation is deterministic.',
                         'UI parity compares migration on/off on the selected System source, not historical System renderer versions.'],
              'retry_frame_sha256': {x.name: sha(x) for x in (out/'frames').glob('*.ppm')},
              'hardware_verified': False}
    (out/'test-evidence.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'{len(runs)} workflow/regression runs and {len(parity)} paired render/storage parity checks PASS')


if __name__ == '__main__':
    main()

"""Opt-in Quick Controls motion; frozen native runtime/alarm authority stays put."""
import argparse
import importlib.util
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PIN = json.loads((ROOT / 'sdk/paper-transitions-sources.json').read_text())
SCROLL_PIN = json.loads((ROOT / 'sdk/native-touch-scroll-sources.json').read_text())
BROADCAST_PIN = json.loads((ROOT / 'sdk/native-broadcast-sources.json').read_text())
IDLE_PIN = json.loads((ROOT / 'sdk/native-idle-sources.json').read_text())


def options(parser):
    for name in ("source", "sdk", "runtime-sdk"):
        parser.add_argument("--x4-idle-" + name, type=Path)
    parser.add_argument('--touch-scrolling', action='store_true', help='Explicit native paper list scrolling; defaults OFF')
    parser.add_argument('--ble-broadcast', action='store_true', help='Explicit native telemetry client; requires paper transitions, defaults OFF')
    parser.add_argument('--paper-transitions', action='store_true',
                        help='Opt-in X4 paper Quick Controls pull-down motion')
    parser.add_argument('--motion-system', type=Path,
                        help='Clean exact System motion checkout; required only with --paper-transitions')


def select(args, parser, adapter, defines, manifest=None):
    if bool(args.paper_transitions) != bool(args.motion_system):
        parser.error('--paper-transitions and --motion-system require each other')
    idle = any(getattr(args, 'x4_idle_' + n, None) for n in ('source','sdk','runtime_sdk'))
    if idle and (not args.paper_transitions or not all(getattr(args, 'x4_idle_' + n, None) for n in ('source','sdk','runtime_sdk'))):
        parser.error('X4 idle requires paper transitions and all three explicit helper/SDK inputs')
    scrolling = getattr(args, 'touch_scrolling', False)
    if scrolling and not args.paper_transitions:
        parser.error('--touch-scrolling requires --paper-transitions and --motion-system')
    broadcast = getattr(args, 'ble_broadcast', False)
    if broadcast and not args.paper_transitions:
        parser.error('--ble-broadcast requires --paper-transitions and --motion-system')
    if not args.paper_transitions:
        return adapter, list(defines), manifest, None
    pin = IDLE_PIN if idle else SCROLL_PIN if scrolling else BROADCAST_PIN if broadcast else PIN
    source = args.motion_system.resolve()
    def git(*arguments):
        return subprocess.check_output(['git', '-C', str(source), *arguments], text=True).strip()
    if git('rev-parse', 'HEAD') != pin['adapter_sha'] or git('status', '--porcelain', '--untracked-files=all'):
        raise ValueError('Clean exact motion dependency required: ' + pin['adapter_sha'])
    for name, digest in pin['source_sha256'].items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != digest:
            raise ValueError('Frozen motion input changed: ' + name)
    if manifest is not None:
        manifest = {**manifest, 'version': pin['versions'][manifest['id']]}
    receipt = {'enabled': True, 'build_define': pin['define'],
               'source_revision': pin['adapter_sha'], 'source_public': pin['adapter_public'],
               'source_directory': str(source), 'source_sha256': pin['source_sha256'],
               'scope': 'paper Quick Controls and native BLE telemetry' if broadcast else 'paper Quick Controls only', 'hardware_verified': False}
    flags = [*defines, pin['define']]
    if scrolling:
        flags += ['-DPORTABLE_TOUCH_SCROLL', '-DPORTABLE_APP_TOUCH_SCROLL', '-DPORTABLE_PRODUCTIVITY_SCROLL', '-DPORTABLE_PAPER_PREFERENCES']
        receipt['scope'] += ' and paper list scrolling'
        receipt['touch_scrolling'] = {'enabled':True,'scope':'Points and Timecard paper lists','frame_policy':'completed-frame hit identity'}
    if broadcast:
        flags += ['-DPORTABLE_BLE_BROADCAST', '-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF', '-DPORTABLE_PAPER_PREFERENCES']
        if manifest is not None:
            manifest = {**manifest, 'requires': [*manifest['requires'], {'capability':'telemetry.broadcast','api':1}]}
        receipt['ble_broadcast'] = {'enabled':True,'default':'off','grant_lifetime':'transient'}
    return source, list(dict.fromkeys(flags)), manifest, receipt


def configure_idle(args, parser, adapter, output, includes, defines, manifest):
    """Use the selected System policy and typed SDK; ordinary builds stay identical."""
    if not getattr(args, 'x4_idle_source', None):
        return list(includes), list(defines), [], None
    path = adapter / 'scripts/portable_idle_build.py'
    spec = importlib.util.spec_from_file_location('selected_productivity_idle', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    selection = argparse.Namespace(**vars(args))
    selection.quick_actions = selection.quick_radios = selection.alarm_client = True
    selection.tagged_alarm_utilities = True
    selection.time_profile = 'x4-native-time'
    flags, sources = module.configure(selection, parser, adapter, output, includes[0])
    module.requirements(selection, manifest['requires'])
    for name in ('board.battery', 'net.wifi', 'bluetooth.hci'):
        requirement = {'capability': name, 'api': 1}
        if requirement not in manifest['requires']:
            manifest['requires'].append(requirement)
    if len(manifest['requires']) > 16:
        raise ValueError('Native idle app exceeds declared capability limit')
    final_includes = [Path(selection.x4_idle_receipt['compiled_include_directory']), *includes]
    final_flags = list(dict.fromkeys([*defines, *flags, '-DPORTABLE_QUICK_RADIOS']))
    return final_includes, final_flags, [*sources, adapter / 'lib/PortableApps/src/quick_radios.c'], selection.x4_idle_receipt

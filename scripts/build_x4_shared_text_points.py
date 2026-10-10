#!/usr/bin/env python3
"""Build the reserved X4 resident Points 0.6.12 from the frozen .50 profile."""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.6.12'
SYSTEM = 'c336b776c869029ecd9f096aa5e6497759581440'
RUNTIME = '615fb236b591bc6974a35ae23c7b2b785c0a5016'
UTILITIES = 'bda1c2ec01d2c18e39f822fbcf6cc8ac3b12aa9e'
BASELINE_HELPER = '1f7209b1f74d8f706b7b5f73be9aaf52061ce5679bbb99dea79f8aa7bd0779de'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--utilities', type=Path, required=True)
    preliminary, _ = p.parse_known_args()
    sys.path.insert(0, str(preliminary.utilities.resolve() / 'scripts'))
    import resident_client_build as resident
    resident.options(p)
    p.add_argument('--baseline-custody', type=Path, required=True)
    p.add_argument('--baseline-target', type=Path, required=True)
    p.add_argument('--reservation', type=Path, required=True)
    a = p.parse_args()
    utilities = a.utilities.resolve()
    resident.exact(utilities, UTILITIES)
    if resident.sha(utilities / 'scripts/resident_client_build.py') != BASELINE_HELPER:
        p.error('The frozen installed resident build helper changed')
    if a.system_revision != SYSTEM:
        p.error('The selected shared-text System source is required')
    # Explicitly select the canonical .100 Runtime SDK. No dependency source is
    # edited and the historical helper's old default remains unchanged.
    resident.RUNTIME = RUNTIME
    baseline = json.loads(a.baseline_custody.read_text())['build_receipts']['points_in_time']
    installed = json.loads((a.baseline_target / 'x4-native-app.json').read_text())
    if baseline != installed or baseline['version'] != '0.6.11':
        p.error('Frozen .50 custody and original Points build receipt differ')
    if resident.sha(a.baseline_target / 'points_in_time.elf') != baseline['elf_sha256']:
        p.error('Frozen Points target bytes differ')
    reservation = json.loads(a.reservation.read_text())
    if reservation.get('Points') != VERSION:
        p.error('The live local version reservation does not name Points 0.6.12')
    c = resident.prepare(a, p, utilities)
    defines = baseline['build_defines'] + ['-DPORTABLE_TEXT_INPUT_CLIENT']
    grants = baseline['required_grants'] + [dict(capability='ui.text-input', api=1, instance_id=0)]
    features = dict(baseline['features'], shared_text_input='ui.text-input@1 instance0; no local fallback')
    record = resident.build(c, ROOT, 'points_in_time', VERSION, defines,
                            [ROOT / 'Apps/points_catalog_app.c'], grants, features)
    selected = json.loads((ROOT / 'Apps/native/points_catalog_x4_resident.json').read_text())
    manifest = json.loads((c['out'] / 'points_in_time/points_in_time.json').read_text())
    if selected != manifest:
        raise ValueError('Selected source manifest does not match the target identity/profile')
    if record['build_defines'] != defines or record['required_grants'] != grants:
        raise ValueError('The complete frozen flag/grant profile was not preserved')
    for field in ('requires', 'required_grants'):
        entries = [e for e in record[field] if e['capability'] == 'ui.text-input']
        expected = dict(capability='ui.text-input', api=1)
        if field == 'required_grants':
            expected['instance_id'] = 0
        if entries != [expected]:
            raise ValueError('Exactly one native ui.text-input@1 declaration/grant is required')
    record['profile_custody'] = dict(
        baseline_version='0.6.11', baseline_elf_sha256=baseline['elf_sha256'],
        baseline_custody_sha256=resident.sha(a.baseline_custody),
        baseline_receipt_sha256=resident.sha(a.baseline_target / 'x4-native-app.json'),
        added_defines=['-DPORTABLE_TEXT_INPUT_CLIENT'],
        added_grants=[dict(capability='ui.text-input', api=1, instance_id=0)],
        removed_defines=[], removed_grants=[],
        native_time=True, tagged_alarm_api=2, telemetry_default='off',
        resident_policy=True, local_keyboard_fallback=False)
    record['selected_manifest_sha256'] = resident.sha(ROOT / 'Apps/native/points_catalog_x4_resident.json')
    record['selected_build_helper_sha256'] = resident.sha(Path(__file__))
    record['reservation_sha256'] = resident.sha(a.reservation)
    stacks = []
    for path in (c['out'] / 'points_in_time').glob('*.su'):
        for line in path.read_text().splitlines():
            fields = line.split('\t')
            if len(fields) >= 3:
                stacks.append(dict(function=fields[0], bytes=int(fields[1]), kind=fields[2]))
    stacks.sort(key=lambda row: row['bytes'], reverse=True)
    if not stacks or any(row['bytes'] >= 16384 for row in stacks):
        raise ValueError('Missing stack evidence or individual frame exceeds selected app stack')
    record['stack_frames'] = stacks
    resident.write(c['out'] / 'points_in_time/x4-native-app.json', record)
    resident.write(c['out'] / 'cohort-receipt.json', dict(
        schema=1, role='foreground', quick_render_total=0, built={'points_in_time': record},
        hardware_verified=False, installable=False))


if __name__ == '__main__':
    main()

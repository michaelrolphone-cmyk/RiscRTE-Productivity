#!/usr/bin/env python3
"""Replace only Points in three pinned products; preserve all unrelated bytes."""
import argparse
import copy
import hashlib
import json
import sys
import zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
LANES = {'watch': 1, 'wave': 1, 'x4': 2}
REMOVABLE = {'input.touch.raw', 'input.navigation', 'input.power-key', 'power.sleep',
             'display.power', 'motion.accel', 'bluetooth.hci', 'net.wifi'}
MANIFEST = 'points_in_time.json'
ELF = 'points_in_time.elf'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def encode(value):
    return (json.dumps(value, sort_keys=True, indent=2) + '\n').encode()


def safe(name):
    if not isinstance(name, str) or not name or name == '.':
        raise ValueError('Invalid path')
    path = PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts or '\\' in name or path.as_posix() != name:
        raise ValueError('Unsafe or noncanonical path ' + name)
    return name


def write_tree(root, entries):
    for name, data in entries.items():
        path = root / safe(name)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def declarations(manifest, field):
    rows = manifest.get(field, [])
    if not isinstance(rows, list):
        raise ValueError('Invalid declarations ' + field)
    result = set()
    for row in rows:
        if (not isinstance(row, dict) or set(row) != {'capability', 'api'} or
                not isinstance(row['capability'], str) or not row['capability'] or
                type(row['api']) is not int or row['api'] <= 0):
            raise ValueError('Invalid capability declaration')
        key = (row['capability'], row['api'])
        if key in result:
            raise ValueError('Duplicate capability declaration')
        result.add(key)
    return result


def validate_manifest(manifest, alarm_api, *, source=True):
    expected = {'id': 'points_in_time', 'file_name': ELF, 'type': 'application',
                'entry': 'app_main', 'architecture': 'xtensa-esp32s3'}
    if source:
        expected['runtime_profile'] = 'shared-scene-v1'
    if any(manifest.get(key) != value for key, value in expected.items()):
        raise ValueError('Wrong application manifest')
    if not isinstance(manifest.get('version'), str) or not manifest['version']:
        raise ValueError('Missing application version')
    required = declarations(manifest, 'requires')
    optional = declarations(manifest, 'optional')
    if required & optional:
        raise ValueError('Overlapping capability declarations')
    declared = required | optional
    if ('ui.scene', 1) not in required or any(cap in REMOVABLE for cap, _ in declared):
        raise ValueError('Wrong scene/physical declarations')
    if {(cap, api) for cap, api in declared if cap == 'alarm.service'} != {('alarm.service', alarm_api)}:
        raise ValueError('Alarm declaration mismatch')
    return required, declared


def validate_selection(lane, raw, selection, app_hash, canonical):
    if (selection.get('manifest_sha256') != digest(raw) or selection.get('elf_sha256') != app_hash or
            selection.get('alarm_api') != LANES[lane]):
        raise ValueError('Selection differs')
    manifest = json.loads(raw)
    validate_manifest(manifest, LANES[lane])
    expected = copy.deepcopy(canonical)
    for field in ('requires', 'optional'):
        for row in expected.get(field, []):
            if row['capability'] == 'alarm.service':
                row['api'] = LANES[lane]
    if manifest != expected:
        raise ValueError('Selection differs from canonical application')
    return manifest


def validate_application(application):
    app = (application / ELF).read_bytes()
    record = json.loads((application / 'build.json').read_text())
    raw = (application / MANIFEST).read_bytes()
    canonical = json.loads(raw)
    validate_manifest(canonical, 1)
    if (not app.startswith(b'\x7fELF') or record.get('sha256') != digest(app) or
            record.get('size_bytes') != len(app) or record.get('repeat_identical') is not True or
            record.get('manifest_sha256') != digest(raw) or
            (application / 'points_in_time.repeat.elf').read_bytes() != app):
        raise ValueError('Application build differs')
    if set(record.get('product_selections', {})) != set(LANES):
        raise ValueError('Product selections differ')
    manifests = {}
    for lane in LANES:
        raw = (application / 'selections' / lane / MANIFEST).read_bytes()
        manifests[lane] = validate_selection(lane, raw, record['product_selections'][lane], digest(app), canonical)
    return app, record, manifests


def read_archive(path):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError('Duplicate ZIP entries')
        for name in names:
            safe(name[:-1] if name.endswith('/') else name)
        return {name: archive.read(name) for name in names if not name.endswith('/')}


def verify_baseline(directory, lock):
    expected = {lane + suffix for lane in LANES for suffix in ('-store.bin', '-sd-apps.zip')}
    rows = lock['assets']
    if len(rows) != len(expected) or {row['name'] for row in rows} != expected:
        raise ValueError('Baseline asset set differs')
    for row in rows:
        data = (directory / safe(row['name'])).read_bytes()
        if len(data) != row['size'] or digest(data) != row['sha256']:
            raise ValueError('Baseline differs ' + row['name'])


def route(entries, row):
    manifest = json.loads(entries[safe(row['manifest'])])
    if 'image_source' in row:
        source = row['image_source']
        if (not isinstance(source, dict) or type(source.get('volume_instance')) is not int or
                source['volume_instance'] <= 0):
            raise ValueError('Invalid image source')
        return 'sd', safe(source['path'])
    return 'internal', safe(str(PurePosixPath(row['manifest']).parent / manifest['file_name']))


def verify_preservation(before, sd0, out, sd, original, row, changed, sd_changed, sd_removed, sd_added):
    old_boot = json.loads(before['boot.json'])
    boot = json.loads(out['boot.json'])
    restored = copy.deepcopy(boot)
    rows = [r for r in restored['app_capabilities'] if r['manifest'] == MANIFEST]
    if rows != [row]:
        raise ValueError('Points authority ambiguous after assembly')
    restored['app_capabilities'][restored['app_capabilities'].index(row)] = original
    if restored != old_boot:
        raise ValueError('Unrelated boot authority changed')
    old_other = {k: v for k, v in original.items() if k not in ('grants', 'image_source')}
    new_other = {k: v for k, v in row.items() if k not in ('grants', 'image_source')}
    old_source = copy.deepcopy(original.get('image_source', {}))
    new_source = copy.deepcopy(row.get('image_source', {}))
    old_source.pop('path', None)
    new_source.pop('path', None)
    if old_other != new_other or old_source != new_source:
        raise ValueError('Unrelated Points authority changed')
    storage = lambda r: [g for g in r['grants'] if g['capability'].startswith('storage.')]
    if storage(original) != storage(row):
        raise ValueError('Storage namespace or authority changed')
    if set(out) != set(before) or set(sd) != (set(sd0) - sd_removed) | sd_added:
        raise ValueError('Unrelated file set changed')
    if (any(out[n] != b for n, b in before.items() if n not in changed) or
            any(sd[n] != b for n, b in sd0.items() if n not in sd_changed | sd_removed)):
        raise ValueError('Unrelated bytes changed')


def assemble_lane(lane, before, sd0, manifest, app):
    required, declared = validate_manifest(manifest, LANES[lane])
    out, sd = dict(before), dict(sd0)
    boot = json.loads(before['boot.json'])
    # Identity aliases are ambiguous even when their manifest paths differ.
    rows = [r for r in boot['app_capabilities']
            if json.loads(before[safe(r['manifest'])]).get('id') == 'points_in_time']
    if len(rows) != 1 or rows[0]['manifest'] != MANIFEST:
        raise ValueError('Points authority ambiguous')
    original = copy.deepcopy(rows[0])
    row = copy.deepcopy(original)
    installed = json.loads(before[MANIFEST])
    if installed.get('type') != 'application' or installed.get('file_name') != ELF:
        raise ValueError('Points identity differs')
    old_route = route(before, original)
    for other in boot['drivers'] + boot['app_capabilities']:
        if other is rows[0]:
            continue
        if route(before, other) == old_route:
            raise ValueError('Points executable route is shared')
        other_manifest = json.loads(before[other['manifest']])
        internal = ('internal', safe(str(PurePosixPath(other['manifest']).parent / other_manifest['file_name'])))
        if internal == old_route:
            raise ValueError('Points executable overlaps an internal fallback')
    removed = [g for g in row['grants'] if (g['capability'], g['api']) not in declared]
    if any(g['capability'] not in REMOVABLE for g in removed):
        raise ValueError('Unreviewed grant removal ' + repr(removed))
    row['grants'] = [g for g in row['grants'] if g not in removed]
    alarms = [g for g in row['grants'] if g['capability'] == 'alarm.service']
    if len(alarms) != 1 or alarms[0]['api'] != LANES[lane]:
        raise ValueError('Alarm authority mismatch')
    owners = [d for d in boot['drivers']
              if ('ui.scene', 1) in declarations(json.loads(before[d['manifest']]), 'provides')]
    if len(owners) != 1 or owners[0].get('instance_id', 0) != 0:
        raise ValueError('Scene provider missing or ambiguous')
    if ('time.civil', 1) in declared:
        civil = [d for d in boot['drivers']
                 if ('time.civil', 1) in declarations(json.loads(before[d['manifest']]), 'provides')]
        if len(civil) != 1 or civil[0].get('instance_id', 0) != 0:
            raise ValueError('Civil time provider missing or ambiguous')
        grant = {'capability': 'time.civil', 'api': 1, 'instance_id': 0}
        if grant not in row['grants']:
            row['grants'].append(grant)
    scene = {'capability': 'ui.scene', 'api': 1, 'instance_id': 0}
    if scene not in row['grants']:
        row['grants'].append(scene)
    catalog = {'capability': 'app.catalog', 'api': 1, 'instance_id': 0}
    contexts = [r for r in boot['app_capabilities'] if r['manifest'] == 'contexts.json']
    if len(contexts) == 1 and catalog in contexts[0]['grants'] and catalog not in row['grants']:
        row['grants'].append(catalog)
    granted = {(g['capability'], g['api']) for g in row['grants']}
    if not required <= granted or not granted <= declared or len(row['grants']) > 32:
        raise ValueError('Missing required or undeclared/capacity grant')
    if len({encode(g) for g in row['grants']}) != len(row['grants']):
        raise ValueError('Duplicate grant')
    new_manifest = copy.deepcopy(installed)
    # Build-only category/runtime_profile metadata is not part of the pinned
    # Runtime manifest schema; retain the installed launch metadata instead.
    for key in ('requires', 'optional', 'version', 'entry', 'architecture'):
        new_manifest[key] = copy.deepcopy(manifest[key])
    out[MANIFEST] = encode(new_manifest)
    storage, old_path = old_route
    target = sd if storage == 'sd' else out
    if old_path not in target:
        raise ValueError('Application route missing')
    changed, sd_changed, sd_removed, sd_added = {MANIFEST, 'boot.json'}, set(), set(), set()
    new_path = old_path
    if storage == 'sd':
        folder = PurePosixPath(old_path).parent
        sidecar = str(folder / 'manifest.json')
        if sidecar not in sd or json.loads(sd[sidecar]) != installed:
            raise ValueError('SD sidecar differs')
        if folder.name.startswith('image-'):
            if folder.name != 'image-' + digest(sd[old_path]):
                raise ValueError('Original Points content address differs')
            if {n for n in sd if n.startswith(str(folder) + '/')} != {old_path, sidecar}:
                raise ValueError('Unknown content-addressed assets')
            if {n for n in sd if n.startswith(str(folder.parent) + '/') and n.endswith('/' + ELF)} != {old_path}:
                raise ValueError('Stale or ambiguous Points content-addressed executable')
            new_folder = folder.parent / ('image-' + digest(app))
            new_path, new_sidecar = str(new_folder / ELF), str(new_folder / 'manifest.json')
            if new_path in sd or new_sidecar in sd:
                raise ValueError('Candidate route collision')
            del sd[old_path]
            del sd[sidecar]
            sd_removed.update((old_path, sidecar))
            sd_added.update((new_path, new_sidecar))
            sd[new_path], sd[new_sidecar] = app, out[MANIFEST]
            row['image_source']['path'] = new_path
        else:
            sd[old_path], sd[sidecar] = app, out[MANIFEST]
            sd_changed.update((old_path, sidecar))
    else:
        out[old_path] = app
        changed.add(old_path)
    boot['app_capabilities'][boot['app_capabilities'].index(rows[0])] = row
    out['boot.json'] = encode(boot)
    verify_preservation(before, sd0, out, sd, original, row, changed, sd_changed, sd_removed, sd_added)
    if (sd if storage == 'sd' else out)[new_path] != app:
        raise ValueError('Selected executable differs')
    receipt = {'app_sha256': digest(app), 'app_row': row, 'original_app_row': original,
               'app_route': {'storage': storage, 'path': new_path},
               'changed_internal': sorted(n for n in changed if out[n] != before[n]),
               'changed_sd': sorted(n for n in sd_changed if sd[n] != sd0[n]),
               'removed_sd': sorted(sd_removed), 'added_sd': sorted(sd_added),
               'native_metadata_unchanged': out.get('cohort.json') == before.get('cohort.json'),
               'storage_authority_unchanged': True, 'providers_unchanged': True, 'physical_execution': False}
    return out, sd, receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('application', 'baseline', 'bootfs-tools', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    # These helpers are immutable staged System sources from the app build.
    sys.path[:0] = [str(args.application / 'sources/System/scripts'), str(args.bootfs_tools)]
    from assemble_shared_home_cohort import inventory, fingerprint, pack_candidate
    from image_envelope import MAGIC, decode_image, encode_image, receipt as envelope_receipt
    from read_only_spiffs import read_image
    from current_bootfs import build
    baseline = json.loads((ROOT / 'docs/shared-points/baseline.json').read_text())
    verify_baseline(args.baseline, baseline)
    app, _, manifests = validate_application(args.application)
    args.output.mkdir(parents=True, exist_ok=False)
    results = {}
    for lane in LANES:
        raw = (args.baseline / (lane + '-store.bin')).read_bytes()
        stored0 = read_image(raw, len(raw))
        before = {n: decode_image(b) if b.startswith(MAGIC) else b for n, b in stored0.items()}
        sd0 = read_archive(args.baseline / (lane + '-sd-apps.zip'))
        out, sd, receipt = assemble_lane(lane, before, sd0, manifests[lane], app)
        stored = dict(stored0)
        for name, data in out.items():
            if data != before[name]:
                stored[name] = encode_image(data) if stored0[name].startswith(MAGIC) else data
        if {n: decode_image(b) if b.startswith(MAGIC) else b for n, b in stored.items()} != out:
            raise ValueError('Canonical recovery differs')
        directory = args.output / lane
        for kind, entries in (('before', before), ('before-stored', stored0), ('before-sd', sd0),
                              ('after', out), ('stored', stored), ('after-sd', sd)):
            (directory / kind).mkdir(parents=True, exist_ok=True)
            write_tree(directory / kind, entries)
        encoded = {name: envelope_receipt(out[name], data)
                   for name, data in stored.items() if data.startswith(MAGIC)}
        if encoded:
            receipt['image_storage'] = {
                'schema': 1, 'format': 'RTEIMG1', 'images': encoded,
                'canonical_files': {n: digest(b) for n, b in sorted(out.items())},
                'stored_files': {n: digest(b) for n, b in sorted(stored.items())},
                'canonical_recovery_verified': True,
                'selection': 'Preserve each pinned baseline file storage representation'}
        receipt['inventory_sha256'] = fingerprint(inventory(before, sd0))
        receipt.update(pack_candidate(directory, stored, sd, len(raw), build, read_image))
        results[lane] = receipt
    (args.output / 'assembly.json').write_bytes(encode({'schema': 1, 'products': results,
        'shared_points': digest(app), 'packed': True, 'baseline_release': baseline['release']}))
    print('Canonical Points and unchanged provider products packed:', digest(app))


if __name__ == '__main__':
    main()

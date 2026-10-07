"""Watch 1.0.2 tag publication preserves the prior release and immutable refs."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import urllib.error
import urllib.parse

ROOT = Path(__file__).parents[1]
SPEC = importlib.util.spec_from_file_location(
    'publisher_102', ROOT / 'scripts/publish_watch_101_components.py')
p = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(p)
CONFIG = json.loads((ROOT / 'release/watch-1.0.2-components.json').read_text())
REPO = CONFIG['repository']
SOURCE = CONFIG['source_sha']
SNAPSHOT = json.loads((ROOT / 'tests/fixtures/watch-1.0.2-manifests.json').read_text())
OTHER = 'b' * 40
TAGS = ['app-points_in_time-v0.4.3', 'app-timecard-v0.1.0']


class API:
    def __init__(self):
        self.refs = {}
        self.created = []
        self.failed_workflow = None
        self.race_target = None

    def request(self, path, data=None):
        if path.startswith('/actions/workflows/'):
            filename = urllib.parse.unquote(path.split('/')[3])
            workflow = '.github/workflows/' + filename
            return {'workflow_runs': [{
                'head_sha': SOURCE, 'head_branch': 'main', 'event': 'push',
                'path': workflow, 'run_number': 1, 'status': 'completed',
                'conclusion': 'failure' if workflow == self.failed_workflow else 'success',
            }]}
        if path.startswith('/git/ref/tags/'):
            return self.refs.get(urllib.parse.unquote(path.split('/git/ref/tags/')[1]))
        if path == '/git/refs':
            tag = data['ref'].removeprefix('refs/tags/')
            self.refs[tag] = {'object': {'type': 'commit',
                                        'sha': self.race_target or data['sha']}}
            if self.race_target:
                raise urllib.error.HTTPError('https://api.github.com', 422, 'race', {}, None)
            self.created.append(data)
            return self.refs[tag]
        raise AssertionError(path)


def git(*args):
    if args[0] == 'show':
        source, path = args[1].split(':', 1)
        if source != SOURCE:
            raise AssertionError(source)
        if SNAPSHOT['source_sha'] != source:
            raise AssertionError(source)
        return json.dumps(SNAPSHOT['manifests'][path])
    return SOURCE


class Watch102PublisherTests(unittest.TestCase):
    def test_exact_reviewed_component_inventory(self):
        self.assertEqual(TAGS, p.validate_config(CONFIG, REPO))
        self.assertEqual('5acbd52d97d90c509e217a128b3ee5a6da30975c', SOURCE)
        self.assertEqual(3, len(CONFIG['required_workflows']))
        timecard = next(c for c in CONFIG['components'] if c['id'] == 'timecard')
        self.assertEqual('Apps/native/timecard.json', timecard['manifest'])
        p.verify_source(CONFIG, 'main', git)

    def test_both_tags_are_idempotent_and_preserve_previous_refs(self):
        api = API()
        old = json.loads((ROOT / p.CONFIGS['Watch1.0.1']).read_text())
        for tag in p.validate_config(old, REPO):
            api.refs[tag] = {'object': {'type': 'commit', 'sha': old['source_sha']}}
        previous = copy.deepcopy(api.refs)
        p.publish(CONFIG, REPO, 'main', api, git)
        self.assertEqual(TAGS, [x['ref'].removeprefix('refs/tags/') for x in api.created])
        self.assertTrue(all(x['sha'] == SOURCE for x in api.created))
        p.publish(CONFIG, REPO, 'main', api, git)
        self.assertEqual(2, len(api.created))
        for tag, ref in previous.items():
            self.assertEqual(ref, api.refs[tag])

    def test_any_collision_prevents_all_creates(self):
        for tag in TAGS:
            with self.subTest(tag=tag):
                api = API()
                api.refs[tag] = {'object': {'type': 'commit', 'sha': OTHER}}
                with self.assertRaisesRegex(ValueError, 'Immutable tag collision'):
                    p.publish(CONFIG, REPO, 'main', api, git)
                self.assertEqual([], api.created)

    def test_every_required_source_workflow_must_pass(self):
        for workflow in CONFIG['required_workflows']:
            with self.subTest(workflow=workflow):
                api = API()
                api.failed_workflow = workflow
                with self.assertRaisesRegex(ValueError, 'CI not successful'):
                    p.publish(CONFIG, REPO, 'main', api, git)
                self.assertEqual([], api.created)

    def test_racing_different_tag_is_never_moved(self):
        api = API()
        api.race_target = OTHER
        with self.assertRaisesRegex(ValueError, 'Tag creation conflict'):
            p.publish(CONFIG, REPO, 'main', api, git)
        self.assertEqual([], api.created)
        self.assertEqual(OTHER, api.refs[TAGS[0]]['object']['sha'])

    def test_unreviewed_release_and_wrong_owner_are_rejected(self):
        changed = copy.deepcopy(CONFIG)
        changed['release'] = 'Watch1.0.3'
        with self.assertRaisesRegex(ValueError, 'Unreviewed'):
            p.validate_config(changed, REPO)
        changed = copy.deepcopy(CONFIG)
        changed['repository'] = 'michaelrolphone-cmyk/RiscRTE-Utilities'
        with self.assertRaisesRegex(ValueError, 'scoped to Productivity'):
            p.validate_config(changed, changed['repository'])

    def test_main_uses_explicit_release_and_checks_identity_before_network(self):
        event = {'repository': {'default_branch': 'main'}, 'workflow_run': {
            'event': 'push', 'head_branch': 'main',
            'head_repository': {'full_name': REPO},
            'status': 'completed', 'conclusion': 'success', 'head_sha': SOURCE,
        }}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'event.json'
            path.write_text(json.dumps(event))
            env = {'GITHUB_REPOSITORY': REPO, 'GITHUB_EVENT_NAME': 'workflow_run',
                   'GITHUB_EVENT_PATH': str(path), 'GH_TOKEN': 'unit-test-only'}
            configs = {release: ROOT / filename for release, filename in p.CONFIGS.items()}
            with patch.dict('os.environ', env), patch.object(p, 'git', git), \
                    patch.object(p, 'CONFIGS', configs), patch.object(p, 'GitHub') as api, \
                    patch.object(p, 'publish') as publish:
                p.main('Watch1.0.2')
                self.assertEqual(CONFIG, publish.call_args.args[0])
                api.assert_called_once_with(REPO, 'unit-test-only')
                api.reset_mock()
                with patch.object(p, 'CONFIGS', {'Watch1.0.2': configs['Watch1.0.1']}):
                    with self.assertRaisesRegex(ValueError, 'identity mismatch'):
                        p.main('Watch1.0.2')
                api.assert_not_called()


if __name__ == '__main__':
    unittest.main()

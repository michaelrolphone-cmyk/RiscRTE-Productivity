# Independent Productivity development build

Requirements: Python 3.11+, a host C compiler, and the pinned public Xtensa S3 compiler.

```sh
python -m pip install platformio==6.1.19
pio pkg install --global --tool 'espressif/toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5'
python -m unittest discover -s tests -v
python scripts/test_apps.py
python test/native_apps/text_editor_discard_source_test.py
python scripts/test_regressions.py
python scripts/build_all_apps.py
python scripts/check_release_parity.py
```

`PLATFORMIO_CORE_DIR` selects another compiler package directory; `NATIVE_APP_CC`
selects an explicit compiler. `--id text_editor` or `--id timecard` on the builder
makes a selective build; the release-parity command deliberately requires both.
The builder clears only its generated `dist/apps` output so stale apps cannot
leak into artifacts. Normal full output is two ELF+JSON pairs, build evidence,
and a release-parity report. CI uploads these as 14-day development artifacts.

## Pinning and provenance

`sdk/baseline.json` pins the required headers (including USB keyboard interface
transitive headers), compiler helpers, integrity/manifest validation, ELF validator
and upstream fixtures by Git blob and SHA-256. The SDK and three original fixtures originate at Reader
`3300229d0a232b4e6047a7c93b2f518c033c3cfa`. The pinned public firmware export list
is derived from that commit's app/libc/compat tables, whose source blobs are also
recorded. Opening and clock-failure regression files remain pinned to Reader `4530c8b23b13a64f29c212cd64f1e86b05885287`. Current Timecard store-failure fixtures are pinned to Reader `00f9b2458dbfdae2188f2695634edb40c65c7ab0`; the SDK itself is unchanged. It does not import the privileged provider inventory or grant capability
access. Original license and comments are retained. The repository's small build
and audit wrappers are adapted from System-Apps main `b64e1c99`; they read only
this repository and the pinned SDK, not a remote moving branch.

Build evidence records source/manifest/helper blobs, versions, actual compiler,
repository commit and dirty state, byte size and SHA-256. The actual firmware ELF
validator runs against each result and exercises malformed-header, section and
relocation mutations. Unknown public imports, unsafe inventory paths, duplicate
IDs, manifest/version disagreement and modified SDK snapshots are rejected.

## Tests and their limits

- Ten Python pipeline checks cover SDK hashes, manifests, inventory safety,
  source conflict classification, import validation and integrity/parity checks.
- Three unchanged upstream C fixtures exercise editor core, editor UI/keyboard
  behavior and Timecard UI navigation.
- The upstream discard source contract is retained.
- Two retained UBSan-enabled C regression fixtures call the actual app functions:
  discard restore/failure/no-file paths and 12 lunch/shift boundary cases plus
  null input. The discard fixture verifies failed discard does not exit or drop
  edits, including an invalid saved document and mismatched read length.

- Three additional UBSan fixtures exercise actual editor opening, Timecard clock failure, and store failure/recovery: retained-cursor pagination, unsupported entries, cancellation and read/open retry; all four punches, repeated clock failure, recovery, date rollover and save retry; staged store loading, malformed/trailing data rejection, read-only recovery and retry. Timecard source-contract checks run through the regression runner.

Both generated ELFs are checked against current Reader release identities (Text Editor 0.2.3 and Timecard 1.0.4); CI enforces sizes/digests and versions in `sdk/release-baseline.json`. Text Editor's ELF bytes reproduce 0.2.2; Timecard 1.0.4 has a distinct current ELF hash. Current Reader RTE package archive digests/sizes and their release-action evidence are recorded alongside the nested ELF identities. Nothing is re-released under an old identity.
These are host/ELF/build checks, not device, capability hotplug, package-install,
rollback or prospective U1 acceptance tests.

## Safe synchronization

```sh
python scripts/check_baseline.py --reader /path/to/read-only/Reader \
  --ref FULL_COMMIT_SHA --output docs/source-drift.json
```

The optional Reader path is read through immutable Git objects only. Without it,
only local baseline agreement is established. The audit classifies unchanged,
converged, upstream-only, external-only and conflicting inputs, including the
editor helper header. It never writes app source. Review upstream-only changes
with versions/docs; preserve external-only fixes and reconcile conflicts before
advancing baselines. SDK rebaselines require separate inspection and all checks.

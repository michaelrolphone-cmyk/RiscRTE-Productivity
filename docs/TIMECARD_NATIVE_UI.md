# Native Timecard development target

This explicit development profile is version `0.3.0`. It compiles the existing
`Apps/timecard_portable.c` controller and authoritative `Apps/timecard.c` ledger
model with the private native-color view in `Apps/timecard_native_ui.inc` and
System's production adapter. Historical manifests, native-time and `.64`
profiles, product recipes, storage schema and install catalogs are unchanged.
It is not a firmware build, release, installation or hardware qualification.

## Selection and authority

`scripts/build_timecard_native_ui.py` selects only the new
`sdk/timecard-native-ui-sources.json` profile and
`Apps/native/timecard_native_ui.json` manifest. It requires private
`PORTABLE_TIMECARD_NATIVE_UI` owner 14, NOVA native canvas, app-owned chrome,
retained RGB565 handoff, native Alarm and Quick, the native custody fence,
input navigation and app-data storage. Root Home targets `default.elf`;
`TIMECARD_RETURN_APP="springboard.elf"` preserves app-owned nested Back.
Generic `PORTABLE_RETURN_APP` is intentionally absent.

Two independently closed clock compositions are available:

- Default RTC: `rtc.clock@2`, `alarm.service@1`, and wall-time presentation.
- `--utc`: read-only `runtime.realtime@1`, tagged `alarm.service@2`, and the
  existing selected-timezone source. This selects `TIMECARD_NATIVE_TIME`,
  `ALARM_NATIVE_UTC`, `ALARM_SERVICE_TAGGED_V2` and the native time toolbar.
  `TIMECARD_PAPER` remains defined to satisfy the historical native-time
  controller contract; the controller selects native color first when the
  private adapter reports it available. Existing paper handling stays available.

Both compositions use `storage.app-data@1` for the existing civil-date/minute
ledger and `storage.key-value@1` for common preferences. Neither requests clock
write, radio, shared-data or broader file authority. UTC changes only current
civil date and entry defaults; it does not reinterpret old punches or migrate
stored history. The default source manifest is RTC; the builder emits the exact
UTC capability manifest only when `--utc` is selected.

Direct rendering is the default. `--snapshot` selects immutable raster snapshots.
Neither option relaxes owner visibility, contact cancellation, shared-raster
readiness or retained-custody requirements.

## Frozen inputs

The profile records these exact bases:

- Productivity: `fd59829e841ac45354243cbdf2de19881071ac3e`
- System: `c071b3f42a5dd7dd918a57b62c806ca71e2caa7d`
- Runtime: `13eca114cb58469cf6004fafc3da84fbd9d29beb`
- Utilities: `4aea3e2f1c6232dcaf54d504cb0a9ec756a559a8`

System, Runtime and Utilities must be clean exact checkouts. System is the
new source-frozen development commit, based on
`ff6151285ee41bdbeff75ebd31ee2621d524e899`, that adds private owner-14 hooks. Its complete selected source/header inventory must match the profile,
including every new file; a matching Git base alone is insufficient. There is
no dirty-tree waiver or pin override. The original Timecard model SHA-256 and
the application source inventory are independently enforced.

SDK staging uses symlinks. Canonical Runtime `RiscRuntimeV1.h`,
`RiscRealtimeV1.h` and `RiscAppDataV1.h`, and canonical Utilities Alarm API 1/2
headers override copied System headers. The selected Runtime app-data header
adds shared-data and BUSY declarations to the historical consumer header but
preserves the API-1 table. This new target uses the canonical pinned header;
it does not rewrite or silently rebaseline the old consumer header.

The canonical Utilities API-1 optional suffix is a sleep-resume callback.
System's older copied API-1 header instead describes an output-mode suffix at
the same offset, so these headers cannot be interchanged. The Timecard-selected
Quick compatibility path recognizes the canonical sleep-resume declaration and
uses the API-1 legacy both-output fallback without reading the callback bytes as
output flags. The tagged UTC API-2 descriptor remains unchanged.

The UTC composition additionally links the five production System realtime,
timezone and native-source helpers. Both link the production Quick helpers.
No System implementation is copied into Productivity. GCC's actual dependency
closure must be fully included in the frozen inventory before compilation;
inputs are checked again afterward.

## Build and verification

Use the official ESP32-S3 GCC 8.4.0 `esp-2021r2-patch5` compiler:

```sh
export NATIVE_APP_CC=/path/to/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc
python scripts/test_timecard_native_ui_profile.py
python scripts/build_timecard_native_ui.py \
  --system-apps /path/to/System-Apps \
  --runtime /path/to/Runtime \
  --utilities /path/to/Utilities \
  --output-dir /path/to/new/timecard-rtc-direct
```

Add `--utc`, `--snapshot`, or both for the other three compositions.
`--verify-only` checks source/profile/compiler inputs without creating output.
Every build needs a new output directory and will not overwrite prior evidence.
The exact compiler version and little-endian Xtensa macros are required.

All builds use `-Wall -Wextra -Werror`, hidden visibility and an export map
exposing exactly `app_main`, `app_module_init` and `app_module_fini`. All eight
private Timecard hooks must be local, defined symbols. Unexpected imports,
exports or missing private hooks are rejected. Each output contains the ELF32
little-endian Xtensa ET_DYN artifact, selected capability manifest, compiler
command, symbol reports, loader validation, full source evidence, licenses and
a development-only notice. Loader validation also tests malformed/truncated
ELF rejection; it does not execute code on a device.

The profile unit tests cover selectors and owner, RTC/UTC capability closure,
Alarm versions, app-data and native Quick requirements, direct/snapshot
selection, compiler identity, immutable model and source inventories, source
mutation, path escapes, dependency pins and dynamic/private symbol closure.

Run the production host composition tests separately:

```sh
python scripts/test_timecard_native_ui.py \
  --system-apps /path/to/System-Apps --runtime /path/to/Runtime \
  --utilities /path/to/Utilities --output /path/to/new/host-rtc
```

The runner accepts `--utc`, `--snapshot`, `--sanitized`, `--size WxH` and
`--scenario N`. Capture only bounded representatives with `--capture`.
Each composition covers five native sizes from 160x160 to 410x502, thirteen
scenarios and immediate/100-ms completion: controls and editing, save retry,
gestures, clock failure, real Alarm and Quick interruption, retained failures,
physical navigation, repeated actual app invocations, invalid/blank input,
history capacity, read retry, pending-layout input cancellation, 12/24-hour
format and year-midnight defaults. The full eight-way matrix contains 1040 cases.
`scripts/test_timecard_native_ui_parity.py` supplies separate selector-off and
paper parity checks. A target build is not a replacement for these tests.

The parity runner checks 16 selector-off token compositions and 276 paper
scenarios across RTC portrait, RTC native landscape and UTC paper, each with
the selector off/on and normal/sanitized execution. RTC captures 119 identical
frame hashes for each orientation; UTC compares 33 identical scenario stdout
hashes. The original historical fixtures and all their application assertions
remain unchanged. Temporary derived copies modernize only the old host-provider
boundaries: valid grant slot/generation and cleared release state, real ordered
touch edges consistent with snapshots, and no false queue-loss signal at
`lost_at == 0`. The RTC legacy fixture alone suppresses missing-field-initializer
warnings for its historical prefix initializers. Native application and target
builds retain full `-Wall -Wextra -Werror` without warning waivers.

## Dedicated CI and publication limit

`.github/workflows/timecard-native-ui.yml` checks the exact application head,
reads dependency revisions from the frozen profile, validates the complete
source closure before fixtures, runs all eight RTC/UTC × direct/snapshot ×
normal/sanitized native compositions, and checks selector-off/paper parity.
It builds all four target compositions, repeats both direct builds to require
byte identity, and uploads development-only evidence. It does not publish,
flash, migrate or modify any product recipe.

The owner-14 System commit is published from the exact independently reviewed
source tree, so hosted CI can fetch the frozen dependency. The workflow fails closed if the
exact pinned commit is unavailable or its inventory differs. This workflow is
prepared software-qualification machinery, not evidence of a green hosted run.
Local clean exact-commit/hash-matching builds are qualified separately. The
builder does not commit, push, release or silently repin dependencies.


## Shared source inventory refresh for Badge current-profile additions

The legacy native0.3.0 selection remains on the same dependency, capability,
compiler and authoritative model anchors. Its three shared source hashes now
include explicit TIMECARD_CURRENT_PROFILE-only changes to retry status, current
preference reading and native presentation. The prior digests and exact source
baselinebb8d0d99b3a857103d2b01e6b4c756b913aa14d3 are retained in source_refresh.
No wholesale manifest regeneration or application behavior rollback was used.

The source-refresh gate compares complete preprocessed application token streams
for RTC/UTC × direct/snapshot, old three-file content against current content,
with the selected real frozen SDK. All four match. The existing1040 native cases,
16 selector-off profiles,276 paper cases and repeated target determinism checks
remain required. Token parity alone is not target-binary or hardware proof.

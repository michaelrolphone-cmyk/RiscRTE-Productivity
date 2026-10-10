# Timecard native time profile 0.2.0

This explicit X4 profile uses read-only `runtime.realtime@1` instance 0 and
the selected `time_zone` in KV namespace 1. The unchanged Timecard controller,
paper presentation, keyboard, app-data namespace 1, shared time format, Home,
Back and QuickControls remain in use. The alarm client selects the tagged
`alarm.service@2` descriptor. It requests neither `rtc.clock` nor
`runtime.realtime-control`; no clock setting, RTC recovery or migration occurs.

`scripts/build_timecard_native_time.py` is the only native profile entry point.
The default builders and manifests remain Reader 1.0.4, Watch 0.1.1 and raw-paper
0.1.4. Points files and its completed native profile are unchanged. Version 0.2.0
was reserved after checking live main and PR 15 head `4921a430` on 2026-10-07.
This is a local development package, with no publication, release, installation,
flash or physical hardware qualification.

## Clock and ledger have different semantics

Runtime holds absolute UTC. A fresh snapshot is projected through the selected
IANA rule for each current-time punch and each calendar redraw. That single
local snapshot supplies both date and minutes, including midnight and DST
transitions. Shared paper/QuickControls clocks use the same checked source.
Missing `time_zone` uses the Reader's virtual UTC default without writing it;
invalid records or ordinary preference/read failures show clock unavailable.
The native reader closes before a successful sample returns.

Timecard has never stored timestamps: `timecard.json` stores a civil `YYYYMMDD`
and four integers representing minutes after midnight. That schema remains
unchanged. Historical dates and punches never move or change when a timezone
selection changes. An open editor continues to edit its original civil date.
Only a new current-time punch uses the newly selected zone and its current date.

Worked time remains the original wall-clock calculation with lunch-overlap
subtraction. For Denver's 2026 spring jump, 01:59 to 03:00 is 61 wall minutes;
the repeated 01:30 in autumn remains the same minute value. Manual civil entries
in a DST gap remain valid as before: the app does not invert them into UTC.
If changing zones causes Out to precede the stored In, the original model still
saves the entered civil value and displays worked time as unavailable. A next-day
punch belongs to that next civil day and does not close the previous day's In.
No offsets, zones or absolute instants are guessed for old history.

## Storage and lifecycle

The existing app-data bridge retains revision-checked whole-file reads and
atomic replacement of `timecard.json` in namespace 1. It preserves the 400-day
limit, blank-day compaction, strict JSON validation, editing/clear/cancel,
retry after ordinary failures and reconciliation after an uncertain commit.
An unconfirmed save restores the displayed RAM snapshot and locks further edits
until a full read succeeds; the backing file may already contain the new value.

Native-only wrappers check app-owned grant acquisition/release, shared preference
reads and app-data results. Context, retained or unknown statuses, failed external
acquisition, and unconfirmed release enter the shared Runtime retention fence.
No more provider calls, drawing, polling, launching or teardown follow that fence.
Normal typed storage failures remain visible and retryable. The flag-off code
path preserves the previous ELF bytes.

## Build and verification

Dependencies are frozen in `sdk/timecard-native-time-sources.json`: System helper
sources `1d589d90`, shared native adapter `47464cf9`, Runtime `30dcec5c`, and Utilities
`637e13b0`. The builder verifies clean exact dependency checkouts, app-data SDK
bytes and the unchanged authoritative Timecard source. It symlinks external SDK
inputs rather than copying shared implementation into Productivity.

```
python3 scripts/test_timecard_native_time.py \
  --system-apps /exact/System-public --adapter-system /exact/System-native \
  --runtime /exact/Runtime --utilities /exact/Utilities
NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc \
  python3 scripts/build_timecard_native_time.py \
  --system-apps /exact/System-public --adapter-system /exact/System-native \
  --runtime /exact/Runtime --utilities /exact/Utilities
```

The actual controller, app-data adapter, checked time source, shared paper/Quick
adapter and tagged alarm client are linked together for 33 fresh-process scenarios
both normally and with ASan/UBSan. Cases cover timezone changes, DST folds/gaps,
year/leap-day boundaries, legacy civil history, unavailable clocks, malformed
snapshots, CAS conflict, commit uncertainty, failed/full storage, 400-day retention,
invalid/edit/clear/cancel, Home at every depth, local Back, QuickControls with an
unsaved editor, and frozen native/prefs/storage/alarm/cleanup failures.

The target builder checks Xtensa ELF32 ET_DYN, the existing bounded import set,
exact three exports, seven-capability manifest and included license/provenance
files. It emits `build-evidence.json`, the central `x4-native-app.json` receipt,
`timecard-grants.json` and the native-only manifest. Runtime/Sdk hashes in the
receipt are the actual compiled declarations. Raw Watch and rotated paper ELFs
are also rebuilt and compared byte-for-byte with source checkpoint `d9ef6af0`.
The ordinary legacy/portable/paper tests remain applicable. These are host and
target-build results; device touch, physical storage/power cuts and sleep remain
unqualified.

# Points in Time shared development build

Points in Time 0.1.0 is an original application under `portable_apps`, separate from
`apps` (Text Editor 0.2.3 / Timecard 1.0.4). The pinned legacy SDK, Reader parity
baselines, previous build workflow and existing app sources remain unchanged.

## Exact inputs

- System-Apps: `911be9e8042f1bcc46038fb70189eebe4ca106c5`, shared portable adapter,
  RTC/time-format/time conversion declarations and bounded in-place service labels.
- Utilities: `06104e746c6b83542c260250b91dab5ed5386eaf`, shared PointsRecords.h and alarm
  service declarations. Runtime and output implementations are not copied here.
- Public compiler: PlatformIO 6.1.19 and
  `espressif/toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5` (GCC 8.4.0).

The builder requires clean exact dependency checkouts, not moving branch heads.
It structurally validates the Xtensa ELF through the existing validator, checks
ELF32 little-endian ET_DYN architecture, rejects unknown imports/exports, verifies
manifest identity/authority, and emits SHA-256, size, compiler, dirty state, input
hashes and exact dependency pins. There are only three public exports:
app_main, app_module_init and app_module_fini. No direct sound/vibration API import
is permitted. Adapter/fonts/Utilities/Productivity license notices accompany output.

## Reproduce

With the exact System and Utilities commits checked out beside Productivity:

```sh
python -m unittest discover -s tests -v
python scripts/test_points_in_time.py --system-apps ../system-apps --utilities ../utilities
NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc \
  python scripts/build_points_in_time.py --system-apps ../system-apps --utilities ../utilities
```

For the explicit Watch time basis, append `--denver` to the builder and build the
service/faces with the same definition. No deployment-specific firmware source is
embedded in the app. Output is `dist/points-in-time/points_in_time.elf`, its bounded
JSON sidecar, licenses and `build-evidence.json`. The separate GitHub workflow
uploads 14-day development evidence only. There are no release, catalog, flash or
device actions.

The selected local sandbox runs under ptrace, which prevents LeakSanitizer startup.
The complete successful local host command is:

```sh
ASAN_OPTIONS=detect_leaks=0 python scripts/test_points_in_time.py \
  --system-apps ../system-apps --utilities ../utilities
```

This disables leak detection only; AddressSanitizer and UndefinedBehaviorSanitizer
remain enabled. The app itself has no heap allocation. CI does not apply that
override and runs the sanitizer binary with its normal environment. This local
limitation is not represented as a leak-check pass.

## Coverage

The fixture includes actual `Apps/points_in_time.c` and its writer, rather than
reimplementing their behavior. It runs in bare, shared-adapter and explicit app-owned root-return modes, each
normal and ASan/UBSan (six binaries). It checks:

- Missing catalog stays empty with no writes; existing data loads; corruption and
  read failures block changes. Save/readback/reload preserves all eight slots.
- Failed put with and without persistence, failed readback after commit, byte-exact
  retry, editing/Back lock while uncertain, monotonic revision and no implicit save.
- Every edit page, both list pages, slot eight, selection hit regions, enabled/day
  validation, all four alert modes, all five types, hour/minute wrap, duration clamps,
  type-change duration clearing and draft discard.
- 12-hour midnight/noon AM/PM formatting, shared 24-hour preference, zero-write default.
- RTC/service failures, each missing dependency, exactly sized 240×240 UI bounds,
  labels fitting the actual adapter's 6-pixel character cells, native alert dismissal,
  normal step ownership, bounded false-poll cleanup and retained-sleep no-cleanup.
- App-owned root Back queues the selected launcher exactly once. Nested Back and
  uncertain-save Back never queue it. Denied launch preserves the current view and
  grants, reports a retryable error, and can retry the same root destination.

Python inventory tests assert that the legacy app cohort/versions remain unchanged,
new app authority is restricted, source/version agree, and CI uses the same exact
pins as the builder. The unchanged full legacy regression and parity pipeline also
runs independently; Points is deliberately absent from legacy release baselines.

These are host and ELF checks. They do not establish physical sound/vibration,
RTC wake reliability, visual hardware quality, installation safety, or exactly-once
alert delivery. Runtime/Watch integration and service recurrence qualification are
owned by their corresponding repositories.

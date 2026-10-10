# Points in Time 0.6.0 native UTC profile

This is an explicit development profile for X4, separate from the unchanged
0.5.3 default/Watch build and manifests. It compiles the existing Points windows,
paper/NOVA controls, custom names/colors, eight slots, 0–720 minute durations,
independent end/three-minute notices, mode preservation, delete confirmation,
draft cancellation and exact persistence retry. Timecard is unchanged.

## Native time and service contract

Select `ALARM_NATIVE_UTC`, `ALARM_SERVICE_TAGGED_V2`,
`PORTABLE_NATIVE_TIME_TOOLBAR` and `PORTABLE_NATIVE_CUSTODY_FENCE` together through
`scripts/build_points_native_utc.py`. Native Points requires `alarm.service@2`:
it validates the complete tagged descriptor before treating the output mode as
visual-only. It does not interpret a v1 suffix, downgrade, or grant a second
clock authority. Copied status/token structs remain the v1 value layout.

Only `runtime.realtime@1` is acquired, as a read-only TIMER_ONLY client. Every
snapshot closes its reader grant before timezone storage, rendering or the next
sample. The shared toolbar/Quick callback uses the same sampler and frozen
419-entry rule. The app and adapter therefore never overlap reader grants and
never show different clock bases. No RTC read, recovery, seed, control grant,
UTC+08 fallback or timezone write is present in this profile.

The read-only namespace 1 `time_zone` preference names the frozen IANA rule.
Missing means the shared virtual UTC default. Malformed/noncanonical/corrupt
preferences or ordinary IO leave time unavailable and can be retried. Native
UNSET is shown as unset and blocks catalog saves. New-point draft suggestions
use local civil time. Time editors identify the current zone. A zone change
updates projection and display without rewriting persisted catalog bytes.

All absolute catalog, service status, token and occurrence fields are UTC seconds
since 2000, bounded by 1200798847 (Unix INT32_MAX). The app uses Utilities
`PointsUtcSchedule.h` rule-taking hooks. Gaps and folds are skipped and reported;
ends/warnings use elapsed duration from a unique start, retaining its local-day
ownership. No raw `points_project` or raw event conversion is used. A pending
service occurrence remains owned by the service with its original UTC identity
across a timezone edit. The app never reads/writes the occurrence ledger.

App namespace 5 writes only `points_utc_cfg` and `points_utc_meta`.
`points_utc_occ` is the service-owned PTU1 ledger, with its frozen catalog
`timezone_index`. The service has exactly 9 bound keys; the app metadata key is
not service-bound. Raw `points_cfg`/`points_meta`/`points_occ` records are neither
read nor migrated. Factory defaults remain virtual until an explicit save.

A catalog save preserves the other slots, increments revision, and records a
fresh UTC creation boundary. Exact pending bytes survive ordinary put/readback
IO, metadata-first partial commits, subsequent timezone changes, and retry.
While uncertain, editing/Back remains blocked. Cancel/Back/accepted global Home
do not persist a draft. Saving intentionally resets future eligibility for the
whole catalog as in the existing app.

Native/KV CONTEXT, unknown KV status, uncertain grant ownership or release, and
ALARM_RETAINED fence through `portable_adapter_retain()`. All later provider,
status, render, storage, cleanup and release calls stop. Ordinary typed KV IO
is an unconfirmed/retryable persistence error, not automatic retained custody.

## Reproduce and provenance

`sdk/points-native-utc-sources.json` freezes public System helper bytes, canonical
Runtime 30dcec5 headers, Utilities source and the matching adapter source. Builders
require clean exact checkouts. An adapter publication flag records its known
publication state and does not imply hardware or product qualification.

```
python scripts/test_points_native_utc.py --system-apps ../system-public \
  --utilities ../utilities-utc --runtime ../runtime
python scripts/test_points_native_adapter.py --system-apps ../system-public \
  --adapter-system ../system-adapter --utilities ../utilities-utc --runtime ../runtime
NATIVE_APP_CC=/existing/xtensa-esp32s3-elf-gcc \
  python scripts/build_points_native_utc.py --system-apps ../system-public \
  --adapter-system ../system-adapter --utilities ../utilities-utc --runtime ../runtime
```

The builder links external sources directly and uses header symlinks with the
canonical Runtime and Utilities declarations. It does not copy unpublished
System implementation into this repository. It validates ELF32 Xtensa ET_DYN,
the three app exports, bounded imports, native-only manifest, hashes and licenses.
Outputs are local development artifacts, not product/BIN/install catalogs.

The controller suite runs normal and ASan/UBSan, covering production source,
DST gap/fold boundaries, elapsed durations, UTC2000 limits, virtual defaults,
UI controls and held/canceled contacts, byte-exact partial-save retry, zone edits,
API 2 descriptor refusal and terminal custody. The actual app/helper/adapter suite
separately checks one live reader, toolbar output, persistence, safe IO retry and
no provider or cleanup after retention. Existing paper renderer/control and
Watch keyboard regressions remain separate. Leak detection is disabled in the
local ptrace sandbox; AddressSanitizer/UBSan remain enabled. No hardware,
suspend/resume, flash install or product cohort is qualified by these checks.

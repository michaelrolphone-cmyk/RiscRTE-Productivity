# Timecard portable UI/model profile 0.1.0

This is development work toward the existing Timecard on small displays. It is
not an installable Watch application yet: the current paired Watch has no writable
user-data filesystem with the required atomic replacement contract. The builder
therefore emits `timecard-ui-development.elf`, provenance and an explicit
`NOT_INSTALLABLE.txt`, with **no deployable manifest or launcher entry**.

## One authoritative model

`Apps/timecard_portable.c` includes the unchanged `Apps/timecard.c` under private
entry/getter names. Calendar math, 400-day records, weekly labels, lunch-overlap
worked time, JSON loading and serialization remain the original Timecard 1.0.4
implementation (Git blob `fee216e8b5507b7b2fd4ee5488e34767f4bc13d2`). There is no
Watch copy of that source. The original manifest, SDK baseline, migration
inventory and released ELF expectations remain untouched.

The portable profile's controller connects those functions to existing System
Apps NOVA drawing, raw-contact/crown-navigation, RTC conversion, 12/24-hour
preference and the standard Points 32-key keyboard. It does not use the obsolete
eight-key pager or pretend to provide firmware KeyboardEntryActivity/relaunch.
Editing is invocation-local: DONE saves, Back cancels, and the same opaque core
cookie identifies the original day/punch. An external app handoff exits without
queuing a competing Home request.

System dependency: `a708a45ef47a4d625c1da9fd334bbbd817aa3e15` (PR46). This profile
requires no edits to System Apps, Runtime or Watch for its independent UI tests.
All drawing uses the existing shared renderer and embedded font/FontAwesome
assets. Shared functions, not board or pin constants, own display/touch/RTC.

## Small-screen behavior

- Scroll through all 20 weeks, seven days and four punch actions
- Readable day cards show all four labeled times; no five-column squeeze
- Day rows edit Clock in, Lunch start, Lunch end and Clock out
- Standard Points keyboard preserves its three ASCII pages and hit geometry
- Namespace-1 shared time-format preference is read only; missing/invalid values
  use the existing shared 12-hour fallback without writing defaults
- One validated clock snapshot anchors each entire frame and each current-time
  punch; clock failure cannot create a 1970 record or mix dates at midnight
- Touch drag scrolls; right swipe is recognized while contact remains valid,
  not by guessing that a lost/cancelled sample is a successful release
- Crown Back cancels editing, then traverses Day -> Week -> Weeks -> Home
- Header/footer/status are redrawn after scrolling, so content cannot cover them

The compact renderer uses the existing 240-pixel NOVA presentation surface.
Raw touch uses the same centered origin on larger surfaces; smaller surfaces are refused before storage access. Other presentation styles can reuse the same Timecard model. No hardware
accuracy, current draw, touch timing or rotary crown behavior is claimed.

## Data-safety guards for this profile

The original source's permissive parser is entered only after a bounded strict
schema validator accepts the complete input. It accepts at most 49,151 bytes,
400 unique real Gregorian dates in 1970..9999, known fields, integer punches
-1..1439 and valid JSON grammar. Overflow, malformed/trailing data, invalid dates,
unknown/duplicate fields and duplicate days are rejected without publishing a
partial history. Existing documents emitted by Timecard remain accepted.

This deliberately tightens acceptance of files the legacy parser would silently
reinterpret or drop: unknown fields, escaped keys, fractional/exponent numbers,
invalid separators, pre-1970 dates and out-of-range punches. Such files remain
untouched and editing is unavailable until a complete clean read succeeds.

Time entry accepts hours with optional minutes and an exact case-insensitive
AM/PM suffix; blank input clears the punch. It rejects junk, overlong numeric
fields and mixed suffixes rather than treating any `a` or `p` as a time marker.

Every portable mutation uses one guarded wrapper. It refuses a new 401st day
instead of calling the legacy eviction path. Records with no punches are
compacted only consistently with the original serializer, which omits them;
clearing a day's final punch frees that slot without discarding recorded history.

A failed save restores the last displayed RAM snapshot and makes editing
unavailable until an explicit full reload. The backing file may contain the old
or new complete value after an uncertain write; this is not a storage rollback
promise. The keyboard retains its draft after failure. Repeated reads, displays,
preference loads and navigation do not write history.

## Storage boundary and remaining implementation

The app-local `timecard_portable_file_storage()` link seam returns the existing
`T5StorageApi` complete-file callbacks. Its default is NULL, which renders an
honest unavailable screen and never interprets missing storage as an empty
writable history. Only host fixtures supply an implementation in this increment.
There is no new Runtime import or arbitrary KV-based file representation.

A real adapter must map only `/sd/.crosspoint/timecard.json` to its admitted
application data file. `exists=false` may mean confirmed file absence only;
unmounted/unavailable/corrupt storage must cause a failed read, never a new store.
Whole-document reads must reject oversize/partial results. Atomic writes must
preserve the prior file until complete replacement; ambiguous commits remain
explicit. No bootfs writes, formatting, partition changes or installed-files
capability are hidden in this frontend.

KV@2's 2,048-byte values and shared 24 KiB NVS are still insufficient. The separate
app-data prototype investigates a dedicated LittleFS partition and explicitly
scoped ordinary files, preserving both boot stores. A new paired layout must
remain opt-in and incompatible old OTA must be rejected. This document does not
claim that backend, provisioning, layout transition or reflash preservation is
implemented by the UI profile.

## Build and verify

```
python scripts/test_timecard_portable.py --system-apps /exact/System-Apps
python -m unittest discover -s tests -v
NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc \
  python scripts/build_timecard_portable.py --system-apps /exact/System-Apps
```

The tests run normal and ASan/UBSan variants for strict parsing, unchanged legacy
UI/calculation/clock/storage fixtures, the actual portable controller and real
NOVA pixel renderer. They cover 400/401 days, maximum document length, overflow,
failed reads, uncertain saves with both old/new durable outcomes, clean recovery,
clear-at-capacity followed by a new punch, edit/cancel, both time formats,
midnight/year rollover, held/drag/lost contact, nested Back, and clipping guards.

A separate integration fixture links the unmodified production shared adapter,
real portable app and raw touch/display/RTC/navigation providers. It exercises
editing/DONE, crown cancellation, nested Back, root return, stride guards and
capability/frame cleanup. The optional foreground alarm client honors shared retained sleep failures before drawing, launching or releasing grants. A focused consumer test covers that guard; Quick Controls and full hardware sleep coexistence remain later production-integration checks.

The exact GCC 8.4 build validates Xtensa ELF structure and import/export bounds
and records source hashes/pins. The existing Reader migration workflow still
builds both legacy apps and checks their old release identities. These are host
and target-build results, not execution of Xtensa instructions on a device,
physical file durability, power-cut qualification, installation or deployment.

## Explicit app-data prototype bridge

The optional `--runtime-appdata /prototype/Runtime` build/test argument compiles
`TIMECARD_APP_DATA` against that checkout's `RiscAppDataV1.h`. This is a separate
`dist/timecard-appdata-development` artifact, still without an install manifest.
Its evidence records the actual Runtime checkout identity/dirty state and exact
header hash; it does not present unpublished local changes as a finalized pin.

`Apps/timecard_appdata_bridge.h` maps the single original path to `timecard.json`
in explicit app-data namespace 1. It distinguishes confirmed absence from all
errors and carries the service's opaque revision through read/replace CAS.
Oversized input is refused. A stale write requires reload; failed revision refresh
after confirmed replacement is reported as unconfirmed commit. RETAINED latches
locally and allows no more file calls, display, launch or grant release. The app
returns to the new Runtime's separately tested pre-finalization barrier, which
must preserve the invocation rather than unload or sleep. The prototype must
never be combined with a Runtime that lacks that safety barrier.

Normal/sanitized bridge and actual source-controller tests cover full-size bytes,
missing versus unmounted/error, revision creation/read/replacement, stale CAS,
commit-then-error reconciliation and retained early-return cleanup. These tests
complement the Runtime file/fault/actual-ELF suites. They are not a claim of a
complete Watch image or qualified LittleFS installation. Default UI builds remain
unbound until an explicitly selected app-data deployment replaces that profile.

The builder also accepts --alarm-client and --navigation to target-compile the existing shared lifecycle interfaces. These flags do not themselves claim a deployable Watch graph or physical sleep qualification.

CI also target-builds --app-data-client --alarm-client --navigation --denver against the recorded consumer declaration. This compiles the complete client/lifecycle combination without pretending that a Runtime implementation is embedded or pinned by that client-only artifact.

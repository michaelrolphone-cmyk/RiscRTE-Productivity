# Selected storage-scaled Points 0.6.9

`Apps/points_catalog_app.c` is the explicit selected source for the Watch NOVA
and X4 paper editor. The original eight-slot app, metadata, source, recipes and
SDK selections are unchanged. This profile requires Runtime 0.1.83 and the
catalog-aware alarm service from the same cohort. An old API1 or API2 provider
without the tagged Points projection suffix is refused before catalog loading
or editing. The service API1/raw-RTC Watch and API2/native-UTC X4 domains remain
separate; their saved documents cannot be silently reinterpreted.

The shared model uses stable event/type IDs, dynamic arrays and a complete
revision-checked atomic file replace. There is no fixed application count limit.
The file is `48 + 32 * events + 64 * types` bytes. The selected backend's file,
volume and available memory limits are the practical limits. Sorting uses a
heap index and iterative heapsort rather than recursion or a count-sized stack
array. Full storage keeps the draft; a later Save can retry. A failed list-index
allocation also keeps the complete saved document and exposes Retry.

A missing `points.catalog` imports the appropriate legacy config and metadata
in memory. Existing IDs, labels, colors, enabled flags, weekdays, durations and
notification options are preserved. Ordinary entry and cancellation do not
write defaults or migration data. The first explicit successful Save persists
the complete migrated catalog. Corrupt or unavailable legacy data is not
replaced with defaults. No SD migration is included.

Custom types persist a 31-character ASCII name, 24-bit color, independent symbol
0–7, duration availability/default, default mode and end/warning options. New
points receive defaults when a type is explicitly chosen. Editing a type does
not rewrite existing points or reset their occurrence revisions. Referenced
types cannot be deleted. On paper the name editor has an eight-symbol picker;
color remains a separate property that is visible on Watch.

A Save with an uncertain commit blocks further mutations and local Back.
Check Save only reads the complete file and compares it with the pending image;
it never blindly replays a replace. A conflict requires explicit reload/review.
The existing X4 global Home action remains an accepted adapter exit: it can leave
an uncertain-save screen after the alarm fence, releases cleanly, performs no
second write, and reloads the complete document on the next entry. It does not
report the uncertain save as confirmed. Watch preserves its existing
accepted-exit path; this exact adapter does not implement a new Home gesture.

Every app-owned storage operation and capability acquisition/release first
stops Contexts, then BLE broadcast.
Refusal retains the invocation before storage. CONTEXT/RETAINED, a lost Runtime
API, failed capability release, or uncertain output ownership produce one-way
retention. Saved/read/staged buffers stay owned by that frozen invocation; no
allocation, free, retry, provider call, draw, release or repeated entry follows.
Ordinary I/O, full storage, stale revision and unavailable clock remain
recoverable. The selected app validates provider custody after every call.

## Layout and input

Watch uses 240×240 NOVA, explicit seven-day selection, paged type/name controls,
and the original app-owned Back policy. X4 uses the physical 800×480 display
rotated to 480×800; unsupported geometry is rejected before drawing or storage.
The paper renderer follows the supplied refined reference's 62-pixel edit rows,
Orbitron/Rajdhani typography, weekday pills, OFF/ON control, QWERTY proportions,
time columns, heavy outlines and one-bit bottom fades. Licensed font subsets
and their reproducible generator are included. Hours below 10 are unpadded;
minutes remain two digits.

Paper edits add explicit Cancel/Save below the visible reference content;
vertical paging reveals those controls. The original reference did not define
persistence controls. The time buttons retain one-minute precision; outlined
minute neighbors show five-minute context. X4's notification row reports its
actual visual-only capability while preserving the saved mode. The keyboard
keeps the model's 31-character capacity rather than the reference demo's 14.
Repeated occurrences of a type use its saved symbol, independent of list order.

Ordinary input acts only on a completed, unchanged frame. Contacts that cross
an unrelated pending frame are discarded. The keyboard captures a separate
key identity on finger down and inverts that key with partial damage. Its own
pressed-frame present may remain pending while an accepted release is queued;
the character is committed once after completion. Drag-off, multicontact,
cancel, Back or retention clears the contact; another tap while pending cannot
activate a second key. Frames are drained on a clean exit. Terminal custody
never drains or touches them. Hardware keypress latency and ghosting remain
device checks.

## Grants and selected sources

The editor needs `storage.app-data@1`, namespace 5, exact file `points.catalog`,
read/write/atomic replace. Its legacy `storage.key-value@1`, namespace 5, grants
are read-only: Watch `points_cfg`, `points_meta`; X4 `points_utc_cfg`,
`points_utc_meta`. Preferences use namespace 1 read-only (`time_format`, plus
`time_zone` on X4). It has no occurrence-ledger or provider output grants.
Other required capabilities are declared in the selected manifests. The full
Watch recipe also selects Contexts, BLE, Quick Actions/radios and the exact
local sleep helper. The X4 recipe retains Quick Actions, input navigation,
app-owned Back and `default.elf` global Home. Provider bound-file/KV grants
belong to the alarm package and are not duplicated by the app.

The full local source pins are in `sdk/points-catalog-sources.json`. Build with
`scripts/build_points_catalog.py`, supplying each path and its exact revision:
`--target watch|x4`, `--adapter-system`, `--presentation-system`, `--utilities`,
`--runtime`, and each matching `--*-revision`. All dependencies must be clean.
Set `NATIVE_APP_CC` to the pinned Xtensa GCC 8.4.0 esp-2021r2-patch5 executable.
For both full profiles select `--ble-broadcast`; Watch also selects `--contexts`,
`--watch-firmware` and `--watch-firmware-revision`. Supply `--output-dir`.
The recipe symlinks narrow SDK headers and compiles the exact shared adapter;
it does not vendor unrelated System source. It emits the ELF, manifest,
compiler/source/dependency/import/export/hash receipt, individual stack frames,
ELF structural validation and required licenses. The only app exports are
`app_main`, `app_module_init`, and `app_module_fini`.

## Qualification

Run `scripts/test_points_catalog.py --utilities PATH --runtime PATH
--presentation-system PATH --watch-system PATH --x4-system PATH --output-dir
PATH`. The actual controller, application and selected shared renderer are
compiled with typed capability fakes, both normally and with ASan/UBSan.
Coverage includes more than 126 events, 25 custom types, migration holes and
metadata, full storage/retry, both outcomes of ambiguous save, conflict/reload,
custom defaults, independent symbols/colors, day masks, cancelled sub-edits,
repeated entry, old service refusal, real X4 global Home during uncertain save (Watch accepted-exit simulation),
background-stop ordering and actual retained load/resolve/write/release paths.
Allocator hooks reject every post-terminal allocation or free. The production
control coordinates also exercise paper QWERTY/layers/space/clear/backspace,
length limits, disabled blank Done, inline days/checks/toggle, time and Cancel.
Raw-contact tests verify finger-down inversion, bounded partial damage,
move-off/multicontact cancellation, queued release, repeated pending taps,
Back and retention cleanup.
LeakSanitizer is disabled because this executor's ptrace prevents it; ASan and
UBSan are enabled. Retained allocations intentionally remain pinned.

`compare_points_catalog_frames.py` composes supplied 480×800 reference PNGs
with actual unscaled one-bit production frames. It records hashes for both
inputs and each sheet. Reference, renderer and deliberate functional
differences are labeled; these are not screenshots of a reimplemented mock.
The four final sheets and qualification summary are in
`docs/evidence/points-catalog/`. Target stack figures are individual static
frames, not a hardware stack high-water measurement. The app budget remains
16 KiB. Physical storage/power cuts, wearable input, e-paper ghosting and sleep
are still device acceptance checks. Nothing here claims a hardware flash or
publication.

Legacy parity is checked against Productivity 8c8263a8: all original Points
sources/manifests/build recipes are byte-identical. The baseline raw-API host
script has a pre-existing missing descriptor compatibility guard; its separate
repair is qualified in the parent hour-format commit 830e50c550ab187d88b83fd5a009414deb563a92.
That unrelated legacy delta is deliberately not folded into this selected
source addition. X4's separate automatic-idle helper/transition profile remains
an explicit deployment integration selection.

# Selected X4 Points and Timecard touch scrolling

The explicit `--touch-scrolling` profile selects Points **0.6.6** and Timecard
**0.2.6**. It requires `--paper-transitions --motion-system PATH`; add
`--ble-broadcast` to retain the telemetry profile. Defaults, Watch, nonselected
paper profiles, UTC/timezone authority and the Timecard civil-history model
remain unchanged. This is local source/build evidence, not a release or hardware
qualification.

`sdk/native-touch-scroll-sources.json` pins the clean System input. Its small
`PORTABLE_APP_TOUCH_SCROLL` interface returns the already sampled app-visible
contact, raster clip and frame status. It neither reads input twice nor changes a
Runtime/provider ABI. System implementation remains external. The selected
Productivity flags are `PORTABLE_TOUCH_SCROLL`, `PORTABLE_APP_TOUCH_SCROLL`,
`PORTABLE_PRODUCTIVITY_SCROLL`, and `PORTABLE_PAPER_PREFERENCES` alongside the
existing native-time, custody, navigation and Quick Controls flags.

## Behavior

Overflowing Points lists, editor options, type/mode choices and day presets use
bounded pixel scrolling with finger tracking and decelerating momentum. Timecard
week history and week detail use the same engine. Four-punch day lists and other
lists that fit stay fixed. Quick Controls still owns top-edge gestures; Home,
Back and shared reader rotation retain their existing routes. Timecard Up/Down
controls move one selection and reveal it instead of paging.

Every frame records its content generation, scroll position and selection. A
finger-down latches the last **completed** frame. A later completion cannot
change which item that contact targets. Model/page/keyboard changes invalidate
that identity, and a contact never selects after a drag. Hardware confirmation
first reveals a hidden selection. Pending frames keep only the latest dirty
state; input and momentum keep advancing without queuing display work. Modal,
invalid/replaced contact and retained failure paths cancel momentum. Text,
backgrounds, separators and icons share the same clip rectangle. No scrolling
operation writes app data or changes an alarm.

## Reproduction

Use the exact System, adapter, Utilities and Runtime pins in the existing native
source manifests. Set `NATIVE_APP_CC` to the pinned Xtensa GCC 8.4.0
`2021r2-patch5` compiler. With those dependency paths:

```sh
python scripts/build_points_native_utc.py \
  --system-apps "$SYSTEM" --adapter-system "$ADAPTER" \
  --utilities "$UTILITIES" --runtime "$RUNTIME" \
  --paper-transitions --motion-system "$SCROLL_SYSTEM" \
  --ble-broadcast --touch-scrolling --output-dir dist/points-scroll
python scripts/build_timecard_native_time.py \
  --system-apps "$SYSTEM" --adapter-system "$ADAPTER" \
  --utilities "$UTILITIES" --runtime "$RUNTIME" \
  --paper-transitions --motion-system "$SCROLL_SYSTEM" \
  --ble-broadcast --touch-scrolling --output-dir dist/timecard-scroll
python scripts/test_productivity_scroll.py \
  --system-apps "$SYSTEM" --adapter-system "$ADAPTER" \
  --utilities "$UTILITIES" --runtime "$RUNTIME" \
  --paper-transitions --motion-system "$SCROLL_SYSTEM" \
  --ble-broadcast --touch-scrolling
# Repeat the test command with --flip for raw 180-degree touch and display.
```

Run `test_points_native_adapter.py` and `test_timecard_native_time.py` with the
same selected dependency arguments for the retained UTC/timezone, civil history,
app-data, BLE pause, Quick Controls and failure matrices. Run
`python -m unittest discover -s tests -v` for the repository contracts.

## Verification and limits

- 21 scrolling cases for each app, normal and ASan/UBSan, in each orientation:
  168 process scenarios. Includes drag/tap, bounds, velocity/deceleration,
  snapshot and queued releases, replacement/multitouch cancellation,
  changed/deleted/reordered data, partial rows, keyboard and keyboard changes,
  modal interruptions, old-frame hits, latest-state rendering, confirmation
  visibility and pending deletion confirmation.
- 160 retained native composition scenarios across the two apps, normal and
  ASan/UBSan, including flipped Quick Controls; 55 Python contract tests.
- The shared scrolling engine's ASan/UBSan test covers clock wrap, suspension,
  clipping, limits and momentum interruption.
- Watch Points, Watch Timecard, nonnative paper Timecard, native default Points
  and Timecard, and telemetry Points and Timecard reproduce all seven baseline
  target ELFs byte for byte when scrolling is off.
- Target ELF structure, public imports/exports and pinned dependency hashes pass
  for both selected versions. No provider, storage format or entitlement changes.
- Captured MONO1 app frames were inspected. 63 normalized frames match under
  shared 180-degree output/input orientation. Representative images are in
  `evidence/native-touch-scrolling/`; they are renderer output, not photographs.

Host fixtures exercise real application, adapter, font and glyph code with fake
capability providers. The pinned target compiler builds relocatable ESP32-S3
ELFs. No device, install, flash, physical refresh latency, ghosting, panel FPS or
battery measurements were performed. Source publication and hardware actions
remain outside this work.

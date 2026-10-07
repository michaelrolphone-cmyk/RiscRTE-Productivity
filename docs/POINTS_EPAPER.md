# Points in Time 0.5.0: capability-selected Nova7 E-Ink

The same Points app now uses the shared paper presentation when `display.output`
reports a retaining monochrome logical portrait surface. The Watch 240×240 Nova
layout and its original keyboard/picker paths remain unchanged. For X4's native
800×480 MONO1 panel plus 480×800 touch, the deployment selects software display
rotation 90; app layout is chosen from the resulting capability geometry.

The paper list displays the actual saved/default catalog, Font Awesome type markers, time,
name, duration, days, active count and explicit Add. Six 88px rows fit the X4
screen, with static paging for the remaining records. Selection is inverted.
There is no kinetic scrolling, picker motion, per-second redraw or invented
prototype schedule. Record/service-state changes and deliberate interaction
trigger redraws; unchanged pixels do not submit another physical frame.

Editing preserves the existing writer/service authority. Type, time, bounded
0–720 minute duration, independent end/3-minute warnings, days, notification
mode and enabled state remain drafts until Save. Delete requires its second
explicit selection. Custom-name input exposes all 95 printable ASCII symbols,
retains the 13-byte name bound and keeps keyboard Done separate from custom-type
Save. Literal key labels retain letter case. Colors remain shared metadata so
Watch presentation is preserved. Cancel/Back does not write drafts. An uncertain
save locks edits and Retry reuses the exact pending record.

## Dependencies and development build

- System Apps: `a91770df140173955fe7b0c8ea38a2a75197aa94` (Nova7 paper client).
- Utilities: `23a4887f1eeb7b7ce867c0e243158b899e43f0f8` (matching copied CUE status).
- Point config/custom metadata serialization is unchanged. The app still does
  not own occurrence records or output hardware.
- Shared runtime imports remain `risc_runtime_get_api` plus bounded libc; there
  are no new kernel/provider ABI exports or chip-specific calls in this app.

```sh
python scripts/test_points_paper.py --system-apps /path/to/system --utilities /path/to/utilities
python scripts/build_points_in_time.py --system-apps /path/to/system --utilities /path/to/utilities \
  --display-rotation 90 --navigation --partial-damage --return-app springboard.elf \
  --output-dir dist/points-paper
```

The default build retains the existing Watch full-frame profile. Both dependency
checkouts must be clean and match exact pinned commits; the e-paper manifest adds
navigation only when requested. Namespace 5 still owns `points_cfg`/`points_meta`;
namespace 1 provides the shared time-format preference. Preserve emitted licenses
including the paper font notices.

## Evidence and limits

The new ASan/UBSan fixture links the actual Points model and production MONO1
adapter independently. It checks list paging using snapshot-only touch, pixel
stability when idle, literal lower-case rendering, all 95 keyboard symbols,
draft/Save/Cancel, bounded duration, day toggles, exact-byte uncertain retry,
double-confirm delete, and grant/surface/subscription cleanup. It also executes
the real app main loop on 480×800: idle produces one frame, and Add/Edit/Cancel/
root Back completes without unwanted writes. Existing Watch app/model, 3601-frame
picker, GUI and touch regression suites still run against the new source.

Screens below are actual native MONO1 output rotated only for viewing:

- [Points list](nova/screens/points-list.png)
- [Second list page](nova/screens/points-list-page-2.png)
- [Edit point](nova/screens/points-edit.png)
- [Time](nova/screens/points-time.png)
- [Days](nova/screens/points-days.png)
- [Custom keyboard](nova/screens/points-keyboard.png)

These are development fixtures and ELF checks, not installed-bundle or physical
hardware qualification. X4 provisioning remains separate. Frozen Watch 1.0.2
tag tests read historical manifest fixtures; no old component tag is moved.

## Font Awesome icons

The closest existing FA glyphs replace the mockup's custom markers: regular
circle, square, caret-up, diamond, plus, grip-lines-vertical and solid circle.
Back, Add, More and edit chevrons also use genuine FA subset glyphs. Shared
font provenance and OFL notices are shipped with the artifact; no custom
geometry stands in for a Font Awesome symbol.

## Visual-only output profiles

The acquired alarm service reports supported physical outputs through the optional
size-gated output descriptor. A visual-only service displays `VISUAL ONLY` for
Notify Start. Its sound/vibration selector cannot change the draft; saving other
point fields preserves the existing portable mode, including System Default.
Legacy Watch service tables retain their existing audio/haptic choices. No app
receives output authority or rewrites existing records just to display them.

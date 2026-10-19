# Native Points development target

This is an explicit development artifact, version `0.7.0`, built from the
production `Apps/points_in_time.c` controller and System's production
`lib/PortableApps/src/adapter.c`. It does not build or install firmware, update an
installation catalog, change product recipes, or select a new default build.
The existing UTC, Watch, paper, `.64` and other product source pins are unchanged.
No hardware result is implied by a host test or successful target ELF build.

## Selection and dependencies

`scripts/build_points_native_ui.py` is the only entry point for this target.
Its separately frozen profile is `sdk/points-native-ui-sources.json`; the manifest
is `Apps/native/points_native_ui.json`. It selects:

- Private `PORTABLE_POINTS_NATIVE_UI` owner 12 with native canvas and app-owned
  touch chrome. The production app chooses native color only when the adapter
  reports it available; paper remains on the existing paper presentation path.
- Native UTC controller semantics, explicitly tagged `alarm.service@2`, and
  read-only `runtime.realtime@1`. The same writer and schedule code remain in use.
- App-owned `POINTS_RETURN_APP="springboard.elf"`. The generic
  `PORTABLE_RETURN_APP` selector is deliberately absent, preserving nested Back
  and save/uncertain-storage guards.
- `PORTABLE_ALARM_NATIVE` makes the existing Alarm overlay use reported display
  dimensions, including the 160x160 native-color surface.
- Shared native Quick controls and the native custody/time-toolbar fence.
  The profile does not request radio or time-control capabilities.
- Direct rendering by default, or the immutable raster-snapshot implementation
  with the explicit `--snapshot` argument. Retained RGB565 handoff is selected
  in both modes.

Runtime `64c8e52e46fd91e8fdafd636db04eed88f0c51a9` and Utilities
`2d0e03a58d1d78cec10033dfa384368281156a46` must be exact clean checkouts.
System is source-frozen at
`c980898dc62749df5c9e15dc4e167bafe2dd638f`, composed on
`b98875eb0226eb209b4ec572343bcd68d5fcf373`. The exact revision, file inventory
and SHA-256 values are checked through the new profile. A matching base revision alone does not authorize different source.
There is no general `--allow-dirty`, dependency-pin override or warning waiver.

The builder stages symlinks to System SDK headers, replacing the Runtime and
Realtime headers with the canonical Runtime SDK versions and the two Alarm
service headers with the canonical Utilities versions. This matters because
System's copied API1 Alarm header is not byte-identical to Utilities' canonical
header. The full Utilities Alarm include directory supplies Points records and
UTC schedule contracts. Four production System realtime/timezone helper sources
and the three production Quick helper sources are compiled into the artifact.
No unpublished implementation is copied into Productivity.

The profile freezes the complete selected dependency source directories and
application files. Before compilation, GCC's actual non-toolchain dependency
closure must be a subset of those frozen files. The builder checks hashes again
after compilation to reject a source change during the build.

## Build and verify

Use the existing official ESP32-S3 GCC 8.4.0 `esp-2021r2-patch5` toolchain. The
compiler's exact version line and little-endian Xtensa target macros are checked.
For example, with the four repositories already available locally:

```sh
export NATIVE_APP_CC=/path/to/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc
python scripts/test_points_native_ui_profile.py
python scripts/build_points_native_ui.py \
  --system-apps /path/to/System-Apps \
  --runtime /path/to/Runtime \
  --utilities /path/to/Utilities \
  --output-dir /path/to/new/points-direct
python scripts/build_points_native_ui.py \
  --system-apps /path/to/System-Apps \
  --runtime /path/to/Runtime \
  --utilities /path/to/Utilities \
  --output-dir /path/to/new/points-snapshot --snapshot
```

Add `--verify-only` to validate source/profile/compiler inputs without creating
output. Each build requires a new output directory; no old artifact is deleted
or overwritten. The build uses `-Wall -Wextra -Werror`, hidden default visibility,
and a linker version script exposing only `app_main`, `app_module_init`, and
`app_module_fini`. All eight Points ownership/contact hooks must exist as local
symbols in the final ELF. Unknown imports and extra exports fail the build.

Every successful output includes the Xtensa ELF32 ET_DYN binary, the native
manifest, exact compiler command, dynamic and full symbol listings, loader
validation result, source/profile/dependency evidence, and license/provenance
copies. `build-evidence.json` explicitly records that hardware verification and
product-recipe production were not performed. The loader test also exercises
malformed/truncated ELF rejection; it does not execute the Xtensa code on a
physical device.

The profile contract test exercises selector/owner drift, manifest capability
expansion and alarm downgrade, compiler drift, source mutation and inventory
changes, path escapes, missing source freezes, revision mismatch, unpinned
compiler dependencies, and unexpected/missing dynamic or private symbols.
Functional controller, adapter, and direct/snapshot interaction tests are separate
qualification steps; this builder's success does not replace them.


## Frame readiness and Alarm restoration

The native Points renderer uses the existing shared
`portable_paper_frame_ready()` gate before beginning a frame. A busy shared
raster leaves the native view dirty, and the production app loop retries it.
The gate prevents a newer native draw from overwriting pending direct or
snapshot work; it does not add a public Runtime or T5 API.

Alarm and Quick can restore byte-identical application pixels under a foreign
presentation owner. After that interruption, Points requires completion of a
new owner-12 presentation before accepting input. The adapter explicitly avoids
RGB damage elision for this pending Points visibility generation, so an
unchanged-looking restored frame cannot leave input permanently blocked.
The Alarm test checks real modal drawing, hardware dismissal, cancellation of
the held pre-modal contact, native visibility recovery, and fresh navigation.

`scripts/test_points_native_ui.py` exercises nine scenarios for each selected
size and composition, plus an actual Quick modal scenario when Quick is selected:

1. Time/duration bounds, notification modes, weekdays, enable, save and confirmed
   deletion.
2. Custom-type keyboard, paging, delete/done, color, metadata uncertainty and
   guarded Back/retry.
3. Configuration-save uncertainty, unchanged pending bytes and Back/retry.
4. Horizontal out-and-back gestures, shared picker full/half-row settling,
   navigation, raw queue gaps and changed-map cancellation.
5. Retention with no subsequent provider calls.
6. The real `app_main` RGB selection, Home exit and repeated invocation.
7. Visual-only policy, invalid-clock/zero-weekday save guards and add/cancel.
8. The real `app_main` raw edit, nested Back, root Back and clean exit.
9. Alarm interruption, modal dismissal and restored owner-12 visibility.
10. With Quick selected: real top-edge drag, modal raster, consumed hardware
    Back, held-contact cancellation, restored visibility and fresh navigation.

Default sizes are 160x160, 240x240, 240x280, 320x240 and 410x502: 45 scenario
executions per composition without Quick, or 50 with Quick. The snapshot tests
assert that the first draw seals without acquiring or submitting a physical
frame, proving that they exercise snapshot replay rather than immediate
fallback. `--quick`, `--snapshot` and `--sanitized`
select shared Quick, immutable snapshot and ASan/UBSan coverage respectively.
`--full-frames` selects the same full-frame policy as the development target;
without it, damage-tracked rendering is exercised too. The legacy/paper parity
profile is separate and is not changed by these native target selections.


## Qualification commands

Run the native matrix against the same dependency roots as the target build:

```sh
for q in 0 1; do
  for s in 0 1; do
    for a in 0 1; do
      quick=; snapshot=; sanitizer=
      [ "$q" = 0 ] || quick=--quick
      [ "$s" = 0 ] || snapshot=--snapshot
      [ "$a" = 0 ] || sanitizer=--sanitized
      python scripts/test_points_native_ui.py \
        --system /path/to/System-Apps --runtime /path/to/Runtime \
        --utilities /path/to/Utilities $quick $snapshot $sanitizer \
        --output "/path/to/results/q$q-s$s-a$a"
    done
  done
done
```

Use a different output directory for each composition to retain the generated
command, individual scenario logs, result JSON and normal-build raster captures.
Also run Quick direct/snapshot with `--full-frames --sanitized` to qualify the
development target's full-frame policy. Host tests use a real ordered raw-event
queue, asynchronous native presents, padded RGB565 rows, and production glyphs;
they do not replace hardware acceptance.

Run `System-Apps/scripts/test_points_native_input.py` for the owner-12 reducer
and production direct/snapshot/Quick adapter tests (normal and sanitized), and
`System-Apps/scripts/test_points_native_optout.py` for exact selector-off token
parity with the pinned System base. Foreign owners 0, 8, 9, 10 and 11 are controls.
`System-Apps/scripts/test_points_native_composition.py` compares 68 Points-off
compositions against the merged Updater/LoRa base, including all combinations
of Files, Frequency and LoRa selectors. Their private source remains unchanged.
Host sanitizer runs disable LeakSanitizer; ASan/UBSan success is not LSan coverage.

In Productivity, `test_points_native_ui_parity.py --system ... --utilities ...`
checks eight exact selector-off app token profiles and eight legacy MONO1 scenes
across selector off/on and normal/sanitized. The existing API1 fixture explicitly
uses System's retained copied API1 header; production builds use canonical
Utilities headers. `test_points_native_ui_paper_utc.py --system ... --runtime ...
--utilities ... --output ...` runs 72 tagged UTC/API2 paper scenarios and compares
all submitted MONO1 frames across selector off/on, direct/snapshot and
normal/sanitized. Existing Points model, glyph and raw-touch regressions remain
part of qualification. Their fixture updates supply the current runtime layout,
ordered provider sequence semantics and renderer checkpoint hooks, without
changing legacy app behavior.

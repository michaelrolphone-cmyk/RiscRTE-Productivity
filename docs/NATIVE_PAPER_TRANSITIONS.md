# Opt-in native paper Quick Controls

Points 0.6.3 and Timecard 0.2.3 select the shared X4 paper pull-down motion only
when both `--paper-transitions` and `--motion-system` are supplied. Their normal
native versions remain 0.6.2 and 0.2.2. Watch/raw-paper builders and catalogs are
unchanged. This is local development source and ELF evidence, not a release,
installation, BIN change, or hardware qualification.

The existing `--system-apps`, `--adapter-system`, Runtime and Utilities pins
are verified exactly as before. The additional motion checkout is separately
pinned in `sdk/paper-transitions-sources.json` and must be clean. Only the
selected motion profile compiles its adapter, Quick Controls sources and
presentation headers. Native-time helper sources remain on the original System
pin. Canonical Runtime and tagged alarm SDK headers still take precedence.
No authority, time policy, storage ownership, save behavior or cleanup ownership
is changed by this builder selection. No app-to-app crossfade is selected here.

## Reproduce

Set these paths to clean exact checkouts:

```sh
BASE=/workspace/scratch/c744abbbbd60
export NATIVE_APP_CC="$BASE/watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc"
ARGS=(--system-apps "$BASE/x4-plain-app-logs-63"
      --adapter-system "$BASE/x4-plain-app-logs-63"
      --utilities "$BASE/x4-points-utilities-utc"
      --runtime "$BASE/x4-alarm-app-runtime-public")
MOTION=(--paper-transitions --motion-system "$BASE/x4-controls-radio-015")
python3 scripts/test_points_native_adapter.py "${ARGS[@]}" "${MOTION[@]}"
python3 scripts/test_timecard_native_time.py "${ARGS[@]}" "${MOTION[@]}"
python3 scripts/build_points_native_utc.py "${ARGS[@]}" "${MOTION[@]}"
python3 scripts/build_timecard_native_time.py "${ARGS[@]}" "${MOTION[@]}"
```

Omit both motion options for the existing native profiles. Default motion
outputs are `dist/points-native-utc-paper-transitions` and
`dist/timecard-native-time-paper-transitions`, separate from static outputs.
Each contains its target ELF and native-only manifest, `x4-native-app.json`,
full compiler command, effective source/SDK hashes, exact dependency paths and
revisions in `build-evidence.json`, and licenses. The manifests retain their
original six and seven capability requirements. The ELF validator, bounded
imports and three-export checks run on every target build.

## Qualification

The actual Points/Timecard controllers link the production adapter and native
clock helpers. Motion adds eight fresh-process cases to each existing suite,
both normally and with ASan/UBSan:

- Repeated pull-down/up, intermediate changed pixels and byte-exact restoration
  of the editor background; draft values survive and no write or launch occurs.
- Back during opening, physical Home handoff during opening, and the existing
  crown-to-dismiss behavior. Input from dismissal does not leak into editors.
- Malformed coordinates, replaced contact IDs and multitouch cancellation,
  followed by a successful second opening and dismissal.
- A failed touch-provider snapshot during opening enters retained custody;
  later draw, save, poll and teardown paths make no more provider calls.

The motion suites run 17 Points and 41 Timecard cases per sanitizer mode.
Existing native failure, exact-save-retry and read-only clock checks remain.
Flag-off suites, the full Python test suite, Reader host/regression fixtures,
and the two Reader ELF release-parity checks also pass.

`docs/evidence/native-paper-transitions/verification.json` records results and
the six target byte comparisons against Productivity `829cee872880874473aa3c3bdaee875477034c60`:
native flag-off, Watch, and raw paper for each app. All six are byte-identical.

Provider time and display completion are deterministic test doubles. Physical
panel cadence/ghosting, real touch hardware, frontlight hardware, suspension,
power cuts and installation have not been tested. LeakSanitizer is disabled in
the local sandbox; AddressSanitizer and UndefinedBehaviorSanitizer remain on.
The exact motion pin includes the shared backlight toggle and radio policy correction. Changing it requires rerunning both affected motion suites and target builds.

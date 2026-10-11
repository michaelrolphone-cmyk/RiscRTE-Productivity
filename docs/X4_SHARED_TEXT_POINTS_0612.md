# X4 resident Points 0.6.12

This is the explicit X4 0.1.51 composition target. It extends the frozen 0.1.50
Points 0.6.11 resident profile; it does not replace the Watch or standalone
development recipes and it is not a release or hardware qualification.

## Selected identity and preserved profile

`Apps/native/points_catalog_x4_resident.json` is the matching source manifest.
`scripts/build_x4_shared_text_points.py` requires the live local 0.6.12 reservation,
compares the original target receipt with the frozen product custody record,
checks its original ELF hash, and preserves all 19 installed defines and all 10
policy grants. It adds only `PORTABLE_TEXT_INPUT_CLIENT` and the single
`ui.text-input@1` instance0 declaration/grant. Native UTC, tagged alarm API2,
storage namespaces, telemetry default-off, return destinations, resident
descriptor and host-owned power policy remain selected. There is no local
keyboard fallback and no application grant for scene, profile or raw USB HID.

The exact target uses Runtime SDK source
`615fb236b591bc6974a35ae23c7b2b785c0a5016` and frozen Utilities source
`bda1c2ec01d2c18e39f822fbcf6cc8ac3b12aa9e`. The builder independently records
the committed System dependency, all staged SDK and compiled dependency hashes,
the baseline receipt/hash, complete command, import/export sets and stack use.
The existing GCC8.4 2021r2-patch5 compiler is invoked directly; PlatformIO does not
run. Selected dependency sources must be clean and exact.

## Qualification

`scripts/test_x4_shared_text_points.py --build TARGET --utilities UTILITIES
--output EVIDENCE` compiles the real controller, app entrypoint, and System
adapter with every target define. It runs normal and ASan/UBSan cases for
storage custody, native-time/tagged-service behavior, telemetry policy,
draft/uncertain-save guards, resident checkpoints, clean Home, input coordinates,
async frames, shared host acceptance/cancellation/unavailability, pending close,
alarm cancellation, active-session fini, and Runtime loss at acquire, open,
poll, close and release. Six added actual `app_main` cases each invoke the app
twice and assert host close/grant cleanup before exit. Home remains blocked while
a type/event draft is open; cancellation restores the clean resident return.

The existing selected Watch/X4 catalog suite runs separately as a regression
check with the real pinned Watch adapter. The resident wrapper forwards current
shared-host scenarios instead of obsolete local-keyboard cases.

`scripts/check_x4_shared_text_target.py` reads actual ELF32 bytes, checks the
unique 16-byte foreground descriptor `(1,16,2,0)`, verifies exactly four app
exports, rejects local-keyboard/shared-control symbols, and resolves every
target import through the exact native firmware export tables. It checks native
ELF and firmware SHA256 against the independently verified binary export
evidence. This is structural/ABI evidence, not execution of Xtensa instructions.

## Limits and composition obligations

- Capability providers and resident Runtime dispatch are deterministic host
  fixtures, not physical e-ink, touch, flash, power or keyboard hardware.
- LeakSanitizer is disabled for executor ptrace compatibility; ASan and UBSan
  remain enabled. Retained invocations intentionally preserve their allocations.
- The alarm entrypoint fixture qualifies text cancellation and cleanup; it does
  not qualify the complete alarm foreground renderer.
- Product assembly still must add the three ordinary shared text/scene/profile
  providers, retain the full original graph and boot grants, perform exact
  native admission and store round trips, and qualify physical behavior.
- The frozen .50 graph has no real USB keyboard-input provider. This target does
  not create hardware keyboard discovery or claim hardware qualification.
- Abandoned sessions remain retained; the client does not invent automatic
  provider-owned app-exit cleanup.

Full installed stage logging exposed a terminal-custody integration defect:
System text-adapter retention must use the existing silent native retention
helper after liveness loss. The ordinary development profile omitted stage logs
and therefore did not expose that call. The final dependency pin must include
that narrow correction; the terminal no-I/O assertions stay strict.

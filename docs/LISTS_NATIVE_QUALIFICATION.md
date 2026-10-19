# Lists native software qualification

This qualification exercises the shipping semantic Lists application on the
shared `ui.scene` presenter at 160×160, 240×240, 320×180, 410×502 and 502×410.
It produces no product firmware recipe, install image, release or physical-device
claim. Existing Watch/X4 deployment files, app manifest/version and all Lists
model/storage/application sources are preserved by SHA-256 checks.

## Source and package closure

`profiles/native-lists-qualification.json` pins clean Runtime and System commits
and trees. It also records preserved application/model/storage/deployment hashes,
the five sizes, the exact SDK 6.1 compiler and `color-dynamic` scene-host profile.
The native application remains semantic and dimension-free. The shipped text
surface is the shared presenter's embedded `SceneKeyboardV1`; this work adds no
new `ui.text-input` dependency. Required capabilities remain `ui.scene@1`,
`storage.app-data@1`, and `time.civil@1`.

The builder checks exact commits/trees, rejects modified tracked dependencies,
verifies the ESP32-S3 little-endian frontend and pinned compiler, rebuilds Lists
in a second directory for ELF equality, and checks loader structure/imports/
exports for Lists, the scene host, both external presentation profiles and both
civil-clock providers. These are qualification artifacts; no product catalog,
firmware or release is changed.

## Reproduced and corrected defects

The System presenter correction is limited to its shared components renderer:

- A fixed-width confirmation/reminder dialog clipped outside 160-pixel displays;
  the alert also clipped at 410 pixels. Dialog width is now bounded by the
  display width, retaining the existing inner controls and exterior cancellation.
- Color keyboard title/field used fixed scaled vertical coordinates while keys
  already used display-height proportions. This overlapped the field and first
  key row at 160×160, 320×180 and 502×410. Title, cancel and field now use the
  same height proportions. Text reducers, key ordering, limits and key events
  are unchanged.
- Lists' standalone builder did not expose the selected System script directory
  when dynamically importing its builder, so current System's sibling
  `native_toolchain` import failed. The import helper now scopes sibling lookup
  to the selected directory and restores the caller's path afterwards.

## Reproduction

Run from the Productivity repository, with sibling `runtime` and `system`
checkouts at the profile pins. Choose a fresh target output directory.

```sh
python -m unittest discover -s tests -v
python scripts/test_lists_native.py --runtime ../runtime --system-apps ../system --output ../evidence/renderer --sanitize
python scripts/test_lists_native_app.py --runtime ../runtime --system-apps ../system --output ../evidence/app-normal
python scripts/test_lists_native_app.py --runtime ../runtime --system-apps ../system --output ../evidence/app-sanitized --sanitize
python scripts/test_lists_scene.py --runtime ../runtime --system-apps ../system --frames ../evidence/legacy
python scripts/build_lists_native_qualification.py --runtime ../runtime --system-apps ../system --output ../evidence/target
```

Export SDK 6.1's compiler or set `NATIVE_APP_CC` to its
`xtensa-esp32s3-elf-gcc` frontend. CI installs the exact official compiler archive
with a SHA-256 check and remains read-only; it has no publication job.

## Verified scope

- 150 renderer cases, including normal and ASan/UBSan, with matching frame hashes:
  real touch targets, completion/detail separation, all four key layers, 24-byte
  edit limit, pending frames, scroll/focus, modal cancellation, checkpoint/reopen
  and nine native-loss boundaries. Pixel checks independently separate title,
  value and key regions and reject modal edge clipping.
- 370 actual app/host cases per mode: real-touch CRUD workflow, task movement,
  due/time/priority/reminder/repeat edits, recurrence recheck without duplication,
  fresh storage, retry/corruption, stale/uncertain writes, cancellation, native
  exit, resident draft recovery, close/release faults and terminal provider/native
  losses. Both explicit retained errors and context loss with a success status
  are checked. Storage changes are never blindly retried.
- 24 previous Lists Watch/paper raster captures remain byte-identical.
- 197 System presenter/profile regression executions per mode.
- Contexts controls: 985 native cases per mode pass against the changed shared
  presenter, with identical normal/sanitized results. Compared with its previous
  qualification, 1,264 frame/storage hashes are unchanged; 36 keyboard/dialog
  captures change on the corrected non-240 color geometries. No saved Contexts
  bytes change. Shared-input versus embedded-input parity assertions pass.
- 788 Contexts legacy Watch/paper cases (normal and ASan/UBSan) pass; the
  legacy shared-input versus embedded-input frame/storage checks remain equal.

LeakSanitizer is disabled in the ptrace-based local executor; AddressSanitizer
and UndefinedBehaviorSanitizer remain enabled. These results do not qualify
physical touch behavior, display timing, persistent hardware media or firmware
composition. Local evidence precedes publication; remote CI must run against the
final reviewed remote dependency pin before delivery is called CI-qualified.

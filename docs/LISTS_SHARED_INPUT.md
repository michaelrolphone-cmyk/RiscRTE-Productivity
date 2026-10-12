# Lists shared text input: Watch and X4

Lists keeps its existing NOVA scene reducer, list/task model, storage protocol,
fonts and device layouts. The selected development builds use the existing
`ui.text-input@1` instance 0 for custom list/task names and renamed list/task
names. The app declares semantic content; it has no display calls, device
coordinates, font files, HID subscription or keyboard implementation in the
shared-input build.

## Scope and compatibility

- Names remain limited to 24 printable ASCII characters, with a 25-byte request
  capacity including the terminator. The existing reducer trims/normalizes
  names to uppercase and applies the same empty/duplicate-name checks.
- The same source serves Watch compact color and X4 portrait monochrome. The
  existing separately loaded scene host and presentation profile retain all
  fonts, non-editor layouts, gestures, continuous scroll and time controls.
- The installed text host supplies its established keyboard styling and
  editing/layer controls. That shared editor is the only keyboard surface;
  non-keyboard scene parity is checked separately rather than claiming the
  predecessor component editor has identical pixels.
- Save occurs only after text close, capability release and scene resumption.
  Cancel returns to the prior route without writing. Invalid names keep the
  copied draft and offer Edit Name/Cancel using ordinary scene controls.
- Missing or busy input services show `TEXT INPUT UNAVAILABLE / RETRY`; Edit
  Name retries the same draft. No app-local keyboard fallback is linked.
- Polling text input does not wait for a new display frame. Text/session cleanup
  still waits for the host to relinquish its submitted frame. The civil clock
  continues its normal cadence while the editor is active.
- Shared-host Home and native Home cancel uncommitted input. Home observed
  during acceptance, close or release prevents the draft from being saved and
  skips editor restoration. The normal app cleanup closes any owned text/scene
  sessions and releases grants. Returning resumes the existing resident owner
  where applicable; no new launcher or shell renderer is added.
- While an editor is open, the optional shared-host checkpoint suffix preserves
  the original resident low-battery/idle-Light cadence. Activity and held input
  remain distinct from display state. POLICY runs after display custody settles;
  an explicit native no-sleep promise lets routine policy retain bounded passive
  input queues and navigation claims. Otherwise it fully detaches before native
  touch-power preparation. Both paths preserve draft/layer/caps. Old owners and
  attempted sleep retain historical wake-input neutralization. A new key racing pause first refreshes resident activity; Home or
  policy EXIT closes the paused editor without saving or reopening it.
- Terminal retention or lost Runtime custody forbids further callbacks. Pending
  close keeps the exact session and grant; bounded failed cleanup retains the
  invocation rather than pretending an owned surface was released.

## Existing features and data

The list/task reducer and storage implementation are unchanged: Today/All Tasks,
12 lists/64 tasks, suggested names, markers, OPEN/ALL filters, completed-item
visibility, task moves, priority, dates/time, foreground reminders, snooze,
repeat rules, deletion confirmation and Watch undo retain their behavior.
Quick Actions still use the original non-keyboard scene
close/checkpoint/reopen path. Keyboard policy uses a host-owned pause/resume
boundary; the old component keyboard already suppressed the top-edge controls
gesture. Missing checkpoint support fails clearly through Edit Name/Cancel.

Each device retains its Lists app-data namespace: Watch 62, X4 63. The filename
`lists.bin`, wire format, CRC, revision checks, atomic replacement and
STALE/COMMIT_UNKNOWN reread behavior are unchanged. Existing unrelated files,
app grants and unrelated providers/SD files are preserved by product composition.
X4 Home receives only the pre-policy no-sleep promise needed to keep routine input
attached while allowing actual sleep; its exact selected profile and boot/wake fixes
are preserved. Watch has no resident owner selected and its native Clock is unchanged. No saved data is seeded, cleared or migrated.

## Reproduction

Shared input is an explicit selected build option. The generic manifest and
flag-off build remain unchanged. Product recipes preserve each device's
selected manifest fields and add only the text requirement and version 0.1.2.

```sh
python scripts/build_lists_scene.py \
  --runtime ../Runtime --system-apps ../System \
  --shared-text-input --output ../build/lists-shared
python scripts/test_lists_shared_input.py \
  --runtime ../WatchRuntime --x4-runtime ../X4Runtime --system-apps ../System \
  --baseline-productivity ../PriorProductivity \
  --output ../build/lists-shared-evidence --sanitize
```

The build uses System commit `f4b30f8ebcc6d8cb905c8e90c91b1691acbe8165` and
checks deterministic target output. The selected build freezes the existing
resident bridge from System `eec748ca081fe59346be03180600c65013cc7298` in
`sdk/lists-selected-profile/`, retaining its exact helper implementation. With
that header, the shared-build SDK compiled without the new flag reproduces the
selected predecessor raw ELF and Watch-compacted ELF exactly. Device recipe
receipts name exact Runtime
and Productivity source commits, target dependencies, admitted full cohorts and
store capacity. Existing flag-off Lists tests remain a separate regression.
Source CI runs normal and ASan/UBSan shared-host tests with platform-default
leak detection. Local ptrace execution disables LeakSanitizer only through the
invoking environment, not the published test runner.

## Qualification boundaries

Host tests execute the real scene/text hosts and application workflow using
transport/storage doubles. Target validation and complete product admission
check target structure, imports, policy and composition; they do not execute
Xtensa instructions or establish physical Watch/X4 behavior. Retained subscriptions
preserve delivered events within bounded queues. The selected polled touch/button
drivers cannot reconstruct a physical tap completed wholly between polls; queue
overflow/GAP follows existing fail-closed recovery rather than inventing input. Static frame size
inspection does not prove maximum dynamic call-chain stack use.

Generated evidence uses hashes and compact fixtures. Full flash images are not
required for composition; full-initial flashing would erase internal saved data.
No device was flashed. Binary delivery remains held until 2026-10-12 23:30 UTC.

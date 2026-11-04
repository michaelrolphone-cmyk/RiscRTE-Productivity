# Shared Timecard 0.3.1

Timecard now has one semantic scene application, `Apps/timecard_scene.c`.
The application has no display dimensions, device-class branches, pixel drawing,
touch coordinates or presenter selection. The shared scene provider renders its
declarations at the device's native resolution. The same Xtensa ELF is used for
Watch, e-ink and round-badge/color presentation.

## Preserved behavior and saved data

The scene application includes the existing `timecard.c` calendar, punch,
worked-time calculation, week labels and JSON implementation. The unused legacy entry is internal to this translation unit and removed by
the optimizer; the original source file remains byte-for-byte unchanged. There is no copied calendar or second file schema.

All 20 recent weeks, seven days, four punches, current-time punch actions,
manual time entry, blank-to-clear, AM/PM and 24-hour entry, lunch-overlap work
totals, shared 12/24-hour display preference and nested Back are retained.
Input goes through the shared keyboard and revision/sequence/node/action fences.

The existing strict validator rejects invalid, duplicate, oversized and truncated
history. The existing app-data CAS bridge retains `timecard.json` and its original
schema. There is no migration, formatting or data reset. A 401st day is rejected
without evicting history; clearing a day's last punch frees its slot.
Uncertain saves restore the displayed RAM snapshot, block editing and require a
complete reload. This is not a claim that an uncertain durable write rolled back.

Missing storage or civil time is shown honestly. Acquired grants are selected
through the boot graph, not hardware identity. Storage and time-format namespaces
must be bound to the existing Timecard history and shared settings respectively;
the app does not select provider IDs. Provider custody loss is terminal and
forbids subsequent I/O or cleanup. Shared controls and resident policy preserve
the document/navigation; editing and unresolved storage inhibit idle/policy.

## Build and verification

```sh
python scripts/timecard_scene.py --runtime /exact/Runtime \
  --system-apps /exact/System --build --frames dist/timecard-frames
```

The pinned CI workflow performs normal, UBSan and ASan tests, actual shared-provider
rendering at 240x240, 800x480, 410x502 and 466x466, repeated byte-identical target
builds, and production ELF structural validation. The renderer fixtures check
frame boundaries and session cleanup. A square 466x466 fixture is a native
viewport test, not evidence of a physical round display's mask or input geometry.

Tests cover editing, cancellation, malformed/stale/forged events, invalid time,
400/401-day preservation, clearing capacity, uncertain writes with reread,
unavailable dependencies and loss of storage/scene custody. Darwin runs normal
and UBSan tests: its installed ASan runtime hung in dyld initialization before
main; the Linux test run supplies ASan evidence.

Application publication does not by itself update a product boot store or claim
physical acceptance. Each product must bind its existing app-data and settings
namespaces, ui.scene and time.civil providers, and qualify its resulting cohort.

The app selects existing Timecard app-data and preference namespace 1 explicitly.
This avoids ambiguous preference lookup when a product also grants context
namespace 3. Only ui.scene is required for launch; missing data/time/preferences
remain visible optional-feature states. Legacy regressions use their owning
workflow pins, not the newer shared-scene adapter.

## Preserving product candidates

The assembly tool replaces only the Timecard image and sidecar in the pinned
published Points cohort. All unrelated provider/app bytes, native firmware
metadata, saved data, preference namespaces and storage representations remain
unchanged. Watch and Waveshare 2.06 activate their already-published, identical
shared Home resident host for Timecard; all other apps retain ordinary legacy
handoff. X4 retains its existing resident admission.

The host is verified by its exact ELF digest, HOST descriptor and the policy
capabilities delegated by the original application. The same Timecard ELF is
installed internally or on the existing authorized SD route. Content-addressed
routes are updated without leaving the old executable selected.

Assembly rejects ambiguous identities/providers, wrong host bytes, unreviewed
authority removal, missing data namespaces, changed sidecars and unsafe paths.
Positive/negative tests cover all three real product baselines. Owning Runtime
graph/ELF admission is tested normally and under ASan/UBSan. These are development
candidates, not evidence of physical installation, memory fit, sleep behavior,
or round 1.75C hardware qualification.

The qualification command uses System composition/admission helpers at
77c427a75e5651f03119c059fcdfd36d23df8708. The workflow records exact native product
and Runtime pins and publishes both full candidate stores and SD companions.

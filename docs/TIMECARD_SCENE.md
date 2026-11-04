# Shared Timecard 0.3.0

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

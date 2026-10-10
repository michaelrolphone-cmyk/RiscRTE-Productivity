# Selected X4 resident Productivity

The opt-in `scripts/build_x4_resident_clients.py --resident-shell-client` uses the qualified Utilities build helper specified by `--utilities`. It builds Points 0.6.10 from the storage-scaled `Apps/points_catalog_app.c` editor and Timecard 0.2.10 from the authoritative civil model through `Apps/timecard_portable.c`. Both use the clean shared System resident client with host-owned idle policy and no Quick renderer or independent sleep helper.

The source ancestor is `cf1f63ae12c47481158e8bf6e6fa42f5e27e48c1`. Native UTC/API2, unpadded hours, Points event/type symbols and catalog storage, complete-frame input identity, and Timecard paper scrolling/civil history are retained. Resident-only launch guards prevent the shared controls from discarding open edits or ambiguous saves. Host policy can preserve those drafts in the live foreground stack. Clean global Home returns to the host; app-owned Back keeps the Runtime-admitted pending launch and normal cleanup.

Provide `--utilities`, `--system-apps`, `--runtime`, `--display-sdk`, `--output` and the pinned compiler via `NATIVE_APP_CC`. The Utilities helper supplies the exact System/Runtime pins. Default Productivity builders/manifests and all flag-off application behavior remain unchanged.

`test_resident_clients.py --build OUTPUT --utilities UTILITIES [--sanitize]` reuses exact target build defines and real production adapter/controller sources with deterministic provider fixtures. It covers open draft and uncertain-save refusal, clean Home, host policy preserving drafts, terminal dispatch fences, 39-event Points editing/reentry/keyboard/frame/storage tests, and Timecard civil/DST/400-day/storage failure cases. The shared Utilities `test_resident_flag_off.py` proves both nonresident target ELFs remain byte-identical to the ancestor.

Artifacts are local-only and not hardware-qualified. No publication, installation, USB transport, or GameBoy changes are included.

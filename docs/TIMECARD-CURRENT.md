# Explicit current Badge Timecard profile

The historical native-time 0.2.9 and native UI 0.3.0 profiles remain unchanged.
The current 0.3.1 builder uses the original civil ledger/model and checked AppData
bridge, not a replacement model or storage migration. `Apps/timecard.c` remains
SHA256 c17b0fed28eb44d2397c740ef56c8f07bc51c3f707d3080ec9ad5849e25a31a9.
History remains civil dates/minutes: four punches, 400-day capacity, twenty weeks.
Done writes explicitly; Back cancels an editor; root Back returns to
`springboard.elf`, while physical Home routes `default.elf`.

## Current dependency contracts

The builder consumes the canonical Runtime273 SDK, the tagged Alarm2 descriptor
SDK, current System native scene/Quick composition, and the existing returning
foreground Light/alarm helper. It adds no Runtime ABI or native provider.
The current profile requires distinct capabilities at instance1:

- `storage.key-value`: existing time-format reads, no Timecard format writes.
- `storage.app-data`: private `timecard.json`, maximum49151 bytes, checked revision.
- `storage.preferences`: existing copied edit API for explicit shared Quick writes.

No `runtime.realtime.control` or `net.wifi` grant is added. The established Quick
Wi-Fi control is **WI-FI SETUP**, a launch of `wifi_settings.elf`, not a claimed
radio toggle. Shared Quick still owns brightness/DND/torch and supported controls;
ordinary dismissal preserves the app's draft. Accepted external navigation exits
without silently saving a Timecard draft. Home's radio Quick remains separate.
The source receipt identifies descriptor-feature recognition separately from
clock-edit authority (false). Target selection must validate the exact receipt.

## Busy and uncertainty

The current synchronous AppData API's BUSY occurs before the requested I/O;
ordinary failed reads preserve the original transactional parser's prior history.
A successful replacement followed by unavailable/BUSY verification remains
COMMIT_UNKNOWN. The displayed model rolls back only as the original controller
already did; this does not claim the backing write was undone. Explicit Retry
reloads authority before further edits. Unknown/retained results stop callbacks.

A temporary BUSY/IO time-format read never substitutes an unconfirmed12h value
for a known format. Initial unavailable format shows an explicit pending screen
and bounded retry, without writing preference defaults or allowing a misleading
mutation. Genuine absent/malformed preference retains the established12h fallback.

## Presentation and qualification boundary

The old eight-hook source facade binds to the reusable current native scene gate.
Changed maps, Light, alarm and Quick restoration require a matching completed app
frame and fresh input authority; no snapshot is attempted during a known failed
poll. Quick uses full-panel input before app-safe-content transforms, and native
safe-content drawn/hit geometry. Snapshot rendering remains unselected; historical
optional recipes are preserved. This does not claim compatibility with their
frozen binary/SDK hash checks.

New owning tests are separate from frozen validators: controller/ledger/provider
status tests, selector-off preprocessed C token parity, and actual shared-adapter
current native UI composition. Provider doubles and modeled raster tests are not
actual Runtime, target loader, radio, device or final product qualification.

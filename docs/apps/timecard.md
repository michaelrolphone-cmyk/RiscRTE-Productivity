# Timecard

## Purpose

Timecard is a local weekly time-tracking application. Its manifest identifies `timecard.elf`, version **1.0.0**, minimum firmware **1.1.8**, categories `Productivity` and `Time`.

It stores clock-in, lunch-start, lunch-end, and clock-out punches and computes worked time from those punches.

## RiscRTE interfaces

The app uses:

- `T5AppApi`
- `T5StorageApi`
- `T5SystemApi`
- `T5SystemUiApi`
- `T5UiApi`

It verifies API versions and `struct_size` before use.

The system clock comes from `T5SystemApi::local_datetime`; the app does not own timezone conversion.

`T5SystemUiApi` is used for firmware-owned keyboard entry and navigation back to Home.

Shared UI rendering uses list and table renderers, hit testing, event polling, and index helpers.

## Data model

Each stored day contains:

- Date as integer `YYYYMMDD`
- In
- Lunch Start
- Lunch End
- Out

Punch values are minutes from midnight; an unset punch is represented internally as a negative value.

The implementation holds at most **400 days** in memory.

## Persistence

Data is stored at:

`/sd/.crosspoint/timecard.json`

The current JSON shape is:

```json
{"days":[{"d":20260926,"in":480,"ls":720,"le":750,"out":1020}]}
```

Only punch fields that are set are emitted. Storage uses `write_file_atomic`.

The in-memory JSON working buffer is 49,152 bytes. Invalid/unreadable stored data causes the load operation to fail rather than being silently reinterpreted.

## Screens and workflow

The app has three screen states:

1. **Week list** — shows up to 20 weeks, beginning with the current week.
2. **Week** — shows seven day rows plus four punch-action rows.
3. **Day** — shows the four punches for one selected date.

From the week screen, selecting a day opens its punches. Selecting one of the punch-action rows records the current local time for today's corresponding punch.

The day screen lets the user edit an individual punch through the firmware keyboard.

## Keyboard handoff

Editing a punch calls `keyboard_request` with the current formatted time and an encoded cookie containing date, punch index, and week offset. Because the firmware keyboard is a separate system workflow, the app can return and later consume the result with `keyboard_take_result`.

The cookie includes a magic byte so unrelated/stale keyboard results can be rejected.

Accepted time input supports 24-hour values or AM/PM notation. Empty input clears a punch.

## Worked-time calculation

Worked time is calculated as Out minus In. If a valid lunch start/end pair is present, lunch duration is subtracted.

If In/Out are incomplete or invalid, worked time is left unavailable. Negative results are clamped to zero.

## Navigation

Back from Day returns to Week. Back from Week returns to Week List. Back from Week List restores normal Back behavior and requests navigation Home.

Touch uses the shared UI hit-test API. Confirm activates the selected row; Previous/Next use shared selection helpers.

## Source

- `Apps/timecard.c`
- `Apps/timecard.json`

# Timecard shared text input

The opt-in `PORTABLE_TIMECARD_SHARED_TEXT_INPUT` profile migrates the four Timecard time fields to the existing `ui.text-input@1` host. It requires `PORTABLE_TEXT_INPUT_CLIENT` and `PORTABLE_APP_HOME_GUARD`. Generic and legacy Timecard profiles retain their original local editors.

## Editing contract

- Clock In, Lunch Start, Lunch End, and Clock Out retain their twelve-character limit. The copied request capacity is thirteen bytes, including NUL.
- Done retains Timecard's original direct-save operation: validate the cookie and time, reread the authoritative ledger, and mutate only the selected punch. No record is changed before the host confirms close, the capability is released, and the app presenter resumes.
- Empty text retains the original clear-punch operation. Existing AM/PM and 24-hour parsing and display remain unchanged.
- Invalid time and clean storage failures reopen the shared editor with the same draft and the existing error. If the host is unavailable, the app displays the preserved draft and error with Cancel and Retry. It never handles local character entry in this profile.
- Back/Escape cancels the draft. Home cancels the draft, closes and releases the host, and follows the existing product Home route. X4 resumes its existing resident Home owner; Watch requests its Springboard. A refused launch does not save or reload the discarded draft.
- The Timecard application never polls its own input, paints a frame, or writes storage while shared input or close is active. Host input progress does not depend on an app redraw. `AGAIN` retains the current token and grant for a later step.
- Retained or unconfirmed custody stops later provider operations, drawing, launches, and cleanup. Watch checks Contexts cleanup, Broadcast cleanup, each storage callback, optional dependency acquisition, preference reads, and releases individually.

## Preserved data and presentation

`timecard.c`, `timecard_appdata_bridge.h`, `timecard_portable_validation.h`, and `timecard_native_time.h` remain unchanged. The ledger is still civil dates and minutes in app-data namespace 1, using the same `timecard.json` path, JSON schema, revision handling, and atomic replacement. The 400-day safeguards remain in place. This migration does not migrate or rewrite saved data.

Nonkeyboard screens retain their original layout, coordinates, fonts, selections, scrolling, and actions. Watch keeps the delivered NOVA/240-pixel selection and its original icon/font inputs. X4 keeps native time, Paper presentation, scrolling, BLE, resident shell, and raster features. Quick actions, alarms, navigation, and other inherited product features are retained.

## Integration

The product recipes are in the existing Watch and X4 repositories. They compare the actual selected predecessor Timecard payload, retain inherited definitions, verify source/font inputs and imports/exports, then add only the selected Timecard text-input grant. The product versions are Watch 0.2.13 and X4 0.2.16; the generic app manifest is unchanged.

Only component deltas should be applied to the latest authorized complete cohort. Recheck full-cohort packing and admission after any independent changes. The existing shared input/UI hosts and all other app/native bytes must be preserved. The Watch recipe serializes only the already-touched boot catalog compactly and proves that its decoded tree differs solely by the Timecard grant. This formatting-only saving retains six free SPIFFS blocks against the two-block reserve. Full initial flash images can erase internal data and are not a data-preserving update method.

Physical-device behavior is not established by native host fixtures or structural ELF admission. No flashing or delivery is part of these qualification commands. Delivery is held until 2026-10-12 23:30 UTC.

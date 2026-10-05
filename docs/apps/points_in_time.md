# Points in Time 0.2.0

An original shared Productivity app for up to eight recurring daily points:
Work start, Work end, Lunch, Break and Bedtime. This is separate from the existing
Timecard app. Meetings, birthdays and arbitrary appointment categories are outside
this version's scope. No demonstration schedule is installed. The attached NOVA-7 mockup also illustrates custom point kinds and a separate three-minute warning; those controls are intentionally omitted because the current shared 64-byte record and two-edge ledger cannot persist or schedule them.

## Touch workflow on a 240 × 240 display

- The watch-native NOVA-7 view uses a black/cyan 240×240 presentation with a chronological scrolling point list, active/total count, per-type color accents and a `+ ADD POINT` row. Disabled points remain visible but dimmed.
- Tap a point to open a scrolling edit sheet for Type, Time, Duration, Days, Notify, Enabled, Save and Delete. Back from a nested editor returns to the point; Back from the point discards the draft.
- Time and duration use drum-style five-row pickers. Dragging a column or tapping an adjacent row changes it; minutes move in five-minute steps. The list and point summary respect the shared 12-/24-hour preference; missing, invalid or unavailable preference falls back to 12-hour AM/PM.
- Select any Sunday-through-Saturday combination, Every day, Mon-Fri, Weekend, or None. An enabled
  point requires at least one selected weekday. Disabled points may have none.
- Alert modes are System default (0), Vibrate (1), Sound (2), and Sound and vibrate
  (3). Output selection and the global default belong to the alarm service.
- Lunch and Break optionally have a 0–720 minute duration selected from the drum control. Zero means no end alert. Choosing another type clears its draft duration.
- On/Off changes the draft only. Save opens a confirmation explaining the catalog
  reset. Save now commits; Back returns without writing.
- Tap the bottom status line to refresh/retry. Failed dependencies and invalid
  storage have an explicit Retry button. An uncertain save has a dedicated Retry
  save button and locks all editing and normal Back navigation until confirmed.

Empty slots have an all-zero record. `+ ADD POINT` creates only an in-memory draft (Break, next five-minute boundary, weekdays, vibrate, 15-minute duration) until Save is confirmed; it does not write anything before that explicit save. There are no periodic app writes, default repairs or heartbeat saves.

## Important: each save resets the whole catalog

Every explicit save writes a new catalog revision and a fresh raw-RTC creation
boundary, preserving the other seven slot definitions. This intentionally cancels
old catalog pending alerts and derived lunch/break ends. Only starts strictly after
that save are eligible under the new revision.

For example, if a lunch started before the save, its old pending end will not be
carried over, even when editing another slot. This policy appears on every Save
confirmation and is enforced by the shared service. Repeated saves are deliberate
new revisions, including an unchanged point; they are not a no-op optimization.

A selected start is evaluated as civil time under the deployment's selected time
policy (raw RTC wall time by default; an explicit UTC+08-to-America/Denver build is
available). The app's Time editor names that policy. App, service and companion
faces must be built with the same policy. The shared scheduler skips nonexistent
spring-forward and ambiguous fall-back starts rather than silently choosing an
offset. An end is elapsed minutes after its eligible start and keeps that start's
weekday ownership, including when it crosses midnight.

## Durable storage and errors

The app writes only `points_cfg` in its explicit namespace-5 key-value grant. The
Utilities-owned wire format is one versioned, checksummed 64-byte catalog: revision,
raw creation time, eight six-byte slots, checksum. It is independent of the old
Alarm/Countdown namespace-3 records and of Timecard's JSON files.

Save success requires exact readback of the pending bytes. A put error may mean the
new value committed; it does not establish rollback. If readback fails or differs,
the app retains the original pending bytes and offers only Retry save. Retry writes
those identical bytes, with the same revision, creation time and complete catalog.
It never restamps time or accepts another edit while uncertainty remains. A put
error followed by matching readback is a confirmed success. A new invocation reads
the stored catalog; no stronger durability across erase, reflash, or power loss is
promised than the key-value backend supports.

Invalid/unavailable existing data is not silently replaced. Revision exhaustion,
invalid/out-of-range RTC, service unavailable/blocked, and unconfirmed saves are
visible states. Invalid RTC prevents a new catalog revision. Retry asks the service
to reconcile; it does not acknowledge an occurrence. Storage repair is not offered
by this app. The service also guards against observed rollback/reused revisions.

`Catalog saved` confirms persistence only. The existing alarm.service@1 status has
no per-point revision field, so the app does not claim that a particular point is
armed based on the legacy two-entry status array. Service errors remain visible.

## Shared foreground and capability boundaries

Required capabilities are display.output@1, input.touch.raw@1, rtc.clock@2,
storage.key-value@1 and alarm.service@1. Deployment grants storage instances 5
(catalog) and 1 (the shared time_format preference). The preference client only
reads; this is an implementation limit, not a claim of namespace-isolated security.
No app output, provider-storage, occurrence-ledger or hardware control grant is
added. All scheduling, RTC wake selection, output and durable acknowledgment remain
inside the ordinary Utilities service and existing shared integration.

The portable build includes the pinned System adapter with PORTABLE_ALARM_CLIENT.
A deployment may define POINTS_RETURN_APP to choose a root return destination.
It must omit the generic PORTABLE_RETURN_APP for this app: nested Back belongs to
Points, and uncertain-save Back is locked. Only an accepted root Back request queues
the selected launcher, exactly once; a rejected request preserves the view and
private grants so the user can retry.
That adapter owns in-place alerts, settled-frame normal service steps, and bounded
failure-only output stopping. This app does not duplicate step/stop calls in that
build. A retained native-sleep result returns immediately without grant release,
storage/RTC access or normal cleanup. A bare host/native test path independently
exercises at-most-three stop-only attempts and retains resources if output cannot
be proved stopped. It never treats a failed poll as a safe normal-service point.

## Verification and limits

See [build and tests](../POINTS_IN_TIME_BUILD.md). Tests cover production source,
not hardware acceptance. This app has not been released or installed on a device.

## Temporary factory schedule

Only a missing `points_cfg` uses the shared virtual revision-one defaults. A
present catalog, including an intentionally empty one, is never replaced. Invalid
or unavailable storage remains an error. Startup does not write catalog/metadata;
the service continues its ordinary durable occurrence bookkeeping. The first
user catalog edit is revision two and verifies missing default metadata before
saving the catalog; cancelling an edit writes nothing. Existing metadata wins.

Monday–Thursday only, all with sound and vibration (normal volume policy):

- 04:30 Wakeup, point event
- 05:30 Drive to Work, 15 minutes
- 06:00 Work, point event
- 09:00 Break, 15 minutes, warning at 09:12
- 12:00 Lunch, 30 minutes, warning at 12:27
- 14:15 Break, 15 minutes, warning at 14:27
- 16:30 Work End, point event

End notifications remain off. Duration ends remain visible in the schedule faces.
Drive to Work and Wakeup occupy the two default custom types. Metadata still has
64 bytes: existing PTM1 records retain their 12-character layout; PTM2 supports
13-character names, including the exact Drive to Work label. Old PTM1 metadata
remains readable. This does not change flash-storage overwrite behavior.

# Points in Time 0.1.0

An original shared Productivity app for up to eight recurring daily points:
Work start, Work end, Lunch, Break and Bedtime. This is separate from the existing
Timecard app. Meetings, birthdays and arbitrary appointment categories are outside
this version's scope. No demonstration schedule is installed.

## Touch workflow on a 240 × 240 display

- The list has two pages of four numbered slots. Prev/Next changes the page.
- Tap an empty or existing slot. Type, Time, Days, Alert and Duration each open a
  focused editor. Back returns to the point; Back from the point discards the draft.
- Time editing is explicitly labeled 24-hour HH:MM. Hour/minute up/down wraps at
  23/59. The list and point summary respect the shared 12-/24-hour preference;
  missing, invalid or unavailable preference falls back to 12-hour AM/PM.
- Select any Sunday-through-Saturday combination, All, M-F, or Clear. An enabled
  point requires at least one selected weekday. Disabled points may have none.
- Alert modes are System default (0), Vibrate (1), Sound (2), and Sound and vibrate
  (3). Output selection and the global default belong to the alarm service.
- Lunch and Break optionally have a 0–720 minute duration, with one-hour, five-minute and
  one-minute controls. Zero means no end alert. Choosing another type clears its draft duration.
- On/Off changes the draft only. Save opens a confirmation explaining the catalog
  reset. Save now commits; Back returns without writing.
- Tap the bottom status line to refresh/retry. Failed dependencies and invalid
  storage have an explicit Retry button. An uncertain save has a dedicated Retry
  save button and locks all editing and normal Back navigation until confirmed.

Empty slots have an all-zero record. Opening an empty editor merely chooses Work
start as an unsaved type. It does not enable the point, choose weekdays, or write
anything. There are no periodic app writes, default repairs or heartbeat saves.

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

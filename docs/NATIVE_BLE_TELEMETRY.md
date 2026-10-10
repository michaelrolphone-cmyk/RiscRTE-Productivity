# Native Points and Timecard telemetry selection

Add `--ble-broadcast` to the existing native builder with `--paper-transitions`
and `--motion-system` pointing to the exact source in
`sdk/native-broadcast-sources.json`. The selected packages are Points 0.6.4 and
Timecard 0.2.4. Existing native, paper motion and Watch selections keep their
previous versions and preprocessor paths.

Both apps declare telemetry.broadcast API1, instance0, alongside their existing
namespace1 preferences. The accepted checked broadcast record defaults OFF for
X4; Bluetooth-off and airplane mode still override saved broadcast intent.
Runtime owns no app policy. The selected adapter borrows and releases grants
synchronously and leaves timer-only Clock behavior outside these applications.

Timecard binds its existing native AppData fence through the shared
PortableBroadcastAppData wrapper. Advertising is paused before stat/read/replace;
a pause or grant-release failure prevents storage calls and retains the same
invocation. Its civil dates, namespace1 history, revision checks and bytes are
unchanged. Timecard preference reads and Points private KV reads/writes also
pause advertising before a possibly retaining external call. No normal cleanup
or further radio/storage I/O follows retained custody.

The actual controller/adapter fixtures exercise the native UTC and API2 source,
Quick Controls motion, drafts, Home, handoff and retained faults. Explicit cases
force active advertising before a Timecard mutation and Points save, then check
pause ordering, failed pause, failed grant release and retained storage. Targets
are checked with pinned GCC8, exact imports/exports and the real ELF validator.
Radio hardware and physical advertising remain unqualified.

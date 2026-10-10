# Selected catalog naming client

Points' selected native catalog manifests require `ui.text-input@1` exactly
once. The product must compose and admit its host before selecting these apps.
If the admitted provider is unavailable or busy, the type draft stays intact
and the app reports unavailability; there
is no application keyboard or USB HID fallback. Legacy unselected apps are not
changed by this migration.

`PortableTextInputClient.h` copies a label, initial plain text and capacity into
the System host. The app owns type-name validation and persistence. Accepted
text only updates its type draft after host close succeeds and the capability
grant releases cleanly; Save Type still owns the storage write. Cancel keeps
the original domain draft. The app-specific symbol picker is on Type Style.

The selected adapter drains any asynchronous e-paper presentation and releases
an outstanding writable surface before host open. It parks touch/navigation,
suppresses ordinary app poll/present/acquire, and restores fresh neutral input
only after close and grant release. Pending close retries only that operation
with yields; app drawing, storage, navigation and accepted-result application
stay stopped. Settled host frames permit existing alarm reconciliation; an
alarm cancels naming, closes the host, and returns to normal alarm foreground.

## Abandonment and retained ownership

There is deliberately no automatic app-exit cleanup. The app launch guard
refuses handoff during naming. If module fini is reached while the host is still
active or its close is pending, the adapter retains the invocation and leaves
the grant/session pinned. It never guesses that borrowed host state is safe to
unmap. Runtime provider quiesce remains the final unload boundary. A retained
result or lost native Runtime liveness also stops all later I/O, even if the
service returned apparent success. The caller never releases or resumes after
uncertain cleanup.

## Qualification

`scripts/test_points_catalog.py` links the actual selected Watch/X4 adapter to
fake capability providers. It covers accepted/cancelled copied results, invalid
empty domain names, missing provider, pending close, borrowed presenter exclusion,
neutral navigation restore, pending async frames, settled-frame alarm cancellation,
active-session fini, and native Runtime loss at acquire/open/poll/close/release.
Existing catalog/controller/background/storage-retention tests remain enabled.
X4 runs can assert low-latency e-paper intent with `--expect-fast-paper`.

These are source and development-target checks, not device qualification or a
published product activation. Release dependency pins must name committed
System/Watch revisions containing the new client adapter and host contract.

# Watch Points custom-type input

The explicitly selected WATCH_POINTS_SHARED_INPUT profile replaces only the
Watch NOVA custom-type name keyboard with ui.text-input API1 plaintext input.
Generic/Paper/other device selections retain their existing implementation.

The request copies the 13-character name and label into the existing shared
host. No application pointers, callbacks or storage APIs cross the boundary.
DONE updates the custom-type draft only after confirmed modal close and grant
release. Cancel and crown Back preserve the previous name/color and point
schedule draft. Home closes first, then cleanly returns for Runtime to restore
the configured default. The existing custom SAVE and point SAVE remain the
only corresponding persistence operations. Name, color, second custom type,
all eight point records, scheduling, alert modes, end notification and warning
bits are preserved. An unavailable host leaves the custom editor usable and
shows a notice; no local keyboard fallback is silently substituted.

The main loop services the shared host without app polling or rendering while
its presenter owns the modal. Capture is independent of host presentation.
The app checks terminal custody before any subsequent work. Its metadata and
schedule storage boundaries prevent post-revocation readback and cleanup.

Run scripts/test_watch_shared_text_points.py with pinned System and Utilities
sources for normal and ASan/UBSan controller tests. Run the existing
scripts/test_points_in_time.py for the unchanged generic profiles. The Watch
product repository additionally replays the exact deployed compile profile,
real app entrypoint, shared adapter, scene host and text host with copied
hardware providers, and performs full-cohort admission. Host-source tests do
not execute Xtensa instructions or substitute for physical-device testing.

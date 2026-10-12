# Watch Points modal policy

`WATCH_POINTS_SHARED_INPUT` + `PORTABLE_TEXT_INPUT_POLICY` opts the Watch custom-name editor into the shared checkpoint bridge. The selected Watch Points 0.6.13 editor stopped calling the ordinary adapter poll, losing its predecessor's elapsed-time battery, idle-sleep and eligible Contexts/broadcast progression. The new client retains the existing shared keyboard, 13-byte bounded name, custom draft, separate Save and all schedule controls.

The editor snapshots logical activity before preparing policy, latches the selected continuity mode across pending pause, and calls policy only after the host has settled. Routine local battery work preserves passive input. Possible sleep fully detaches first. A pending Runtime exit closes the editor and exits directly without redrawing or issuing the app's root-Back springboard request. Alarm attention remains a cancellation, rather than being mislabeled as Home. Failed resume retains the invocation; it cannot dispatch policy twice.

Absent, malformed or unavailable checkpoint support closes cleanly without committing metadata. The ordinary flag-disabled editor still builds. The existing shared host owns touch, navigation and hardware-keyboard selection; the app does not acquire a competing input provider.

## Reproduction

Run `scripts/test_points_policy.py` with `--system-apps`, `--utilities`, `--runtime`, `--watch`, and a fresh `--output`. Exact dependency pins are checked by the script and CI. `--no-leak-checks` is required only in ptrace environments where LeakSanitizer cannot run; address and undefined-behavior checks remain enabled.

The host runner compiles the production Points app, PortableApps adapter, scene host, text host and presentation profile with the selected Watch feature defines. It uses synthetic display/touch/navigation, native sleep, RTC/storage/radio boundaries, and bounded Contexts/broadcast fixtures. The generated catalog and feature profile preserve the selected target inputs. Target-only `-flto` and `-mno-target-align` are omitted for the host compiler. The selected alarm compatibility header is reproduced with an explicit Utilities include.

The 35 original workflows preserve names, maximum/empty/delete behavior, draft-only Done, separate Save, color selection, repeated reopening, all eight schedules, independent warning/end flags, root/nested Back, Home, rapid queued touch edges, delayed presentation, unsupported displays, retained cleanup and runtime-loss boundaries. Eleven additional workflows cover 65-second idle progression, low battery, pending/busy pause, retained pause, retained/AGAIN resume, paused Runtime Home, and missing/malformed/unavailable checkpoint support. Normal and ASan/UBSan frames are hash-compared, then losslessly gzip-preserved. The legacy model and controller suites remain separate gates.

The paused Runtime Home fixture checks no destination request, no app redraw after exit, no metadata write and complete cleanup. The 65-second idle fixture requires 13 battery samples, one sleep with no input subscription or raster lease, and eligible background progression. Low battery must apply its crossing exactly once. Both non-OK resume variants require retention without a second battery dispatch.

## Scope

This patch is for a later Watch build, after the sealed Files-based test candidate. It does not rebuild the separately selected X4 Points 0.6.17 artifact, whose exact source attribution remains unresolved. Product packaging, font/grant preservation, flag-off predecessor comparison and the two-free-block store reserve are qualified in the Watch component recipe. No physical hardware or target instruction execution is claimed by these host tests.

The generic resident-policy plus Contexts-message combination remains a separately documented helper limitation; this Watch profile uses local policy, not a resident shell.

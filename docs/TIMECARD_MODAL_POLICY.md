# Timecard modal power and background policy

Selected successor versions: Watch 0.2.14 and X4 0.2.17. This correction retains the shared keyboard and the existing Timecard model, validation, native-time sampling, storage, Save, Cancel, and Home paths.

## Regression reproduced

The predecessor local editor returned through the ordinary PortableApps poll while a draft was open. The first shared editor waited inside the shared host without dispatching the adapter's power policy. On the selected Watch profile, a 90-second held draft lost the idle-sleep request and periodic low-battery samples. The selected X4 profile lost resident POLL/POLICY progress. Watch also enables Contexts and BLE broadcast; their original model/capture progression must continue independently of editor input.

## Correction

`PORTABLE_TEXT_INPUT_POLICY` is opt-in. A settled shared-host checkpoint drives adapter background work and decides whether a power-policy boundary is due. Routine checks keep passive touch, USB and navigation attached only after the exact resident continuity promise, or a Watch battery-only decision. A possible sleep or Contexts workflow message fully detaches the host first. A pause that returns BUSY requires fresh activity polling; AGAIN keeps its chosen mode. Resume failure retains custody and never dispatches policy twice. The keyboard draft survives policy and workflow-message dismissal; no policy dispatch saves it.

Contexts capture checkpoints bracket host polling. Broadcast and Contexts model work runs when the scene is settled, including queued-input cases. A Contexts foreground message borrows input only while the host is fully detached and returns it before resuming the same draft. Existing active capture continues to inhibit idle sleep; low-battery checks remain active.

X4 clean EXIT closes the paused host rather than resuming it. Unexpected retention, malformed replies, and a claimed no-sleep POLICY returning REDRAW fail closed. REDRAW represents an attempted sleep and is not proof that physical sleep occurred. Raw physical taps are sampled and are not guaranteed lossless at every focus boundary.

## Qualification

Run `scripts/test_timecard_shared_input.py` and `scripts/test_timecard_policy.py` with the exact dependencies in `sdk/timecard-policy-sources.json`. The former covers 174 workflow/regression runs and six paired frame/storage comparisons across normal and ASan/UBSan. The latter adds 46 real-app/adapter/host cases: idle, low battery, queued input, activity, continuous and detached pauses, AGAIN/BUSY, retained pause/resume/policy, unsupported checkpoints, clean exit, Contexts model/capture/message work and broadcast progress. Scene, device and resident provider boundaries are deterministic fixtures; these tests do not execute target ISA, physical sleep, or real radio hardware.

Target recipes retain all selected predecessor feature flags and original font inputs, check unchanged model bytes, validate stack and loader contracts, and compose only this application over the newest qualified complete cohorts. Full firmware delivery and flashing remain separate.

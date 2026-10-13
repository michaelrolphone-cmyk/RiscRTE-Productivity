# X4 Points 0.6.18 modal policy

The deployed Points 0.6.17 source is Productivity d6a9657378fedcdb61d1cea83f2108799e57ccae,
System 5b28172e979b5f008da19e9a00a3b98e35937974, Runtime SDK
b25b1d467a557e8d693211cff2eff959eb9c79de and Utilities
35c2141c68668971dc524d98702109b00c740815. Rebuilding and compacting reproduces
all 264532 bytes of ELF 0ea368123a6f07bc251e233a15f116831fd2cf60e1144e9ae0514bb231dc0501.
The old Library .62 image independently supplies those bytes; later X4 SD
composition inventories select the same hash. The .108 internal manifest is
0.6.17. Complete physical SD contents have not been audited.

This successor retains the X4 catalog application, semantic raster snapshots,
fonts, controller, schedule/type controls, storage behavior, Home discard,
resident shell, native UTC time and disabled-by-default telemetry. No Watch
binary or Watch UI is used. In particular, the Watch branch lacks the deployed
X4 semantic renderer; replacing the app from that branch would regress it.

Only --modal-policy opts the name editor into shared host checkpoints. It
captures activity, waits for settled input/display custody, latches continuity
across a pending pause, dispatches the resident Home policy, and resumes the
same draft. A routine continuity promise preserves input; possible sleep fully
detaches it. Done updates the draft only; Save remains the storage commit.
Missing checkpoint support cancels cleanly. Failed pause/resume retains the
invocation and prevents further provider calls or repeated policy dispatch.
Runtime/resident exit closes without redraw or a competing app launch; alarm
attention remains ordinary cancellation. All capability grants are unchanged.

The System component c4579cdb14f4ce185aed32d5de86f44a25733007 adds only the
opt-in bridge to the deployed adapter. Native firmware remains a91edf3; only
that SDK is used for the candidate. The complete newer System adapter was
rejected because its queued-controls behavior fails the existing Points draft
policy regression. The opt-in source remains suitable for component build
selection without replacing System main or any other deployed component.

## Reproduction and evidence

Run scripts/qualify_x4_points_policy.py with its pinned dependencies and the
GCC 8.4.0 esp-2021r2-patch5 compiler. The checked-in source-only baseline SDK
archive and original profile are hash-verified before use. No old ELF is a
build input. The retained metadata compactor comes from RiscRTE-Watch
362537417c4f37bbb240879d5ba756686511fec1 and verifies allocated bytes and
relocations survive compaction; repository and bundled source/font notices
remain applicable.

The runner exercises 49 existing real-app/adapter regressions and 16 added
checkpoint scenarios normally and under sanitizers. It compares all 19
captured frames with the exact deployed source profile. Two historical
synchronous fixture assumptions failed against the original 0.6.17 build:
scripted input now waits for the visible page, and the display-drain fixture
waits for an actual submitted token. Original assertions remain enabled.
Host builds omit debug-symbol packaging (macOS dsymutil stalled) and silence
unused static helper warnings from the frozen source; target -Werror remains.

The text provider, resident dispatcher and hardware are deterministic fixtures;
these tests execute the production X4 app and adapter, not Xtensa instructions.
Physical editor/input/power/sleep/wake behavior remains unverified. Final Linux
ASan/UBSan outcomes and exact target hashes are recorded by CI.

The .107 and .108 packages stay immutable. This branch does not modify RF,
OBDTest, Files, text-host binary, settings, Scanner or native firmware. Product
assembly and additive SD selection are separate. Delivery is held until
2026-10-13 23:30 UTC; no merge, release or flash is performed.

# Productivity migration readiness

Audited 2026-10-01 against Reader master
`3300229d0a232b4e6047a7c93b2f518c033c3cfa`. Reader and U1 are read-only references.
This index separates current-master parity from future cutover readiness.

| App | Source/manifest/helper | Version transition | Independent build and published ELF | Documentation |
| --- | --- | --- | --- | --- |
| [Text Editor](apps/text_editor.md) | Exact, 3 files | 0.2.0 → 0.2.1 | Pass, byte-identical to app-text_editor-v0.2.1 | Updated discard semantics |
| [Timecard](apps/timecard.md) | Exact, 2 files | 1.0.0 → 1.0.1 | Pass, byte-identical to app-timecard-v1.0.1 | Updated lunch overlap semantics |

## What changed and was checked

Text Editor restores saved contents before continuing a discard transition and
keeps unsaved edits when reloading fails. Timecard subtracts only lunch overlap
within the shift. The external base's five tracked inputs were compared with the
pre-fix Reader source; every changed input was upstream-only and the helper was
unchanged. No external-only edits were overwritten. [Sync audit](sync-audit.json)
and [current drift report](source-drift.json) record exact blobs.

The standalone build requires no firmware/Reader checkout. [Build instructions](BUILD.md)
describe the pinned SDK, exact compiler, focused host tests and ELF checks.
[Release parity](release-parity.json) records both actual built and published
hashes. Exact-head and postmerge CI links belong to the associated PR discussion;
a local passing build is not itself a claim that CI or merge completed.

## Remaining work before safe Reader removal

Neither app is currently approved for removal from Reader or external cutover.
All of the following must be established in the explicitly authorized cutover:

1. Agree the frozen source/ABI baseline and reconcile any master/U1 changes,
   preserving external fixes, stable app IDs, versions and upgrade ordering.
2. Deliver and validate the U1-compatible per-app `.rte.zip`, generic package
   manifest/index, install layout and resources from a separately authorized
   external release pipeline. Current outputs are development ELF+JSON pairs.
3. Verify target-runtime discovery, install/update, dependency/capability failure,
   rollback/recovery and uninstall behavior. Text Editor keyboard availability,
   input during refresh and file persistence; Timecard stored data and clock/
   keyboard handoff must work with the accepted runtime/provider set.
4. Verify version/digest/size consistency and external asset availability before
   changing any live provider/catalog reference. Retain a safe rollback path.
5. Obtain explicit owner approval for external-provider switch and Reader deletion;
   only then remove the duplicate Reader build/catalog entries without losing
   data or relying on resident substitutes.

Prospective U1 ABI/package parity remains unverified here. Passing current-master
source and binary parity is valuable preparation, not evidence U1 or cutover is
complete. No manual release, catalog switch, Reader deletion, deployment or flash
is performed by this workflow.

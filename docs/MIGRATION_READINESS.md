# Productivity migration readiness

Audited 2026-10-01 against Reader master
`4530c8b23b13a64f29c212cd64f1e86b05885287`. Reader and U1 are read-only references.
This index separates current-master parity from future cutover readiness.

| App | Source/manifest/helper | Version transition | Independent build and published ELF | Documentation |
| --- | --- | --- | --- | --- |
| [Text Editor](apps/text_editor.md) | Exact, 3 files | 0.2.1 → 0.2.2 | Pass, byte-identical to app-text_editor-v0.2.2 | Opening, retry and pagination |
| [Timecard](apps/timecard.md) | Exact, 2 files | 1.0.1 → 1.0.2 | Pass, byte-identical to app-timecard-v1.0.2 | Clock failure and recovery |

## What changed and was checked

Text Editor preserves `/sd` handoffs and maintains a bounded, cancellable picker
cursor across pages. Failed opens/reads preserve the document for retry. Timecard
uses one clock snapshot per current-time punch and performs no punch mutation or
storage write when that snapshot fails. Existing discard/lunch fixes remain.
The external base `0a2d189f2470dc38cb2e52edbefe6cc1de3e4fca` was compared
with its recorded source baseline: four changed inputs were upstream-only and the
helper was unchanged. No external-only edits were overwritten. [Sync audit](sync-audit.json)
and [current drift report](source-drift.json) record exact blobs. SDK, compiler,
external regression fixtures and packaging remain unchanged.

Release index `a8763df20c1e5d42ecac6e166b48d0d13b3e4c70` records the existing
Reader releases copied here; no additional version bump is required for identical
bytes. Host regressions cover failure/retry and cancellation. Directory-read
EOF/error ambiguity and blocking-call deadlines remain ABI limits; failed Timecard
saves do not promise in-memory rollback. See the per-app documents for details.

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

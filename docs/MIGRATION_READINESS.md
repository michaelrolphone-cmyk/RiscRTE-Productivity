# Productivity migration readiness

Audited 2026-10-02 against Reader master
`00f9b2458dbfdae2188f2695634edb40c65c7ab0`. Reader and U1 are read-only references.
This index separates current-master parity from future cutover readiness.

| App | Source/manifest/helper | Version transition | Independent build and published ELF | Documentation |
| --- | --- | --- | --- | --- |
| [Text Editor](apps/text_editor.md) | C/helper unchanged; current manifest synchronized | 0.2.2 → 0.2.3 | Pass, nested ELF `54f609bf…` / 18,256 B | Opening, retry and pagination |
| [Timecard](apps/timecard.md) | Current app source, manifest, and store-failure fixtures synchronized | 1.0.2 → 1.0.3 → 1.0.4 | Pass, nested ELF `80d6c1db…` / 14,572 B | Clock and store failure/recovery |

## What changed and was checked

Text Editor behavior and helper remain unchanged. Timecard now validates the complete stored document before committing loaded history; failed reads or malformed/trailing data leave prior history intact and set a read-only state until a clean retry. Clock-failure, storage-failure, and source-contract tests cover those paths. The external base `4c9ae58cf172babd4405a0d6b095d1517eb1fa5a` was compared with current Reader. The Text Editor manifest and Timecard app/test inputs were upstream-only against their recorded source baseline; scoped target files now converge to that source. No external-only changes were overwritten. The SDK, compiler, and ELF builder remain unchanged. [Current drift report](source-drift.json) records exact blobs.

Reader's current RTE release assets are verified from workflow runs `36965130240` (Text Editor) and `37020390546` (Timecard) against public GitHub Release asset SHA-256/size. Text Editor's nested ELF remains byte-identical to 0.2.2; Timecard 1.0.4 has its own verified nested ELF identity. No further version bump is justified. Host regressions cover failure/retry and cancellation. Directory-read
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

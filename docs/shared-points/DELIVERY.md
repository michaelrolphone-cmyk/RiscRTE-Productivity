# Shared Points in Time

This candidate replaces application-owned pixels and input hitboxes with the reusable component-scene and semantic-intent contracts. One canonical ESP32-S3 ELF is selected by all three products. Providers retain layout, physical input, display geometry, alarm presentation and physical lifecycle.

## Scope and compatibility

- List, add, edit and delete points; time, weekdays, duration, start/end/warning behavior, mode, enablement, custom names and colors; draft rollback and explicit save/check actions.
- Runtime-negotiated legacy and catalog storage. Legacy providers retain eight stable slots and two custom labels with the exact existing records. Catalog providers retain dynamically sized stable-ID events/types. A twenty-row scene window is pagination, not a catalog limit.
- Raw RTC and native UTC storage remain separate. Native storage uses `points_utc_cfg`/`points_utc_meta`; raw storage uses `points_cfg`/`points_meta`. No occurrence namespace access. Opening and cancelling never persist defaults or migrate storage.
- Uncertain catalog saves expose a read-only Check. The legacy provider retains its existing explicit Retry save behavior: it rewrites the pending bytes and verifies exact readback. The UI distinguishes these actions. Legacy key-value storage has no compare-and-swap primitive; the adapter checks observed records before first write, but cannot make concurrent writers atomic.
- Missing capabilities are disclosed and edits gated. Public text entry owns the keyboard. Context services, rules, settings effects, BLE background capture, copied workflow messages/launches, resident controls/alarm/idle and modal text checkpoints remain integrated through reusable controllers.
- Local clock/zone, 12/24-hour preference, native DST gap/fold/range status, and optional reconciled schedule context are preserved. Clock and timezone mutation are not Points responsibilities.
- Publication, physical device execution, heap pressure on devices and real SD operation are not established by host or ELF tests.

## Source and provider authority

The source lock is `sdk/points-scene-sources.json`. Productivity base is `bb8d0d99b3a857103d2b01e6b4c756b913aa14d3`; newer native Points reference `603364d8e0889984b51672c43c7ac27955e92706` was reconciled for clock/context behavior. Shared System components are pinned to published `77c427a75e5651f03119c059fcdfd36d23df8708`, and Runtime to `712ecc3f894dba99d9c9638941778a82f5856b97`.

Utilities `e817612532beb613376d69a502cd1e11b94ac8c4` is the published civil-clock correction qualified with Firmware Update. Its tree is identical to the previously qualified local commit `aaf4f9302ebd281efe6682a7473f77164406c3f5`. The source lock and CI use the published commit; qualification must bind this exact closure.

The Runtime declaration parser permits only one exact API version per capability. Therefore the Watch/Waveshare manifests declare alarm.service API 1 and X4 API 2, matching their existing authority; the ELF is identical. This metadata difference does not introduce device branches into the application. Old physical input/sleep grants are removed. Scene and civil-time grants select unique existing installed providers. Storage namespaces and unrelated applications/providers remain unchanged.

## Reproduce locally

Use Python 3.12+, pyelftools 0.32, a host C/C++ compiler, and the locked ESP32-S3 GCC15 toolchain. Supply local Git checkouts containing the exact pinned commits. The builder uses `git archive`, never dependency working-tree edits.

```sh
python tools/build_points_scene.py --runtime ../Runtime --system ../System --utilities ../Utilities --target-cc /path/to/xtensa-esp32s3-elf-gcc --output evidence/application
python tools/test_points_scene.py --build evidence/application --output evidence/host-normal
python tools/test_points_scene.py --build evidence/application --output evidence/host-sanitized --sanitizers
python tools/assemble_points_scene.py --application evidence/application --baseline ../baseline --bootfs-tools ../Bootfs/scripts --output evidence/assembly
python test/test_assemble_points_scene.py --application evidence/application --baseline ../baseline --bootfs-tools ../Bootfs/scripts --report evidence/assembly-tests.json
```

The six exact published baseline assets and URLs are locked in `docs/shared-points/baseline.json`. Assembly rejects altered assets, manifest/declaration drift, provider ambiguity, storage-authority expansion and stale X4 content-addressed routes. It repacks fixed-size stores and SD ZIPs, reads both back, and checks unrelated bytes. No settings or saved-user-data upload is involved.

Run the published System `scripts/test_home_product_admission.py` in normal and sanitized modes with exact Runtime/product checkouts and the assembled products. It validates the complete actual Runtime admission path; it is not physical execution. Retain its receipt, source pins and all test receipts together.

## Release gate

The source archive includes the selected Productivity files and exact staged dependency sources. The publication plan contains only added/changed Productivity files, their hashes and the immutable original tree. It does not authorize deleting omitted repository files. Before publishing, compare the current destination branch with the base commit and reconcile any intervening edits; requalify changed source.

Keep application/source, host qualification, assembly and actual product-admission receipts bound to the same source and ELF hashes. A stale source closure is a failure, even when individual tests passed. Do not publish from a checkpoint build after later edits.

This source candidate is authorized for ongoing development publication. Release assets and physical qualification remain separate, unverified outcomes until their receipts are recorded. The prior BPMN Orient verified submission was rejected and is not completion evidence. Preserve the original task's hardware acceptance requirement; do not translate software-only qualification into a hardware claim.

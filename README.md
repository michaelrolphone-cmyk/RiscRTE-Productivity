# RiscRTE Productivity

Independent source repository for end-user RiscRTE productivity/work applications. During migration, `michaelrolphone-cmyk/T5S3-Reader` is a strictly read-only upstream source.

## Scope

This repository contains applications for writing, tracking, organization, and other productive end-user workflows. These apps are optional and are not part of the minimal foundational system-app set.

## Application documentation

- [Text Editor](docs/apps/text_editor.md) — plain-text/Markdown editing, file workflows, automatic USB keyboard capability handling, and editor limits.
- [Points in Time](docs/apps/points_in_time.md) — eight durable recurring daily slots and optional lunch/break end alerts, delivered by the shared ordinary alarm service.
- [Timecard](docs/apps/timecard.md) — weekly punch tracking, persistent JSON data, time entry, and worked-time calculation.

## Independent builds and migration status

- [Build instructions and verification limits](docs/BUILD.md)
- [Indexed migration readiness and safe removal criteria](docs/MIGRATION_READINESS.md)
- [Source-aware synchronization audit](docs/sync-audit.json)
- [Published-byte comparison](docs/release-parity.json)

The preserved Reader migration pipeline builds only Text Editor and Timecard; a Reader checkout is not required. CI produces development ELF/sidecar evidence, never releases or changes live catalogs. Reader remains a read-only reference.

Points in Time 0.2.0 is an original, separate portable application with the NOVA-7 watch-native list/editor UX. Its exact-pinned System adapter and Utilities records/service inputs are built through a separate development workflow. It does not extend the migration baseline, duplicate Runtime, or turn Timecard into a Watch app. See [Points build and verification](docs/POINTS_IN_TIME_BUILD.md).

## Documentation and parity policy

Each app has a dedicated source-derived page covering its interfaces, workflows, persistence, limits and failure behavior. Source, version, build and documentation parity must all be verified. External changes must survive later upstream synchronization: classify changes against recorded source blobs before reconciling them. Never blindly overwrite an external fix or treat a successful build as authorization for cutover.

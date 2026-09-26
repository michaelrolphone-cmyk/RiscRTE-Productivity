# RiscRTE Productivity

Independent source repository for end-user RiscRTE productivity/work applications. During migration, `michaelrolphone-cmyk/T5S3-Reader` is a strictly read-only upstream source.

## Scope

This repository contains applications for writing, tracking, organization, and other productive end-user workflows. These apps are optional and are not part of the minimal foundational system-app set.

## Application documentation

- [Text Editor](docs/apps/text_editor.md) — plain-text/Markdown editing, file workflows, automatic USB keyboard capability handling, and editor limits.
- [Timecard](docs/apps/timecard.md) — weekly punch tracking, persistent JSON data, time entry, and worked-time calculation.

## Repository tree

```text
Apps/
  text_editor.c
  text_editor.json
  text_editor_core.h
  timecard.c
  timecard.json

docs/
  apps/
    text_editor.md
    timecard.md

productivity-manifest.json
```

## Documentation and parity policy

Each migrated productivity app must have a dedicated documentation page derived from its actual implementation, manifest, ABI headers, and behavior. Interfaces/capabilities, user workflows, persistence, data formats, limits, failure behavior, and helper files are documented when established by source. Future behavior is not invented.

A productivity app is not parity-complete until source, manifest/version, build/release behavior, and documentation are aligned with the approved upstream app set.

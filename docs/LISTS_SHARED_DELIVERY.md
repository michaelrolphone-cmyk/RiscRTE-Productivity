# Shared Lists 0.1.3 packaging delivery

This change resolves a **packaging-only divergence**, not an application rewrite.
The published X4 0.1.123 Lists 0.1.2 image is the canonical **complete** ELF.
It retains the original Xtensa instruction-property and literal metadata rather
than discarding that qualification evidence. All three outputs receive exactly
these same bytes. Original per-product ELFs and an identical full sidecar remain
in the qualification artifact.

The recipe verifies four immutable original product inputs by full SHA-256,
decodes actual stores, and compares every allocated section's bytes, address,
size, type, flags and alignment plus entry point, ELF flags and relocations.
The existing Watch/Waveshare executable bytes must match X4 before packaging.
Any mismatch fails; it never silently selects one behavior over another.

Only the Lists image and its package version change. The new manifest version is
0.1.3; every other installed manifest field is retained per product. Boot policy,
grants, provider packages, namespace bindings, unrelated internal files and every
unrelated SD member remain byte-identical. Store size/geometry is unchanged.
The builder reads each newly packed store back and compares its complete logical
contents. There is no replacement of saved Lists data, no app downgrade and no
provider upgrade to make the test pass.

## Source and native presentation

The existing semantic controller, model and persistence source is unchanged:
`Apps/lists_scene.c`, `Apps/ListsScene.c`, and `lib/Lists/`.
The app requires `ui.scene@1`, `storage.app-data@1`, and `time.civil@1`.
These core services are not optional: missing-provider startup prints a clear
diagnostic, releases acquired capabilities and exits. The existing missing-1/2/3
tests verify refusal and cleanup, not a usable missing-provider UI. Resident-shell
participation is optional. Invalid civil time disables date-dependent features
while undated lists remain usable; recoverable storage failures expose read-only
retry behavior without replacing saved data. Service presence and operational
availability are distinct contracts.
Presentation, dimensions, input and hardware remain provider-owned. The retained
native qualification covers five reported sizes, real renderer/hit targets,
keyboard, modal/repeated lifecycle and storage failures. Existing features,
foreground reminders, date policy, data format and recovery behavior are
documented in [Lists scene](LISTS_SCENE.md) and
[native qualification](LISTS_NATIVE_QUALIFICATION.md).

This package deliberately reuses a verified original ELF; it does **not** claim
that this change freshly compiled that ELF from today's source checkout.
Existing native source tests are separate evidence from the exact original-byte
equivalence and Runtime admission of the delivered package.

## Owning hosted qualification

`.github/workflows/shared-lists.yml` repeats the complete package projection,
checks byte-identical output stores and ELF, exercises corruption/path/identity
regressions, then runs normal and ASan/UBSan admission against actual Runtime
source. It reuses the pinned, product-generic native admission fixtures published
in System Apps `db47bebca498e1eb44559fdbf69002b935f42dac`; despite their historical
Files directory name, these prepare and validate the complete product cohort.
No Files grants or replacements are applied.

The native admission uses the actual ELF validator, ordinary-import/role checks,
source-closure hashes, scoped file access and retained X4 SD binding. Hardware
and storage callbacks are inert and must remain uncalled. Leak detection is
disabled explicitly. This is not target instruction execution or physical-device
testing. The historical Lists native workflow remains separate and unchanged.

Final exact-head hosted results and canonical SHA-256 belong in the PR handoff
after the runs finish. At initial publication these checks are pending.
The outputs are component/product projections, not relabeled native firmware
or a claim that complete device firmware was rebuilt or flashed.

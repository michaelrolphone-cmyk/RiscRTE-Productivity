# Reader 0.1.3 cache access correction

The X4 0.1.73 / Reader 0.1.2 report reaches `scene ready page=480x632 sd=ready`
and then reports `Invalid link count 44` / `Page cache is unreadable` when a
book is opened. This is a separate porting defect from the stack overflow.

The pinned FreeInk `SDCardManager::openFileForWrite` opens
`O_RDWR | O_CREAT | O_TRUNC`. CrossPoint's `Section::startBuild` uses that
helper, then `loadPageDuringBuild` seeks and reads completed pages through the
same open handle before returning to the write cursor. The RiscRTE adapter
incorrectly used `O_WRONLY`. The production `StorageFatFs/volume.c` checks
the read permission and records `FR_DENIED` for that read. Upstream's unchecked
scalar deserializer then reports an uninitialized count; 44 is not evidence
that the book contains too many links.

The adapter now preserves the upstream read/write contract. Explicit callers
of `Storage.open(..., O_WRONLY)` remain write-only. Driver permissions, source
book access, cache format, bookmarks, and the 0.1.2 stack correction are unchanged.

The original host volume opened writable files using `w+b`/`r+b` and did not
enforce the requested flags, hiding this defect. It now enforces read/write
permissions and keeps a failed handle in the failed state, like the provider.
With that correction, the old port fails `Engine::open` and reports an invalid
count; with the port fix, the existing engine and real app-entry tests pass.

`scripts/test_reader_fatfs.py` also compiles the production Drivers volume and
FatFs against a RAM card. It checks seek/read/write across sector boundaries,
denies read-on-write-only and write-on-read-only, and exercises fresh,
suspended partial, completed, and incomplete caches for EPUB3, EPUB2, TXT and Markdown.
It verifies page turns, bookmark/location preservation, reflow and handle
cleanup. The old port fails the first readback assertion in this test. The
FatFs and test translation units use UBSan; the reused engine objects are the
ordinary host build. This does not emulate SD transport or prove device timing.

The rejected read in 0.1.2 also poisons later writes to that handle, leaving the
section's incomplete sentinel. The existing upstream invalidation path rebuilds
that derived cache on the next open. The FatFs test verifies this recovery while
retaining bookmarks; users do not need to delete their Reader state directory.

Use the unchanged X4 0.1.73 firmware and panel 0.1.12. Replace only
`Apps/ebook-reader/ebook_reader.elf` while Reader is closed. The native
candidate admission check and physical device testing are separate gates.

# CrossPoint to RiscRTE ELF migration

The authoritative baseline is `upstream.json`, not an unlabelled copied fork.
CrossPoint: `3607ea7bb95d65b4cf3edf1aa89b2b0970985793`.
FreeInk SDK: `425d200a8ea447326b4b9696e4e47dc84ad9d7f6`.
The gitlink preserves the original tree. `scripts/reader_sources.py` selects the
imported files; `patches/0001-riscrte-port.patch` contains every engine edit.

| Upstream source | Migration |
|---|---|
| `lib/Epub`, `Txt`, `ZipFile`, `InflateReader`, `Serialization` | Retained parsing, indexing, conversion and pagination; storage types translated at the source boundary |
| `lib/EpdFont`, `GfxRenderer`, `Utf8`, `MiniBidi` | Retained text/bitmap engine into an offscreen buffer; the shared scene provider owns the physical surface |
| `lib/LibraryIndex` | Retained CLX1 records/sorts; state paths redirected; installation and interrupted replacement delegated to the shared filesystem capability |
| FreeInk `libs/font/FreeInkFont` | Retained shaping/FreeType stream fonts; pathname-based libc filesystem entry disabled, explicit storage streams retained |
| `src/ReaderFontSizes.*`, `fontIds.h` | Retained font sizes and IDs |
| EPUB protection entry point | Explicit unsupported-encryption response; no upstream encrypted-book UI/service |
| Firmware activities and menus | Replaced by `ReaderApp.cpp` declarations using existing NOVA components |
| Platform/Arduino HAL calls | Narrow source-compatibility names mapped to capabilities; no hardware implementation in these adapters |

The patch also adapts C++20 constraints and float parsing to the pinned C++17
compiler, removes the single-owner bidi mutex, directs Expat diagnostics and
random seeding through acquired runtime services, and replaces the font cache's
ESP heap call with `memory.heap`. The nested-NCX regression also fixes upstream
NCX href resolution to use the NCX document directory, matching EPUB navigation
semantics, with a dedicated synthetic test. `sdk/cxx` in Runtime is the reusable PIC STL
and constructor/destructor solution. Its source and licenses are recorded there.

## Updating upstream

1. Create a branch from the last working reader commit. Keep the previous build
   receipt, dependency SHAs and test results.
2. Advance the CrossPoint gitlink to a reviewed commit; initialize its exact
   FreeInk submodule. Update both SHAs in `upstream.json` together. Update PNGdec
   and JPEGDEC only when intentionally selecting those revisions.
3. Materialize with `reader_sources.stage(..., apply_patches=False)` to inspect
   the pristine selected tree. Apply/rebase the patch in a separate staging
   tree. Treat failed hunks as review work; do not silently skip them.
4. Compare each changed upstream dependency boundary with the shared capability
   contracts. Add missing generally useful behavior to its owning capability
   and provider, then consume it from the app.
5. Regenerate the patch relative to that pristine tree. Verify a second clean
   materialization is byte-for-byte identical to the reviewed staged sources.
6. Run synthetic EPUB3/EPUB2 NCX/TXT/Markdown, short-I/O, pagination/reflow,
   bookmarks, failed catalog replacement, corrupted state and SD font tests.
   Run shared scene, storage, grant-lifetime and ELF admission tests.
7. Build the exact X4 native/provider/app composition. Verify resolver exports,
   ELF relocation targets, bootstrap-store capacity and resident lifecycle.
   Record physical testing separately; publish no hardware claim from host tests.

## Earlier-reader lessons retained

The old T5S3 project remains a read-only reference. The shared FatFs source was
migrated to Drivers with a separate import receipt; it is not maintained in this
app. TXT short reads are retried. Reflow anchors use document offsets. Catalog
replacement belongs to storage, and retains the prior index on failure. Settings
and bookmarks acknowledge durable readback. Font handles close before shared
shell/USB/sleep transitions. Optional capability gaps fail explicitly rather
than gaining a reader-only fallback implementation.

## Licenses

Keep CrossPoint and FreeInk notices, Noto OFL notices, Expat, miniz/uzlib,
FreeType, bidi tables and PNGdec/JPEGDEC notices with derived distributions.
The pinned source trees retain per-file notices; the build copies license files
alongside the module. Runtime's GCC support includes GPLv3 text and the GCC
Runtime Library Exception. Review upstream license changes during each update.

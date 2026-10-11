# Reader for the e-ink software class

This is a CrossPoint reading engine compiled into an ordinary RiscRTE ELF,
with NOVA scene components for the library, menus, settings and bookmarks.
The initial deployment is X4. No CrossPoint activity, theme, board driver,
firmware entry point, power manager, Wi-Fi service or OTA code is linked.

## Supported documents and fonts

EPUB, UTF-8 TXT and Markdown are read from `storage.volume@1`. The original
EPUB parser, text/Markdown conversion, pagination, bidi processing, image
decoders, font caches and CLX1 book index are retained. Library views offer
title, author and upstream Recent ordering; Continue reading tracks the last
book separately. Bookmarks, names, location and layout survive relaunch.

Noto Serif and Noto Sans are supplied as the original upstream TTF SD assets,
with regular, bold, italic and bold italic faces. Copy the package's `sd/fonts`
folder onto the SD card. CrossPoint's streamed loader supplies selectable point
sizes. A built-in 14-point Noto Serif family remains usable without the assets. The original SD discovery
and loaders support CrossPoint `.cpfont` v4 families and TTF/OTF/TTC fonts under
`/.fonts` or `/fonts`, using CrossPoint's naming rules. Font licensing remains
the responsibility of each font's distributor. Application chrome uses NOVA's
fonts. Reader layout includes spacing, margins, alignment, indentation,
hyphenation, embedded styles, images and text rotation; there is no theme UI.

Encrypted EPUB containers are rejected explicitly. The current protection
adapter also rejects containers declaring obfuscated embedded fonts in
`META-INF/encryption.xml`; it does not implement DRM or font deobfuscation.
Bookmarks are bounded to 64 per book. Bookmark naming uses the shared ASCII
keyboard. Library scanning cooperates with the runtime but is not cancellable.

## Build and test

Use the CrossPoint, FreeInk SDK, PNGdec and JPEGDEC commits in `upstream.json`.
Initialize `third_party/crosspoint` and its `freeink-sdk` submodule. Supply
matching checkouts of the shared Runtime and System changes in this migration.
Python needs `pyelftools`; the target compiler is the pinned ESP32-S3 GCC 8.4
toolchain. `NATIVE_APP_CXX` / `NATIVE_APP_CC` can select its absolute paths.

```sh
python3 scripts/build_reader.py --crosspoint third_party/crosspoint \
  --runtime ../RiscRTE --system ../RiscRTE-System-Apps \
  --png ../PNGdec --jpeg ../JPEGDEC --output build/reader
python3 scripts/test_reader.py --crosspoint third_party/crosspoint \
  --runtime ../RiscRTE --system ../RiscRTE-System-Apps \
  --png ../PNGdec --jpeg ../JPEGDEC --output build/reader-tests \
  --font /path/to/a/test-font.ttf
python3 scripts/test_reader_fatfs.py --drivers ../RiscRTE-Drivers \
  --engine-output build/reader-tests --output build/reader-fatfs
python3 scripts/test_reader_startup.py --runtime ../RiscRTE \
  --system ../RiscRTE-System-Apps --engine-output build/reader-tests \
  --output build/reader-startup
```

The FatFs test runs the production storage implementation on a RAM card and
checks incremental cache readback, access permissions, cache reopen and reflow.
See [CACHE.md](CACHE.md) for the 0.1.3 port-contract correction and its reproducer.

The build materializes selected pristine upstream sources, applies the recorded
patch, compiles the engine and app, links shared C++ SDK support, checks all
relocation write sites and relative pointers, and runs the production ELF
structural validator. `ebook-reader/build.json` records source and output hashes.
`scripts/verify_reader_native.py` runs the existing shared native admission
harness against the exact firmware ELF and all three new modules. Its receipt
also records the verification harness hash. Native resolver admission requires
the matching firmware candidate;
structural validation alone is not permission to install on old firmware.

## Shared capabilities, not app workarounds

| Requirement | Shared owner | Reader's use |
|---|---|---|
| Directory position, metadata, truncation and replacement recovery | `storage.volume@1`, `RiscStorageVolumeFsV1`; Drivers `StorageFatFs` | Direct capability calls; no entry replay or private filesystem |
| Available memory and largest allocation | Runtime `memory.heap@1` | Actual heap snapshot; no allocation probing |
| Expat hash seed | Runtime `random.bytes@1` | Native random source; no fake process/time seed |
| Document viewport and page controls | System `ui.scene@1`, `RiscScenePageV1` | Copied pixels and document; shared navigation/chrome |
| Library, settings and bookmark GUI | System scene components and keyboard | Declarative documents and validated intent events |
| C++ templates, static lifetime and linking | Runtime `sdk/cxx` | Ordinary module lifecycle; no private native services |
| Open with Reader | Runtime `file.open@1` | Acquired table; translates the existing `/sd` handoff namespace into volume-relative paths |

All optional suffixes preserve their existing ABI prefixes. The app checks
them at startup, and all native capabilities require explicit manifest/boot
grants. No display dimensions, panel pins, SD pins or refresh waveform decisions
belong to the reader.

## Persistence and custody

Writable reader state is isolated under
`/System/State/Applications/ebook-reader/`. The original `/.crosspoint` directory
is neither reused nor migrated implicitly. Per-book reading positions and
bookmarks use visible-text offsets plus spine index, not unstable page numbers.
Settings and book state use alternating CRC records with close and readback
before acknowledging a save. Two damaged records are not overwritten with
defaults. The shared filesystem provider owns library replacement/recovery.

The storage adapter loops over bounded short reads and writes. A short read
does not mean EOF. Checked-close or provider custody failure retains the
invocation and bypasses C++ destruction. Before shared controls, sleep policy or
USB access, the app closes its scene and all document, library and font handles;
it rediscovers fonts and reloads the book on return.

## X4 integration status

`deployment/reader/x4.json` selects the e-ink class. The matching scene provider,
filesystem provider and Runtime enhancements must be composed together. A
standalone app update on X4 `.65` does not supply those dependencies. The current
`.65` 5.3 MB bootstrap-store layout has insufficient room for this reader with
all existing applications. The full Noto families are SD assets so the app
itself remains below the existing 2 MB native admission bound. No flash layout or existing app
selection is changed by this source migration. Use the X4 capacity report before
choosing a product composition. Hardware qualification and a fitting complete
firmware cohort are outstanding.

See [MIGRATION.md](MIGRATION.md) for source ownership and the upstream update path.

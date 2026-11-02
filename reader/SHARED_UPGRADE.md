# Shared Reader 0.2.0 — installation and preservation

## What is delivered
One complete canonical `ebook_reader.elf` is selected on X4, T-Watch and Waveshare. This is not a reduced Watch substitute. The application uses provider-reported scene geometry and semantic components, not product-class or fixed-resolution branches. Selected scene providers retain their native display and input behavior.

The source reconciles Productivity main `bb8d0d99b3a857103d2b01e6b4c756b913aa14d3` with the actually delivered Reader 0.1.9 source `8f04e1bcb26c801e63ca57bf4bd95428c693d514`. The latter's installed ELF is 1,918,724 bytes, SHA256 `395eb0ad04b1c86b26f713b2c82e7d3f2a7a26dd802778bf4d355a5b55b351b2`, verified in the cumulative baseline.

Preserved behavior includes EPUB2/3, TXT and Markdown; pagination, images, links/history, Unicode/bidi, Noto Serif/Sans styles, typography and previews; bookmarks/names; CRC-backed state, text anchors and reading positions; library order/filter/continue/finished/hide/re-add; scrolling/scrub; resident reading and library checkpoints. Reader 0.1.9's /Books-only recursive shelf, old whole-card index filtering, outside-root Browse/file-open and efficient directory rows are retained with their regression tests. Hiding a book does not delete it.

## Exact cumulative baseline
`reader/shared-baseline.json` pins all six public assets from Watch release `bringup-shared-233e43841483adf570ae833a88d9b6fc08d68f1a`. This includes published Home 83afd and Contexts aa8 and Bringup 233e. Existing USB is preserved byte for byte; the unpublished USB 710b application is NOT included. System 710b is used only as pinned assembly/admission helper source.

Use the exact native firmware already qualified with that baseline:
- WatchProduct c69d7914935fccdc04749f7bfa429c74219bf88a, Runtime 712ecc3f894dba99d9c9638941778a82f5856b97.
- WaveProduct 97ca6157aa9c3bba7c7fee3858aa7d0ff896d13b, the same Runtime.
- X4Product e361e73de14314bb4b8d2fb3a24eb81e2390b0ee, X4 Runtime 0052d1fc42e9cfc82d626c5a247d460919c87935.
No firmware, board, provider image, unrelated application canonical byte or saved-data format is changed here.

## Data-preserving installation
These `product-*-store.bin` files are fixed-size companion application stores, NOT full-flash images and NOT images to write at address zero. Use the matching baseline's existing application-store installation/recovery procedure and geometry. Do not erase or format the data partition or SD card. Back up user data before any device update.

- Watch: full Reader is internal. Its existing internal package volume is deliberately not granted as book storage. With no user storage provider, Reader displays STORAGE UNAVAILABLE and does not alter books/state. The full engine remains in the identical ELF. The production ELF removes only non-runtime linker/debug symbols, with exact loadable-section, relocation and dynamic-symbol equality checks. It contains the complete engine. If required for fixed-size reserve, ordinary application images are losslessly enveloped using the already-published Runtime decoder; assembly.json records every representation change, canonical hashes and free-space result. Providers/data are not compressed or changed.
- Waveshare: manifest/policy are internal; Reader is `Apps/ebook-reader/ebook_reader.elf` on existing SD volume 12.
- X4: preserves the existing Reader identity, SD path on volume 9, catalog authority and resident foreground policy.
- Merge each capable product's SD archive into the existing SD card, retaining other files. Existing fonts are checked byte-identical; all original Noto TTF styles and OFL notices are included. Do not delete `/Books`, `/System/State/Applications/ebook-reader`, other user folders or `/.crosspoint`. No automatic CrossPoint-state migration is introduced.

Only ui.scene is mandatory for admission. Storage, heap, random and file-open contracts are optional. Missing services needed for reading produce clear unavailable UI without state writes; absent file-open still permits the library and browsing. A missing/unmounted SD card can also make an SD-resident application image unavailable through the native launcher.

## Evidence and limits
The owning CI runs the existing real engine, startup/resident, production FatFs, bookmarks/rollback, links, checkpoint and performance suites; full 28-intent native GUI flows at 480x800, 240x240, 410x502 and an additional 328x356 geometry; optional-service absence scenarios; and the retained 0.1.9 library regressions. Target compiler stack/import/ELF checks are retained.

Each store is packed and decoded at unchanged geometry, all unrelated canonical and SD bytes are compared, then the exact stored bytes enter the selected owning Runtime/CpuPort graph and ELF admission in normal and ASan/UBSan modes. Actual Wave/X4 SD Reader ELFs are also admitted, with malformed-ELF rejection. Receipts bind source dependencies, native profiles and exact candidate hashes. These are native host and target-structure results, not a claim of physical-device execution or performance.

The release includes full source/dependency archives, licenses, font assets, build and admission receipts, baseline pins and a public-download hash ledger. Publication is restricted to owning Productivity PR 31 and branch work/reader-shared-elf-20261102 after the complete qualification dependency chain succeeds. No PR merge or formal approval is performed.

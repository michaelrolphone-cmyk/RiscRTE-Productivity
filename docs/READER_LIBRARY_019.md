# Reader 0.1.9: library and browsing continuity

Reader's library scan now indexes `/Books` recursively. Existing indexes are
filtered before reading per-book status, so old whole-card entries disappear
from the shelf without deleting books or saved positions. Case-insensitive
`/Books/` matching is component-boundary-aware. Explicit Browse SD and file-open
requests continue to open supported files elsewhere on the card.

Policy checkpoints now pause and resume the existing library/list scene and park
its file handles. They preserve the current document, scroll offset and loaded
rows instead of destroying the scene and rediscovering SD fonts/catalog entries.
Shared-controls returns retain their previous cold-reload behavior. Reading page
turns still request CLEAN full refresh; an unchanged reading page remains idle.

Browse SD now consumes copied directory entries directly. It no longer opens
and closes every skipped file just to inspect its name and type.

## Focused qualification

Tested with Runtime `1c1ed8661ea30d47e7e9707c41d492ce3a2b4d70` and System
`c2889adff2a06bcd6c6207f11fbd0795fbbd99fa`, the X4 .100 component pair:

- Actual Reader entry with real scene provider: EPUB3, EPUB2, TXT and Markdown;
  direct outside-root file-open and Browse SD selection; old whole-card index
  filtering; policy/controls restoration and clean next/previous pages.
- The library policy case scrolls the real scene, verifies its changed pixels,
  then checks four policy handoffs (including BUSY) preserve those pixels with
  no extra frame or SD/font reopening. The old application fails this case.
- Scoped-index fixture excludes 200 application Markdown files and a sibling
  Bookshelf folder, retains nested books, and renders an explicitly opened
  outside-root text file. Listing those 200 entries opens no files.
- Existing engine, bookmark rollback, GUI, file-checkpoint, page-performance and
  reading-link suites are retained; commands are the existing reader scripts.
- ESP32-S3 target build passes structural ELF/import validation and checked
  compiler stack budgets. These are host/target checks, not device validation.

The separate Files handoff problem is not claimed fixed by Reader changes.

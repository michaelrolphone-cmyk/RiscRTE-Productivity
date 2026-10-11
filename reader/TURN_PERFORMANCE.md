# Reader 0.1.7: page-turn regression correction

Baseline: Reader 0.1.6 with scene-host 0.3.5 in delivered X4 0.1.76.
This is an app-only update. The internal scene provider and panel 0.1.12 do
not change. Copy `ebook_reader.elf` to `Apps/ebook-reader/ebook_reader.elf` on
SD and close/relaunch Reader. No full firmware flash or state reset is needed.
The firmware-owned launcher manifest may continue to display its pinned 0.1.6
version until the next firmware assembly; the app startup diagnostic identifies
`reader=0.1.7`. Its ID, entry point, descriptor and capability requirements match.

## Causes addressed

After a periodic resident policy checkpoint, Reader 0.1.6 deleted/recreated
its engine on the next interaction. That rescanned font directories, reloaded
FreeType faces, reopened the book and restored or restarted pagination. Its
PrewarmScope also cleared glyph caches at both entry and destruction of each
page render. Turning a cached page synchronously wrote and verified the full
BookState plus summary before publishing the new bitmap.

Policy handoff now retains the engine/parser/page/font objects in RAM while
syncing and closing every SD handle. Before the next actual interaction, the
port revalidates the stored path/type/size/available modification stamp and
reopens each handle at its saved position. It never reapplies CREATE, TRUNCATE,
APPEND or EXCLUSIVE flags. Repeated idle checks neither reopen the files nor
publish a frame. The actual shared-controls path still cold-suspends and
recreates the engine, since shared UI may change files/settings. Unexpected
file changes, resume errors and uncertain close retain the invocation rather
than writing through an invalid cursor. Normal teardown unparks before finishing
partial cache persistence, then closes all storage and releases capabilities.

Vector fonts use their existing bounded on-demand glyph/kerning cache, not the
cache-clearing prewarm scope. The bitmap-font prewarm path is unchanged. Readonly
files gain small heap-allocated windows: one 512-byte sector for random font
reads and up to 4 KiB for sequential cache reads. Cursors remain logical across
buffer hits, short physical reads are handled, and windows are invalidated at
handoff. No large file object is allocated on the task stack.

Changed page state is saved after scene presentation becomes idle, and is also
saved before explicit close/suspend/policy handoff. Unchanged state/summary
records are compared with their last durable snapshots to avoid repeated writes.
Bookmark changes still commit synchronously with rollback on failure. As with
any deferred position save, abrupt power loss before display completion can
leave the previous durable reading position; bookmarks are not deferred.

## Repeatable validation

`test_reader_performance.py --verify` drives the real app and scene through six
post-policy page turns on a completed chapter and six on an incremental chapter.
The identical measurement fixture was also run on unchanged Reader 0.1.6.
For the completed chapter, mean tap-to-publication capability work changed:

| Operation | 0.1.6 | 0.1.7 |
| --- | ---: | ---: |
| File opens | 49 | 6 |
| Font-directory opens | 5 | 0 |
| Physical read calls (173-byte short-read fixture) | 3661.5 | 122 |
| Read bytes | 368293.7 | 20630 |
| Writes before bitmap publication | 35 | 0 |
| Seek calls | 737 | 9 |
| Font face loads | 2.17 | 0 |

For incremental chapters, mean read bytes fell from 421447.7 to 3694.8. Necessary
pagination writes can still occur when the requested page is not yet cached.
These are HOST capability-operation counts, not ESP32 timing or a prediction of
panel speed. No millisecond board-performance claim is made.

The existing 17 bounded-stack startup/lifecycle scenarios, full 28-intent GUI,
engine persistence/failure tests and production FatFs tests are rerun. All 22
native GUI captures match the same local 0.1.6 fixture byte for byte. New ASan +
UBSan file tests exercise short reads, seek/EOF, read-write/directory checkpoints,
move semantics, repeated idle checks and missing/changed/revoked failure paths.
The target build retains the existing ELF-size and stack-frame checks.

Page turns still request one CLEAN refresh. No fast waveform, panel replacement,
extra page repaint or idle redraw is used to obtain the improvement. The new
`Reader/turn` diagnostic reports restore_ms, advance_ms, render_ms,
display_wait_ms and total_ms. display_wait_ms includes scene rasterization,
scheduling and panel completion; it is not a direct panel BUSY measurement.
Physical testing remains pending.

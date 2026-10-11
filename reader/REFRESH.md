# Reader refresh and resident restoration

## 0.1.5: static pages remain static

The 0.1.4 restore path still closed/reopened its scene and republished the same
page every periodic power checkpoint. Its test counted CLEAN requests only,
so it missed redundant DEFAULT submissions and their physical panel effects.

With scene-host 0.3.4, Reader pauses the scene instead. Pause releases input
focus and display custody but preserves the page, chrome and revision. Resume
with zero flags restores input without invalidating the framebuffer. The
resident bridge now returns reply flags: CONFIGURATION_CHANGED alone does not
request a repaint; REDRAW after sleep or shared controls requests one clean
page restoration. Pending/cancelled frame work is retained correctly.

Book and font handles close before dispatch and stay closed while the page is
static. They reopen on the next book interaction; the scene's saved pixels
remain visible throughout. This also removes periodic font discovery, page
rendering and SD access. Incomplete-cache restoration on older providers does
not publish a clean page just because rebuilding the same page finished.
Older scene providers retain the earlier close/reopen fallback; X4 0.1.75 is
required for zero-submission idle restoration.

The real-app test now counts **all** display submissions and storage opens.
Four policy checks including BUSY produce no new page publications, display
submissions or book/font opens. Cases cover EPUB/TXT/Markdown, a later saved
page, an explicit redraw reply, shared controls, and next/previous after idle.
The 0.1.4 app fails the tightened test at its first unchanged-page publication.
Scene tests cover zero-I/O paused servicing, cancelled raster and pending-frame
custody, full frame restoration after external drawing, paused close, and
retention fences at unsubscribe/subscribe/snapshot/focus/reset failures.

## 0.1.4: library startup and resident restoration

Direct launch reads the CLX1 library; a File Browser handoff opens the selected
book directly. CrossPoint's `readTitle` and `readAuthor` return false for empty
optional fields as well as read failures. The library now checks actual I/O
failure separately and falls back to the file name for untitled books. An
unreadable derived index leaves Browse SD and Scan library available.

The resident shell requests periodic battery/power policy checkpoints. Reader
must close its scene and storage handles before dispatch. Previously it rebuilt
the scene with the Reading progress document, which service callbacks could
paint while reopening fonts and caches. It now copies the saved page into the
new scene before doing that work, then reconstructs the engine bitmap behind
the visible page. OK and BUSY both restore; retained custody still terminates.
Active pagination inhibits idle policy until it reaches a usable page.

New page publications request `RISC_SCENE_PAGE_REFRESH_CLEAN` through the
optional shared scene suffix. System scene-host 0.3.3 maps this to one full
display CLEAN/FIFO submission with no partial damage rectangle. Cached-page
restoration uses DEFAULT, so ordinary power checks do not clean-flash the page.
The app does not select a panel or waveform. On an older scene provider it keeps
the existing presentation behavior; X4 0.1.74 supplies the clean-refresh service.

`test_reader_startup.py` uses the real app and scene provider on a 32 KiB host
thread stack. It covers library/EPUB3/EPUB2/TXT/Markdown startup, indexed books
without author metadata, browsing after an injected index-read failure, four
policy restores including BUSY with identical
pixels and only one clean refresh, and next/previous page changes with three
clean submissions. Old 0.1.3 fails the catalog and resident tests. These are
host contract tests; hardware refresh quality remains for device testing.

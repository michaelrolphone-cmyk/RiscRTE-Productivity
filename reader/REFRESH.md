# Reader 0.1.4: library startup and resident restoration

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

# NOVA-7 e-ink Reader, 0.1.6

## Design and native implementation

Basis: the owner's `Reader (eBooks) — NOVA-7 watch + e-ink.html`, specifically
the 480x800 e-ink presentation. This is native app/provider code, not an HTML
viewer, static screenshot, or the mockup's fictitious book database.

`ReaderGui.inc` declares library filters, continuation/progress, cover-marked
book rows, book details, contents, dismissible bookmarks, finished/removal
flows, font/size/spacing/margin controls and paged/scrolling layout. The shared
scene provider owns the reusable visual components and typography. A real
selected-font preview is copied into its bounded preview component.

Reading uses edge taps for page turns and center taps for the hidden controls.
The top overlay has back, bookmark and layout actions. The bottom overlay has
a ten-block scrubber, progress/remaining estimate, contents and typography.
Book page changes request CLEAN; idle pages are not resubmitted. Toggling the
controls retains the scene's bitmap and does not rerun book parsing or font
rasterization. Input capture remains under the existing shared scene lifecycle.

## Real content, settings and durable state

EPUB, TXT, Markdown, upstream pagination/shaping, CLX1 catalog, SD font families
and previous layout controls remain in the pinned CrossPoint engine. Advanced
settings retain alignment, paragraph/word/character spacing, indent,
hyphenation, embedded styles, images and rotation. There is one polarity
switch, not a theme subsystem. Watch RSVP and cross-device sync are not added.
The demo store's + action is a real SD browser.

Reading position and bookmarks keep their original binary record format.
New CRC-checked alternating sidecars `nova-status` and `nova-preferences`
store library flags, cached progress/estimates, display preferences and a
monotonic reading-session number. Existing position records are migrated
lazily; malformed records are not silently replaced. Last-read text identifies
the saved session rather than inventing a calendar date. Remaining time is
explicitly approximate: 250 words/minute, current-page word density and
upstream chapter-size weighting. It is not measured personal reading speed.

The latest actual reading session is promoted ahead of the catalog. Other
rows retain CrossPoint's arrival/title/author ordering, with eight visible
rows and bounded metadata residency. Removing a book hides it from the
library, not from the SD filesystem; reopening it from + restores it without
erasing bookmarks or position. Finished and reading filters are persistent.

Scrolling renders the actual text viewport across current/next pages, with at
most two page objects resident. A released swipe moves by its pixel distance;
it is not a timer-driven animation. Reflow, reopen and suspend recover the
visible source-text anchor. Chapter transitions and scrub positions are
resolved by the upstream engine, not estimated fixed lines of text.

## Integration

Pair Reader **0.1.6** with scene-host **0.3.5**. The app checks the optional
reading interface and shows UPDATE FIRMWARE on older providers. X4 0.1.75 has
scene 0.3.4 internally, so copying only the app to that firmware is insufficient.
The Reader SD identity remains `Apps/ebook-reader/ebook_reader.elf`. Keep the
existing SD fonts, books and `/System/State/Applications/ebook-reader` data.
No partition, panel selection, or full-flash installation is implied by this
source update. The matching scene is part of the next firmware assembly.

## Validation

Run `test_reader.py`, then `test_reader_startup.py`, `test_reader_gui.py` and
`test_reader_fatfs.py` with the same pinned Runtime/System/engine inputs.
`test_reader_gui.py` drives the actual app and provider through 28 intents and
captures the actual native 480x800 framebuffer. It does not render the HTML.
The fixtures are original synthetic fiction. Fonts stay local test/build
inputs, not visual-test artifacts.

Coverage includes EPUB3, nested EPUB2 NCX, TXT, Markdown, 3-spine scrolling,
reflow/suspend anchors, font preview, scrubber, bookmark mutation failure,
restart, finished/hide/re-add, legacy migration, corrupt sidecar safety,
production FatFs cache modes and the retained-page checkpoint regression.
Target compilation keeps the existing 2 MiB ELF and stack-frame budgets.
Physical display contrast, ghosting, timing and device interaction still need
hardware testing; host framebuffer captures do not claim those results.

Reference SHA256: `7fb94be2f133507dd0544078d2d500f66b6dd4048905ec52b7a9e15f912abb20`.

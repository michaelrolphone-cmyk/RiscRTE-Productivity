# Reader 0.1.8 — chapter and internal-link navigation

The 0.1.7 port retained CrossPoint's `Page::links`, but passed only a bitmap to
scene-host. The provider saw page-turn/chrome tap zones, not hyperlink geometry.
This was missing input wiring, not a document-parser failure.

The Reader now copies visible link regions and their original chapter/URI along
with each rendered bitmap. The scene 0.3.6 optional reading-links suffix handles
hit testing and returns a revision-bound region index. Links override page-turn
tap zones only on linked text. The toolbar, page swipes and scrolling remain
available. Back after an inline jump restores its source position; the bounded
16-entry history lasts for the current reader invocation, not across app exit.

Relative destinations resolve against the source XHTML path, including links on
the following page in scrolling mode. Exact normalized ZIP paths are compared;
a same-named chapter in a different directory is not substituted. Fragments are
split before percent decoding, and TOC fragments already decoded by CrossPoint
are not decoded again. External URLs and non-spine resources are not opened;
invalid internal destinations report an error rather than selecting page zero.

Chapter jumps now use Section::findAnchor, including its in-progress parser
index, instead of consulting only the finalized on-disk anchor map. This allows
an available anchored page to open before the rest of its chapter is paginated.
The supplied GUI's Contents screen continues to emit chapter-selection intents.

The 0.1.7 warm file checkpoints, glyph caches, small-read buffering, deferred
position persistence, clean page refresh and idle no-redraw behavior are retained.
There is no parser/cache format, bookmark format, SD font or panel-driver change.

## Tests and deployment

Run `scripts/test_reader_links.py` with the same --runtime, --system,
--engine-output and --output inputs as the GUI suite. Synthetic EPUB tests cover
relative links, duplicate basenames, same-file and escaped fragments, Contents,
warm/cold resume, all four text orientations, clipping and invalid destinations.
The app test uses actual touch DOWN/UP edges through the real scene provider,
not fabricated Reader intent events, and captures its native framebuffers.

For direct link tapping install matching X4 firmware containing scene-host 0.3.6,
and put Reader 0.1.8 at `Apps/ebook-reader/ebook_reader.elf` on the existing SD card.
Older scene 0.3.5 still supports the base Reader UI but lacks inline touch links.
No hardware execution or measured device timing is established by host tests.

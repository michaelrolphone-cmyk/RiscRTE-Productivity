# Reader 0.1.2 stack correction

The X4 0.1.73 device report for Reader 0.1.1 reaches `scene ready page=480x632
sd=ready`, then trips the `loopTask` stack canary while opening a book from File
Browser. The native firmware SHA prefix is `3b0605d518288202`; its task stack is
16,384 bytes. This app update does not change firmware, task size or drivers.

The exact Reader 0.1.1 ELF SHA-256 is
`52bcce6ba6228307f7eeb70338b4b2f8aa94cf417ac559ac1e8e0b3542a766e2`.
Its relocated text bias in this report is `0x43d32680`, determined from the
`App::openBook` call instruction and verified against the complete caller chain.
The key frames are:

| Function | Frame bytes |
| --- | ---: |
| `Engine::open` | 5,744 |
| FreeType `gray_convert_glyph` | 4,400 |
| `app_main` | 1,136 |

The path is `app_main -> App::openBook -> Engine::open -> loadSection -> step ->
Section::buildSomeMore -> XML parser -> text layout -> font metrics -> FreeType
rasterizer`. The reported stack-pointer span, including resident-shell/native
callers, is 18,032 bytes. The panic is in `gray_set_cell`; the font hinting message
is an informational build-option message, not the exception.

`state=BookState()` caused a full 5,644-byte temporary to occupy `Engine::open`'s
frame throughout nested pagination. Reset its fields and bookmark array in
place instead. Bookmark edits also previously copied the full state, including
an additional nested copy when toggling an existing bookmark. They now retain
only the affected bookmark and undo the list shift on a failed save. The saved
file format, bookmark capacity, font rasterizer and rendering quality are unchanged.

Target compilation now emits `.su` stack reports. The build checks `open` and
bookmark edit frames against 512-byte limits and the app entry against 1,536
bytes, and includes actual target measurements in `build.json`. These limits
catch this regression; they do not prove a bound for every possible upstream
parser/font call path.

Host tests cover failed bookmark add/toggle/remove/rename rollback and invoke
the real app and production scene with fresh fonts and EPUB3, EPUB2, TXT and
Markdown file-open handoffs. The latter run on a 32 KiB host pthread stack;
64-bit host frames and libc differ from Xtensa, so target reports remain the
architecture-specific evidence. Physical X4 testing is still required.

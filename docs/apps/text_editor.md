# Text Editor

## Purpose

Text Editor is a keyboard-oriented plain-text editor for RiscRTE. Its manifest identifies `text_editor.elf`, version **0.2.0**, minimum firmware **1.2.85**, categories `Productivity` and `Files`.

Supported file types declared by the manifest are:

- `.txt`
- `.md`

The manifest declares `usb.hid.keyboard >=1` as an optional capability. The current implementation automatically searches for that capability and retries if it is unavailable.

## RiscRTE interfaces

The app uses:

- `T5AppApi`
- `T5StorageApi`
- `T5FileOpenApi`
- `T5ProviderCapabilityApi`
- the USB keyboard capability interface returned for `usb.hid.keyboard`

Required `T5AppApi` functionality includes polling, presentation, drawing, screen geometry, labels, directory iteration, millisecond timing, and `set_back_exits_app`.

Storage requires `exists`, `read_file`, `write_file_atomic`, and `remove_file`.

`T5FileOpenApi::source_path_get` lets the editor open a path handed to it by a file-opening workflow.

The provider-capability API is used to acquire/release `usb.hid.keyboard` and to obtain detailed capability failure diagnostics when the host provides them.

## Editor model and limits

The local editing engine lives in `text_editor_core.h`.

Current fixed capacities are:

- Document buffer: **16,384 bytes**
- Clipboard: **2,048 bytes**

The editor supports cursor movement, range selection, replacement, backspace/delete, line-wise vertical movement, copy, cut, paste, and selection extension.

Imported files are validated before replacing the current buffer. The current implementation accepts printable ASCII plus tab/newline, rejects binary/non-ASCII content, and normalizes CRLF to LF.

## File workflows

The app supports:

- New document
- Open document
- Save
- Save As
- File-open handoff from another RiscRTE workflow
- Unsaved-change handling before destructive transitions or exit

The file picker enumerates documents and exposes `.md` / `.txt` files. Save As refuses to overwrite an existing different path and validates filenames before use.

Writes use `write_file_atomic`; the editor does not directly perform partial in-place writes.

## Keyboard workflow

The editor automatically retries acquisition of the `usb.hid.keyboard` capability rather than requiring the user to manually enable keyboard mode.

After acquisition it validates the returned keyboard ABI, subscribes to events, and handles connect/disconnect, key events, queue-gap recovery, and polling failures.

If acquisition fails, the app shows host-provided capability diagnostics when available and continues retrying.

Keyboard shortcuts implemented in the source include:

- Ctrl+S — Save
- Ctrl+Shift+S — Save As
- Ctrl+O — Open
- Ctrl+N — New
- Ctrl+A — Select all
- Ctrl+C — Copy
- Ctrl+X — Cut
- Ctrl+V — Paste

Normal editing supports arrows, Home/End, Backspace, Delete, Enter, and Tab.

## Rendering

The app draws its own editor surface using `T5AppApi`. It calculates visible rows/columns from screen dimensions, scrolls to keep the cursor visible, renders a dirty marker in the heading, and shows status/keyboard state in the footer.

When `present_serviced` is available, it uses that path while servicing keyboard collection during display update; otherwise it uses normal `present`.

## Exit and cleanup

The app disables default Back-to-exit so Back can participate in unsaved-change handling. On exit it unsubscribes the keyboard subscription and releases the provider capability lease.

## Source

- `Apps/text_editor.c`
- `Apps/text_editor.json`
- `Apps/text_editor_core.h`

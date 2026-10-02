# Text Editor

## Purpose

Text Editor is a keyboard-oriented plain-text editor for RiscRTE. Its manifest identifies `text_editor.elf`, version **0.2.3**, minimum firmware **1.2.85**, categories `Productivity` and `Files`.

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

File-open handoffs retain the full `/sd/...` storage path. Failed, short, or invalid reads preserve the current document and path for retry.

The file picker retains a directory cursor across pages of up to 64 `.md` / `.txt` names. Each pass examines at most 128 entries or 100 ms between directory calls, servicing events after eight entries or 10 ms. PgDn advances without rescanning the prefix; Home explicitly restarts. Back cancels, and exit or poll failure stops iteration. Cursor cleanup accompanies close and state transitions. Directory-open failures remain visible and can be retried with Home. The ABI does not distinguish end-of-directory from a directory-read error, and these between-call budgets cannot interrupt a blocking directory call. Save As refuses to overwrite an existing different path and validates filenames before use.

Discard reloads the saved document before continuing. A missing/unreadable, oversized, short-read, or invalid saved file leaves the edits in memory and keeps the unsaved-change prompt open. Discarding an unnamed document clears its buffer and file identity.

Failed writes preserve the dirty document for retry. Writes use `write_file_atomic`; the editor does not directly perform partial in-place writes.

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

## Migration verification

App C and helper sources remain unchanged; its manifest matches Reader master `00f9b2458dbfdae2188f2695634edb40c65c7ab0`. The current released RTE package SHA-256 is `6d3963b8e42460ed80a877b60796a93fadd72d44c4447b45fb7965a678798df0` (19,411 B); the nested ELF matches its previous 0.2.2 identity `54f609bf6e38190f5c5cde8b40da55420af4e3e354da42b182ccf99acb60e36f` (18,256 B). See [readiness and removal criteria](../MIGRATION_READINESS.md), [build instructions](../BUILD.md), and [byte-parity evidence](../release-parity.json). This establishes current-master source/build parity, not prospective U1 package/runtime acceptance.

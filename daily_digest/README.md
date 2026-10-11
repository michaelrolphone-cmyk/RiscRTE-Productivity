# Daily Paper for X4

Open Daily Paper. It connects using the existing saved Wi-Fi network, downloads the latest issue from `michaelrolphone-cmyk/Daily-Digest-Epub`, saves `/Books/Daily Digest/YYYY-MM-DD.epub`, and opens that file in the installed `ebook-reader` app. The date comes from the published edition, not the device's local date. Older days remain on the card. Running it again replaces only the same day's file after the new download has finished.

There is one activity/progress view, Cancel, and an error view with Retry and Close. No edition catalog, preferences screen, checksum calculation, or background download scheduler. The existing shared scene progress component is refreshed at most about twice per second. HTTP and SD operations are bounded and use existing capabilities; no application-created tasks or hardware drivers.

Downloads use a hidden `.YYYY-MM-DD.part` file, checked file close, and the existing filesystem replacement operation. Interrupted transfers do not become readable books and do not overwrite a previous complete issue. HTTPS retains normal certificate and hostname verification; set the device date/time and save a Wi-Fi network in Settings. The metadata SHA-256 field is ignored.

## Installation status

The target artifact separates `Apps/` (copy to SD) from `firmware-integration/` (internal build inputs, NOT an SD install). The app path is `Apps/daily-digest/daily_digest.elf`.

**Current X4 0.1.73–0.1.76 still registers new app identities, grants and launcher entries inside firmware. Copying this ELF alone onto an unchanged build cannot register Daily Paper.** The firmware needs its app manifest/policy, launcher entry and the small `net-http-client` provider included once. This provider forwards the already-existing bounded native HTTPS interface; it contains no newspaper-specific logic or TLS bypass. Existing Reader, fonts, saved books, display driver and state remain unchanged.

The Reader handoff uses `file.open@1`, with an absolute `/sd/Books/Daily Digest/YYYY-MM-DD.epub` source and `ebook-reader` handler. When Reader returns, the downloader consumes its completion result and exits instead of redownloading in a loop.

## Build and tests

Pinned SDK inputs are recorded in `.github/workflows/daily-digest.yml`. With Runtime and System checkouts beside Productivity:

```sh
python daily_digest/test.py --runtime ../Runtime --system ../System
python daily_digest/test.py --runtime ../Runtime --system ../System --sanitize
python daily_digest/build.py --runtime ../Runtime --system ../System --output build-daily
```

The target build requires the existing Xtensa ESP32-S3 8.4.0+2021r2-patch5 toolchain. It checks the 1536-byte frame budget, explicit ELF imports/exports and the repository's ELF structural validator. Host tests execute the actual app entry point against controlled capability implementations, including successful transfers, missing Wi-Fi/SD/clock, HTTP failures, cancellation, short writes, SD-full, interrupted replacement, Reader errors and normal Reader return. They do not claim physical X4 execution or a complete firmware assembly.

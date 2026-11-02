# Shared Daily Paper 0.2.0

This is one complete application ELF for X4, T-Watch and Waveshare. It retains the existing Daily Paper identity `daily-digest`, declarative scene UI, dated EPUB naming, transactional replacement, cancellation, retries and Reader return handling. There are no device-name or fixed-resolution branches in the application.

## Exact cumulative baseline

The product packages are a Daily-only overlay of `michaelrolphone-cmyk/RiscRTE-Watch` release `bringup-shared-233e43841483adf570ae833a88d9b6fc08d68f1a`. That published baseline already includes Shared Home and Contexts. Source pins, release asset hashes and reviewed authority are retained in `baseline.json`, `plan.json` and the qualification receipts.

X4's existing Daily Paper 0.1.0 came from Productivity `ba486e56dbf145a9c05d5bb36b39d9b81e0b7ece`; its complete Daily source tree is identical to the main baseline used here. CI measured its actual installed SD ELF as 27,656 bytes, SHA-256 `72a8ef9d1c7eb087e889bbc717c635cdb59a4dd6e7d97d3500971c6b743381aa`. The original X4 SD route and seven capability grants, including saved-network namespace 6, are preserved. The SD catalog manifest and internal reservation both describe the new foreground application.

Watch and Waveshare did not previously contain Daily Paper. Their new internal application uses only reviewed existing grants. Watch does not expose an SD volume for this app; its internal package volume is not misrepresented as an SD card. Neither baseline supplies the Daily HTTPS provider. The app reports unavailable services rather than claiming download support. No credential namespace ownership is reassigned to the new principals.

## What does not change

All unrelated canonical application/provider bytes and saved-data ownership are preserved, including the currently published USB and Reader implementations. The separately qualified but unpublished USB/Reader successors are not included. Any lossless ordinary-application envelope encoding needed to preserve the existing fixed SPIFFS size and free-block reserve is recorded and round-trip checked; canonical bytes remain unchanged.

Native scene providers own dimensions and presentation. This release does not add hardware providers, change native firmware, fabricate working hardware, or migrate saved credentials. Existing books under `/Books/Daily Digest` remain intact; same-day replacement commits only after a complete successful transfer.

## Installation and evidence

These files are companion application stores and SD packages, not full initial flash images. Keep the exact published baseline native firmware and use its data-preserving installation process. Do not bulk-flash over user data or substitute another product's store.

The standalone `daily_digest.elf` and `daily_digest.json` do not independently create firmware policy or missing hardware. Install the matching product package/manifest policy. Host tests, target import/export and stack-frame checks, repeated byte-identical builds, fixed-size store/SD round trips, and actual owning Runtime graph/catalog admission are retained separately from physical testing. No physical hardware result is claimed.

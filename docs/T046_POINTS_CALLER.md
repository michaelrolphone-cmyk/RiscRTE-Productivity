# T046 Points caller recovery

The actual Watch 1.1.29 and Wave 0.1.32 Points executable (97f8128b, version
0.5.3) predates the shared-input migration. Restore the previously qualified
519dba5 caller plus 99d7067 modal policy. Relative to its 0b8667d source,
only Points controller/shared-input integration changed; writer, schedule,
custom colors, persistence and picker features remain.

The opt-in Wave build accepts its physical 410x502 display and maps app contacts
with the same width-based transform used by its existing native-height adapter.
Shared text entry continues to own the full physical display. The Watch profile
retains its strict 240x240 gate. This does not change the X4 catalog-based app.

The extended production-app fixture runs normal and ASan/UBSan against the final
shared providers at both dimensions, retaining schedule, save, cancel, repeated
input, custody, modal background and sleep checks. Physical devices remain untested.

# Reader 0.1.1 startup fix

Reader 0.1.0 queried page geometry before opening its first scene. The selected
scene provider establishes its dimensions in `components.open`, so a fresh
provider returned `RISC_SCENE_UNAVAILABLE` and Reader immediately returned from
`app_main`. ELF loading and module initialization both succeeded.

Reader 0.1.1 opens a progress scene before querying geometry and initializing
the reading engine. It services that scene during font and library preparation,
then updates the same scene with the library or an SD error. Failed capability
acquisition, scene setup and geometry queries now emit startup diagnostics.

The fix is an app-only update compatible with X4 0.1.73 and its existing scene,
storage and panel providers. Replace `Apps/ebook-reader/ebook_reader.elf` on SD
while Reader is closed. The accompanying SD manifest records the new version;
the firmware's static catalog remains unchanged until the next firmware build.

Run `scripts/test_reader.py` to build the host engine, then
`scripts/test_reader_startup.py --runtime R --system S --engine-output E --output O`.
The startup test executes the real Reader entry point against the production
scene provider with its display/input fixture and a filesystem-backed SD fixture.
It starts with no open scene, checks rotated X4 page geometry, reaches the library,
and verifies exit cleanup. The previous Reader source fails this test before any
scene opens. Host checks do not substitute for device testing.

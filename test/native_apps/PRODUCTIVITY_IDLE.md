# Production-profile automatic idle regression

Run from the Productivity repository after a successful combined Points and
Timecard build (paper transitions, touch scrolling, BLE telemetry and X4 idle):

```sh
python3 scripts/test_productivity_idle.py --recipes /path/to/cohort/recipes.json
```

The runner reads each successful recipe's output directory. It verifies pinned
dependencies, the selected System revision, app and adapter source hashes, and
the final `idle-sdk/include` header hashes. It reuses the production defines,
include order, and sources, replacing only the app entry point/catalog with the
existing controller fixture and the native sleep helper with a result stub.
`quick_radios.c`, the native custody adapter, controller, scrolling, telemetry
client, Quick Controls and native time helpers are production implementations.

Ten scenarios run for both apps, in portrait and flipped orientation, with a
normal build and combined ASan/UBSan build (80 executions):

- Unsaved editor and scrolled list state survive both Light success and clean
  refusal. Timecard's keyboard is unscrollable, so its keyboard draft and list
  scroll preservation are tested separately.
- Native retention and telemetry pause failure prevent subsequent polling,
  rendering, saving, releases and cleanup I/O.
- Live contact and pending display transfer defer automatic sleep.
- A held wake contact and its release cannot become an editor action.
- Telemetry pauses and releases its transient grant, touch unsubscribes, and
  navigation resets before the helper. Normal polling may resume telemetry
  only according to saved policy.
- Saved Bluetooth/Wi-Fi intent survives automatic idle. The adapter does not
  apply manual-sleep Bluetooth shutdown before the helper; afterward the real
  Quick radio controller restores saved intent. Wi-Fi auto-connect is forbidden,
  and saved Off remains Off.

The fixture uses the normal 60-second timer and 70% battery. It does not simulate
audio or radio capture roles, which these production profiles do not contain.
Native Light sequencing, low-battery crossings, the real Runtime grant cap, and
physical-device wake/energy behavior belong to their independent tests.

The machine-readable receipt in `build/productivity-idle/receipt.json` records
each execution, production evidence identities, test hashes and host compile
commands. `--app` and `--case` can narrow a diagnostic run. Default/Watch build
profiles and all production sources are unchanged.

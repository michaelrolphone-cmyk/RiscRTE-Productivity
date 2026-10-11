# Recovered Productivity build handoff

Points0.6.13 is recovered .6.12 plus the approved Home discard/exit fix. Timecard0.2.11 application source is unchanged: all10 source dependencies match the installed .47 receipt. No saved ELF/BIN is an assembly input.

Use a fresh output directory for every invocation. Set PLATFORMIO_SETTING_ENABLE_TELEMETRY=No and NATIVE_APP_CC to the verified Xtensa GCC8.4.0 (2021r2-patch5) compiler.

Points:
python3 scripts/build_x4_home_points.py --common-system --baseline-target /workspace/shared/points-source-recovery/051/source-validation/targets/points/points_in_time --baseline-sdk /workspace/shared/points-source-recovery/051/source-validation/targets/points/sdk --system /tmp/system-ui-state-decoupling-20261010 --runtime /workspace/shared/recovered-runtime-0106 --utilities /workspace/shared/points-home-0613/source/baseline-utilities --output NEW_POINTS_OUTPUT --reservation /workspace/shared/points-home-0613/proof/version-reservation.json

The baseline-target directory supplies the frozen JSON profile only. The recipe never reads its old ELF. The baseline SDK contains recovered header/font source, not executable modules. The common-system option stages all selected System/Runtime headers before compiling every selected translation unit.

Timecard:
python3 scripts/build_x4_recovered_timecard.py --baseline-custody /workspace/shared/gameboy-baseline-recovery/baseline-047-build-custody.json --display-sdk /workspace/shared/points-source-recovery/051/source-validation/targets/points/sdk/include --system /tmp/system-ui-state-decoupling-20261010 --runtime /workspace/shared/recovered-runtime-0106 --utilities /workspace/shared/points-home-0613/source/baseline-utilities --output NEW_TIMECARD_OUTPUT

Qualified common source: System3c3b2e031a8f21928aa8b53f29a580b4efa58a93, Runtime274bc66f193cbe29018d2a85c9400cb0dce8aacc. Recompile and rerun if those production bytes change. Points98 and Timecard60 exact-profile normal/sanitized host cases pass. Text0.1.2 is built separately from the same common System source; its Home suffix must be included for keyboard Home exit. No hardware qualification is claimed.

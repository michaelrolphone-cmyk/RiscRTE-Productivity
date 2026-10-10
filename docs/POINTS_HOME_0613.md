# Points Home 0.6.13 working checkpoint

User-selected behavior: Home discards unsaved local drafts and exits to the live default host. Back retains editor cancellation. No draft persistence is added.

Recovered baseline: exact Points 0.6.12 tree d78de5c647212f34694987a9fedeb0c6d5a14de1, from source commit53e0a82bca754a3d302c2cefd26cb596a8734bab in the .51 source archive. This source also supplied .53 and .54.

A compile-time PORTABLE_APP_HOME_GUARD callback distinguishes physical Home from the existing launch/controls guard. The companion focused System patch adds the call only to physical Home/root routing. It must be supplied when building this app. The text provider’s opt-in Home-reason suffix distinguishes shared-keyboard Home from Back without changing old request/result layouts or flags for legacy clients.

Fresh deterministic reproduction: initial Home worked on main/plain Types, but seven draft pages refused it. After the focused change, 98 resident host cases pass normal/ASan/UBSan, including app_main invocation, nine non-keyboard pages, shared keyboard Home, pending close, close retention, uncertain-save no-rewrite, existing Back/navigation, and repeated activation. These are host tests, not physical hardware qualification.

Clean Xtensa target compilation and strict loader validation now pass. The exact target profile passes98 resident host cases. Companion text0.1.2 passes512 real Runtime .100 scenarios across X4/Watch normal/ASan/UBSan. Runtime .106 and the reconciled common System source remain to be qualified. Final composition admission and physical testing are pending. No flash image is supplied. Do not install from this checkpoint.

The checkpoint branch keeps source at normal production paths. Historical PNG evidence is retained in Library archive libfile_da61db0f22d881919435c472f1efa4ba; it is omitted from this recovered source checkpoint. The clean build recipe never uses an old product ELF as input.

# Xoxicoa FX
Creative multi-effect (v1.2): Ghost Double, Plugg Haze, Rage Stutter,
808 Phone-Proof, Mosh Meter, Sidechain Pump, and a marked monitoring-only TikTok Check,
plus a preset browser with 12 factory presets and user presets.

Read SPEC.md for every control and honest limitations.
Read BUILD_ON_MAC.md for Mac build/install/FL Studio scan steps.
`.github/workflows/build-mac.yml` prepares universal AU/VST3/Standalone artifacts.
`TEST_RESULTS_LINUX.txt` records built VST3 tests, not Mac or listening validation.
`tests/UISnapshot.cpp` renders the real editor; `tests/RenderTest.cpp` checks audio;
`tests/Audit.cpp` tests Rage Stutter and sweeps every parameter (see CHANGELOG.md).

C++17 / CMake >=3.22 / JUCE 8.0.4 (fetched on configure).
All cards start OFF. Turn PREVIEW OFF before realtime capture.
No repository pushed or created. JUCE license review required before distribution.

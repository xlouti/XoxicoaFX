# Xoxicoa FX
Creative multi-effect prototype: Ghost Double, Plugg Haze, Rage Stutter,
808 Phone-Proof, Mosh Meter, and a marked monitoring-only TikTok Check.

Read SPEC.md for every control and honest limitations.
Read BUILD_ON_MAC.md for Mac build/install/FL Studio scan steps.
`.github/workflows/build-mac.yml` prepares universal AU/VST3/Standalone artifacts.
`TEST_RESULTS_LINUX.txt` records built VST3 tests, not Mac or listening validation.
`tests/UISnapshot.cpp` renders the real editor; `tests/RenderTest.cpp` checks audio.

C++17 / CMake >=3.22 / JUCE 8.0.4 (fetched on configure).
All cards start OFF. Turn PREVIEW OFF before realtime capture.
No repository pushed or created. JUCE license review required before distribution.

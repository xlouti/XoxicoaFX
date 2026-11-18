# Install Xoxicoa FX in FL Studio for Mac

This is a prototype. Linux VST3 and Standalone compiled and the VST3 passed
synthetic audio tests. **Mac build and FL Studio validation have not run here.**
The Linux binary will not work on a Mac. Build from source or use the prepared
GitHub workflow. No repository has been created or pushed.

## Build directly on your Mac (no GitHub required)

1. Unzip `XoxicoaFX_source.zip`. Put the `XoxicoaFX` folder on your Desktop.
2. Open Terminal and install Apple's Command Line Tools:

```sh
xcode-select --install
```

Finish Apple's installation window. Already installed? Continue.
Install CMake's universal macOS `.dmg` from https://cmake.org/download/ and drag
CMake into Applications. Neither Homebrew nor a paid Apple account is required.

3. Run these commands in Terminal:

```sh
cd "$HOME/Desktop/XoxicoaFX"
export PATH="/Applications/CMake.app/Contents/bin:$PATH"
cmake --version
export MACOSX_DEPLOYMENT_TARGET=10.15
cmake -S . -B build-mac -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
cmake --build build-mac --config Release --parallel 2
```

CMake fetches JUCE 8.0.4 from its official repository; internet required.
Allow several minutes. If Apple's SDK is missing/outdated, install updates for
Command Line Tools in Software Update, or install full Xcode from the App Store.
This universal build supports Intel on macOS 10.15 or later and Apple Silicon on macOS 11 or later.

4. Quit FL Studio, then sign and install personal copies:

```sh
cd "$HOME/Desktop/XoxicoaFX"
ROOT="build-mac/XoxicoaFX_artefacts/Release"
codesign --force --deep --sign - "$ROOT/AU/Xoxicoa FX.component"
codesign --force --deep --sign - "$ROOT/VST3/Xoxicoa FX.vst3"
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
ditto "$ROOT/AU/Xoxicoa FX.component" \
      "$HOME/Library/Audio/Plug-Ins/Components/Xoxicoa FX.component"
ditto "$ROOT/VST3/Xoxicoa FX.vst3" \
      "$HOME/Library/Audio/Plug-Ins/VST3/Xoxicoa FX.vst3"
```

`ditto` replaces an older Xoxicoa FX at those exact paths if one exists.

5. Open FL Studio > **Options > Manage plugins** (Plugin Manager). Enable
**Verify plugins**, then **Find installed plugins**. Include the user-library
folders above if needed. Favorite **Xoxicoa FX** and load it into a **Mixer insert
effect slot**, not as an instrument. Start with VST3; don't load AU and VST3 together.
Use rescan-already-verified/error options if FL has cached an earlier build.

Optional architecture check:

```sh
lipo -info "$HOME/Library/Audio/Plug-Ins/VST3/Xoxicoa FX.vst3/Contents/MacOS/Xoxicoa FX"
```

Expect `arm64` and `x86_64`. Native FL loads arm64; Intel/Rosetta FL loads x86_64.

## Let GitHub build it instead

If you choose to create a GitHub repository, upload this folder's **contents**
at its root, including hidden `.github/workflows/build-mac.yml`. Do not upload
build folders. Go to Actions > **Build Xoxicoa FX for Mac > Run workflow**.
Download the successful `XoxicoaFX_mac_universal` artifact. Unzip it, then unzip
the inner `XoxicoaFX_mac_universal.zip`. The `package` folder contains the component,
VST3 and standalone app. Copy component/VST3 to the Library folders above and rescan.
The workflow checks both architectures before uploading. It has not run yet.

Artifacts are ad-hoc signed, **not notarized**. macOS may block downloaded plugins.
Only if you trust your own build, approve through System Settings > Privacy &
Security. Do not disable Gatekeeper globally. A local source build is another
option. Public releases need proper Developer ID signing/notarization and testing.

## First beat / safety

All modules start OFF. Try Mosh at 20–35% on a Serum lead or Phone-Proof at 25–45%
on an 808. Ghost at 15–25% Mix adds a shadow. Haze tracking works best on single
notes, not chords. See SPEC.md for every control.

**TikTok Check is only a rough monitoring simulation. Turn PREVIEW OFF before
recording, real-time bouncing, streaming or exporting.** It resets OFF on project
load and is forcibly disabled in host-reported offline exports, but cannot know
whether you're listening or making a realtime recording. Hosts that fail to
report offline rendering defeat automatic protection too.

## Development / verification

Normal Linux dependencies (Debian/Ubuntu example):

```sh
sudo apt-get install build-essential cmake ninja-build pkg-config \
  libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev \
  libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DXFX_BUILD_TESTS=ON
cmake --build build-linux --parallel 2
./build-linux/XoxicoaFXRenderTest_artefacts/Release/XoxicoaFXRenderTest \
  "$PWD/build-linux/XoxicoaFX_artefacts/Release/VST3/Xoxicoa FX.vst3" "$PWD/test-renders"
./build-linux/XoxicoaFXUISnapshot_artefacts/Release/XoxicoaFXUISnapshot "$PWD/ui.png" 1.5
```

The screenshot tool renders the real editor; it needs a working display (Xvfb
works). Render test loads the actual built VST3 and supplies an 8-bar 160 BPM
synthetic loop. On Mac the optional test targets are also available through
`-DXFX_BUILD_TESTS=ON`. A test pass is not a listening test or FL host certification.

## JUCE license

JUCE 8 is dual licensed AGPLv3/commercial. Dependency source isn't bundled; CMake
fetches it. Review https://juce.com/legal/juce-8-licence/ and JUCE's LICENSE.md before
selling, distributing binaries or releasing a closed-source version. No commercial
JUCE license is granted by this package. No third-party sounds/art are included.

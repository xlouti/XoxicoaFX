# Changelog

## v1.2 (2026-11-17)
### New module 06: Sidechain Pump
- Tempo-synced ducking at 1/4, 1/8 or 1/2 notes, locked to the host's song position, or
  **Kick Trigger** mode, which ducks on every kick the built-in kick detector finds. It reads the
  external sidechain input if one is connected, otherwise the track itself.
- Knobs: Depth (0–100 %), Release (sync: 10–100 % of the step; kick: 40 ms–600 ms), Shape
  (linear to hard "hold then snap back"), plus the Mode menu. A live curve display shows the
  gain shape and a playhead dot. Sync modes don't duck while the transport is stopped.
- New parameters: `pump_on`, `pump_mode`, `pump_depth`, `pump_release`, `pump_shape`. Existing
  IDs are unchanged, so v1.0 and v1.1 projects load with the pump off.

### Preset browser
- Header bar: previous and next arrows, a name menu (FACTORY and USER sections, plus "Open presets
  folder"), SAVE and DELETE. DELETE only works on user presets.
- User presets are .xfxpreset files in `~/Library/Audio/Presets/Xoxicoa/Xoxicoa FX/` on Mac
  (JUCE user app data path). Factory names are protected. Preview is always forced off on load.
  The preset name is saved with the project.
- 12 factory presets: Init, Glokk Mosh, Plugg Wash, TikTok Ready, Rage Roll Fill,
  Half-Time Drop, Ghost Lead, 808 Phone Punch, Pump It, Kick Duck, Stutter Chop, Dark Tape.

### UI
- Window is 1280×760. Top row: 01 Ghost, 02 Haze, 03 Stutter. Bottom row: 04 Phone,
  05 Mosh, 06 Pump, 07 TikTok Check. Same black-first look and responsive knobs as v1.1.

### Audit
- The per-knob min/mid/max audit now covers all 26 parameters, including the pump. 0 are flagged.
  See TEST_RESULTS_LINUX.txt.
- CMake project version bumped to 1.2.0.

## v1.1 (2026-11-17)
### Rage Stutter: rewritten
- Old: random glitch fill on only the last beat (4 steps); a 1–128 "Pattern" seed that
  mostly sounded the same; transient snapping + 1.5 ms fades that buzzed.
- New: host-synced beat slicer. Records the bar into a PPQ-locked history and plays a
  rearranged 16-step sequence of 1/16 slices (repeats, beat swaps, half-time, reverses,
  1/8-1/64 rolls, gated chops, tape stops). 26 named patterns, 5 modes, deterministic
  Random swap, 4 ms crossfades at every cut. Source = current bar if already played,
  else the same spot one bar earlier. Dry when the transport stops.
- Parameters: `stutter_bars` and `stutter_seed` retired. New `stutter_mode` (choice) and
  `stutter_pattern` (choice of names). `stutter_on`, `stutter_random`, `stutter_mix` kept
  (Random now means "chance to swap pattern per bar"; default 0. Mix default 100%).

### Monochrome UI redesign
- Black / white / grey only. Flat numbered module panels, consistent spacing, value
  readouts under every knob, 16-step pattern map for the stutter, mono level meter.
- Knobs: velocity-sensitive drag, Shift = fine, double-click or Alt/Cmd-click = default,
  mouse wheel (Shift = fine), hover/drag value bubble. Knobs grey out when their module is off.

### Every knob audible (per-parameter audit, tests/Audit.cpp)
- Ghost: Formant now ±9 dB shelves + 8 dB moving peak; Width 0 = mono .. very wide;
  Delay range 10–40 ms -> 5–80 ms; Mix default 25% -> 40%.
- Haze: Key/Scale were nearly inert (Scale measured exactly 0 change). Off-key notes now
  get up to 2.2x deeper, leaning wobble. Wow 30 -> 45 cents, flutter 3 -> 5 cents,
  Tape drive 5x -> 10x, Tone 2–18 kHz -> 0.6–18 kHz (default 70% -> 80%).
  Haze delay centre 10 -> 15 ms, so the plugin's reported latency is 726 samples at 48 kHz.
- Phone-Proof: Tone 0.7–2.2 kHz -> 0.25–4 kHz; Brightness spread widened.
- Smoothing: all continuous params glide in 20–25 ms (was up to 60 ms). Formant, Haze
  Tone and Phone Tone/Brightness are smoothed and their filters refreshed every 32 samples
  (no zipper noise on sweeps).

### Compatibility notes
- Saved FL projects load. The old stutter "every 4/8 bars" and "seed" values are ignored
  (mode falls back to Fill: Last Bar of 4, pattern to Rage Roll Buildup).
- Ranges widened for ghost_delay, haze_tone (same IDs). Saved values load as the same
  number, but haze_tone 0.7 now means about 6.6 kHz instead of 9.3 kHz. FL automation
  clips are stored normalised, so automated ghost_delay sweeps cover the new wider range.

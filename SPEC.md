# Xoxicoa FX — producer spec (v1.2)

A dark purple creative effect panel for rage + pluggnb. Put it after Serum or your
808 on an FL Mixer insert. It is an effect, not an instrument. All six modules
start OFF. Knobs save with the project and are automatable. PREVIEW is deliberately
not automatable and resets OFF on project load.

**Flow:** Ghost Double → Plugg Haze → Rage Stutter → 808 Phone-Proof → Mosh Meter
→ Output gain → TikTok Check monitoring preview → safety limiter.

## Ghost Double
A dark pitched-down shadow under your original sound.
- **ON:** enables the shadow.
- **Pitch:** −24 to +12 semitones; default −12 (octave down).
- **Formant:** −12..+12 colour range (±9 dB shelves + a moving 8 dB resonance); lower = bigger/darker, higher = smaller/brighter.
- **Delay:** 5–80 ms (slapback range at the top), plus the shifter's moving grain window.
- **Width:** 0% = mono ghost, 100% = very wide (offset right-side delay + side boost).
- **Mix:** 0–100% added shadow; original stays present. Default 40%.

Short-grain pitch shifting can smear/chorus on complex sound. Formant is EQ-based
colour, **not true formant preservation**. Best on a single-note lead/voice texture.

## Plugg Haze
Tape wow/flutter that leans toward notes of your chosen key.
- **ON:** enables wobble, tone and tape colour.
- **Depth:** 0–100%; up to roughly 45 cents wow + 5 cents flutter (less at slow rates).
- **Rate:** 0.1–8 Hz main wobble speed.
- **Key:** C..B, sharps shown.
- **Scale:** Minor, Major, Harmonic Minor, Minor Pentatonic, Major Pentatonic,
  Phrygian, Dorian.
- **Tone:** lowpass, 600 Hz (muffled) to 18 kHz (open). Default 80% (about 9.6 kHz).
- **Tape:** 0–100% soft asymmetric saturation (drive up to 10x).

Tracks a monophonic input around 60–1200 Hz. Notes that sit off the chosen
key/scale get a deeper wobble (up to 2.2x) that leans toward the nearest scale tone;
in-key notes get the plain wobble. So Key/Scale only change notes the scales disagree on. **Not pitch correction**:
it won't force a melody in key or tune chords. Chords/noise can confuse tracking.
This is a simplified musical drift effect, not guaranteed harmonic retuning.

## Rage Stutter (v1.1 rewrite)
Host-synced beat slicer that rearranges the groove in 1/16 slices.
- **ON:** enables it; requires playback + BPM/song-position data (dry when stopped).
- **Mode:** Every Bar / Every 2 Bars / Fill: Last Bar of 4 / Fill: Last Bar of 8 /
  Last Beat of 4 Bars (only the last beat of every 4th bar is sliced).
- **Pattern:** 26 named, hand-designed 16-step patterns (see list below). The
  16-step map under the knobs shows what each step plays; the playhead lights up
  while slicing.
- **Random:** 0 = always the chosen pattern. Above 0, each sliced bar has that
  chance of swapping in a different pattern; the choice is deterministic per bar.
- **Mix:** sliced versus dry, 0–100%.

Patterns: Beat 1 Repeat, Half-Time, Half-Time Back Half, Skip 2 & 4, Beat Flip
(2-1-4-3), Beat Shuffle (1-3-2-4), Half-Bar Repeat, Reverse Last Beat, Reverse 2 & 4,
Reverse Back Half, Reverse Swell, 1/8 Retrigger Roll, 1/16 Roll Last Beat, 1/32 Snare
Roll, Rage Roll Buildup (default), Machine Gun 1/64, Downbeat Stutter, Triplet Skip
(3-3-3), 8th Repeat 2 & 4, Plugg Skip, Gated Chop 1/16, Stop-Start, Drop Silence,
Tape Stop End, Tape Stop Half, Half-Time Into Stop.

How the source is found (no lookahead in a live plugin): incoming audio is recorded
into a 16 s history locked to the host PPQ. A segment whose source lies at or before
"now" reads the current bar (e.g. "repeat beat 1" during beat 3); otherwise it reads
the same spot in the previous bar, exactly one bar earlier (e.g. "beat 4 reversed" at
beat 4). On a looping beat that is the same material. Sources from before playback
started fall back to dry, so the first bar after pressing play may be partly dry.
Every slice change is a 4 ms crossfade; rolls crossfade each repeat. Slices are at
least 1/16 except deliberate 1/32 and 1/64 rolls. Follows the host time signature
(16 steps per bar). Standalone has no song transport, so it stays dry there.

## 808 Phone-Proof
Adds pitch-following harmonics so tiny speakers can suggest the bass note.
- **ON:** enables the added harmonic layer.
- **Amount:** strength, 0–100%. Start at 25–45%.
- **Brightness:** balance lower/higher harmonics.
- **Tone:** rolls off the added layer, 250 Hz (dark) to 4 kHz (bright).

Tracks low end around 28–180 Hz (best 30–150), waveshapes the isolated low band
into 2nd–7th harmonics, then bandpasses each around a multiple of detected pitch.
Added layer is mainly above 150 Hz. Original sub isn't filtered. Tracking takes
tens of milliseconds and can misread kicks/chords; unconfident input falls back
to untuned harmonics. Excessive Amount triggers the final limiter, which then
changes the entire signal, including the sub. Can't make phones reproduce true sub.

## Mosh Meter
Drive, clipping and stereo width from one macro.
- **ON:** enables the effect.
- **Mosh:** 0–100%, clean to saturated/clipped/wider.
- **Kick-Follow:** 0–100%, extra drive on low-end hits.

Uses a lowpassed external sidechain when supplied; otherwise its own low end,
which might be the 808, not just the kick. Nonlinear processing runs at 4x sample
rate to reduce aliasing. Width expands existing stereo, not a mono widener.
Extreme values intentionally get destructive. No automatic volume matching.

## TikTok Check — PREVIEW ONLY
Rough phone speaker + compressed loudness check, not TikTok's actual codec.
- **PREVIEW ON/OFF:** large red switch; default OFF, not automatable.
- **Crunch:** optional sample-rate/bit-reduction flavour, 0–100%.

Mono fold, ~280 Hz highpass, mid boosts, ~10 kHz lowpass, heavy compression and
slow level riding toward a −14 dBFS RMS *feel*. **Not LUFS measurement**, platform
normalization or an accurate codec/phone model. Big flashing PREVIEW indicator.

**Always turn it OFF before recording, realtime bouncing, streaming or exporting.**
It resets OFF on project/preset load and forcibly bypasses in host-reported offline
exports (without a preview fade tail). It cannot distinguish listening from a
realtime recording. A host that doesn't flag offline mode also defeats that guard.
It lives on the insert output, not a separate hardware monitor bus.

## Sidechain Pump (v1.2)
Ducks the signal, after Mosh and before the output gain.
- **Mode:** 1/4, 1/8 or 1/2 (synced to the host's song position, latency compensated) or **Kick Trigger**.
  Kick detector: 140 Hz low-pass ×2, fast and slow envelope comparison, 80 ms hold-off. It listens to the
  external sidechain if one is connected, otherwise the track.
- **Depth:** how far the level dips, 0–100 %. At 100 % the dip goes to silence.
- **Release:** how long the recovery takes. Sync modes: 10–100 % of the step. Kick mode: 40–600 ms.
- **Shape:** gain = 1 − depth·(1 − x)^k with k = 0.6…4.0. Low values recover early and smoothly.
  High values hold the dip, then snap back (classic EDM or rage pump).
- Gain is smoothed in 1.5 ms, so there are no clicks. Sync modes do nothing while the transport is stopped.
- Curve display: the shape from the current settings, plus a live position dot.

## Presets (v1.2)
- Header: ◀ ▶ step through factory then user presets. The name menu lists FACTORY and USER
  and has "Open presets folder". SAVE asks for a name. DELETE removes the current user preset (with a confirmation).
- Folder: `~/Library/Audio/Presets/Xoxicoa/Xoxicoa FX/` (Mac), `.xfxpreset` (XML of the
  full parameter state). Preview is always off after a load.
- Factory: Init, Glokk Mosh, Plugg Wash, TikTok Ready, Rage Roll Fill, Half-Time Drop,
  Ghost Lead, 808 Phone Punch, Pump It, Kick Duck, Stutter Chop, Dark Tape.

## Output / shared controls and limits
- **Output:** −24..+6 dB, before preview/limiter.
- **Meter:** stereo sample peaks/RMS, peak hold, limiter reduction.
- **Safety limiter:** always-on, stereo-linked −0.3 dBFS sample-peak ceiling.
  Not true-peak: intersample overs can occur. Not a professional mastering chain.
- Mono/stereo main input/output, optional mono/stereo external sidechain.
- Fixed reported latency = Haze's 15 ms + Mosh oversampling delay, including when
  modules are off. At 48 kHz: **726 samples / 15.1 ms** (v1.0 was 486). Host PDC aligns it.
- User-written process loops allocate no memory; buffers allocated in prepare.
  Oversized host blocks processed in chunks; denormals suppressed.
- Parameters glide in 20–25 ms (filters sweep smoothly, refreshed every 32 samples).
  Discrete choices (key, pattern, mode) switch at once. Not sample-accurate automation.
- Fixed 1280×760 monochrome UI with 7 module cards and a preset bar. Knobs: drag (faster drag = bigger moves), Shift = fine,
  double-click or Alt/Cmd-click = default, mouse wheel (Shift = fine), value bubble on hover.
  No installer or undo yet.
- Linux VST3 + Standalone compiled; synthetic audio checks passed. Mac workflow
  prepared but not run. Needs Mac/FL host testing and listening on real music.
- Established DSP combined into a personal tool; no claim of novel/patentable
  technology, AI, or TikTok endorsement.

## Starting points
Rage lead: Ghost −12 / Mix 20%, Mosh 30%, Stutter Fill: Last Bar of 4 / Rage Roll Buildup / Mix 100%.
Pluggnb lead: Haze in your song's key / Minor, Depth 20%, Rate 0.7 Hz, Tape 15%.
808: Phone Amount 30%, Brightness 50%, other modules off. Compare at matched levels.
Pump: "Pump It" preset (1/4, Depth 75%) for a classic pump, or "Kick Duck" (Kick Trigger) to carve the lead around the kick.

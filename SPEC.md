# Xoxicoa FX — producer spec / prototype v1

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
- **Formant:** −12..+12 colour range; lower = bigger/darker, higher = smaller/brighter.
- **Delay:** 10–40 ms, plus the pitch shifter's moving grain window.
- **Width:** stereo spread of the ghost, 0–100%.
- **Mix:** 0–100% added shadow; original stays present.

Short-grain pitch shifting can smear/chorus on complex sound. Formant is EQ-based
colour, **not true formant preservation**. Best on a single-note lead/voice texture.

## Plugg Haze
Tape wow/flutter that leans toward notes of your chosen key.
- **ON:** enables wobble, tone and tape colour.
- **Depth:** 0–100%; up to roughly 30 cents wow + subtle flutter, less at slow rates.
- **Rate:** 0.1–8 Hz main wobble speed.
- **Key:** C..B, sharps shown.
- **Scale:** Minor, Major, Harmonic Minor, Minor Pentatonic, Major Pentatonic,
  Phrygian, Dorian.
- **Tone:** lowpass brightness around 2–18 kHz.
- **Tape:** 0–100% soft asymmetric saturation.

Tracks a monophonic input around 60–1200 Hz. Key bias skews how much time the
wobble spends sharp/flat toward the nearest scale tone. **Not pitch correction**:
it won't force a melody in key or tune chords. Chords/noise can confuse tracking.
This is a simplified musical drift effect, not guaranteed harmonic retuning.

## Rage Stutter
Auto-fill in the last beat of each 4th/8th bar, synced to the host.
- **ON:** enables fills; requires playback + BPM/song-position data.
- **Fill every:** 4 or 8 bars.
- **Random:** deviation from the default fill.
- **Mix:** fill versus dry, 0–100%.
- **Pattern:** deterministic seed, 1–128. Same bar/settings picks the same pattern.

Transient-aware 1/16 + 1/32 repeats, reverse, tape-stop; short crossfades at cuts.
Follows host time signature (beat = denominator beat). Standalone has no song
transport, so fills don't run there. Pattern choices repeat, but starting playback
at a different point changes available audio history/transients and thus slices.
Algorithmic, no machine learning. No fills during negative-position pre-roll.

## 808 Phone-Proof
Adds pitch-following harmonics so tiny speakers can suggest the bass note.
- **ON:** enables the added harmonic layer.
- **Amount:** strength, 0–100%. Start at 25–45%.
- **Brightness:** balance lower/higher harmonics.
- **Tone:** rolls off added layer above about 0.7–2.2 kHz.

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

## Output / shared controls and limits
- **Output:** −24..+6 dB, before preview/limiter.
- **Meter:** stereo sample peaks/RMS, peak hold, limiter reduction.
- **Safety limiter:** always-on, stereo-linked −0.3 dBFS sample-peak ceiling.
  Not true-peak: intersample overs can occur. Not a professional mastering chain.
- Mono/stereo main input/output, optional mono/stereo external sidechain.
- Fixed reported latency = Haze's 10 ms + Mosh oversampling delay, including when
  modules are off. At 48 kHz: **486 samples / 10.125 ms**. Host PDC should align it.
- User-written process loops allocate no memory; buffers allocated in prepare.
  Oversized host blocks processed in chunks; denormals suppressed.
- Smooth parameter changes, but discrete key/seed/filter changes and rapid
  automation can still create transitions. Not sample-accurate automation.
- Fixed 1120×700 custom UI; no installer, factory preset browser or undo yet.
- Linux VST3 + Standalone compiled; synthetic audio checks passed. Mac workflow
  prepared but not run. Needs Mac/FL host testing and listening on real music.
- Established DSP combined into a personal tool; no claim of novel/patentable
  technology, AI, or TikTok endorsement.

## Starting points
Rage lead: Ghost −12 / Mix 20%, Mosh 30%, Stutter 4 bars / Mix 50%.
Pluggnb lead: Haze in your song's key / Minor, Depth 20%, Rate 0.7 Hz, Tape 15%.
808: Phone Amount 30%, Brightness 50%, other modules off. Compare at matched levels.

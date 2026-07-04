# ISHTAR — Development History

Archive of completed work, moved out of [`TODO.md`](TODO.md) to keep the task list forward-looking.
Newest first. Blow-by-blow detail (round-by-round test logs, superseded approaches) lives in git
history; this is the durable record of *what shipped and why*.

---

## v0.6.4 — Efficiency audit: housekeeping pass (4 Jul 2026)
Engine declared feature-complete; a codebase audit for per-sample waste and orphaned code (branch
`audit-trim`). **No audible change intended** — every edit is either dead-code removal or the same
arithmetic done less often (bit-identical where practical). Deliberately NOT done: skipping unused
drawbars or idle effects — processor weight must stay patch-independent (fixed budget: you can always
use more drawbars/effects without the CPU cost moving).
- **Effects: hoisted block-rate constants out of the per-sample path.** Detune's equal-power
  crossfade gains (cos/sin of mix — two transcendentals per sample, always on) now computed in
  `updateParameters`; Warmth's saturation constants (bias DC-correction tanh, normalisation tanh,
  makeup-gain dB conversion) likewise — only the tanh on the signal itself remains per-sample.
- **Amp envelope: cached curve constants.** k, e^-k and (1-e^-k) depend only on `exponentialControl`;
  now cached in the voice (refreshed on change) so the per-sample envelope pays one `std::exp`, not two.
- **LUT lookups: cheaper indexing.** `SineLUT`/`SoftSawLUT` (the hottest functions — 11–21 calls per
  voice-sample) now use power-of-two masking and int-cast flooring instead of two `%` ops + `std::floor`.
- **Orphaned code deleted** (all verified zero callers): the `FastMath` Padé-exp class (superseded by
  the normalised `std::exp` envelope forms), `PLANETVoice::lfoPhases` + `storedAmpEnvValue`,
  `ModulationState::lfoPhaseDelta` + `::reset()`, `CoefficientParams::spectralMultiplier` +
  `updateSpectralMultiplier()`, `CoefficientGrid::setStagedCoefficient()`/`getActiveCoefficient()`,
  `PLANETVoiceManager::findVoiceForNote()`/`getFirstActiveVoice()`.

Two ear-driven fixes followed on the same branch once Gerard stress-tested the effects:
- **Spread Mix de-zipper.** The equal-power crossfade gains always stepped at block rate (pre-existing,
  not a regression — the old per-sample cos/sin recomputed the same block-constant mix); gains now glide
  to their block-rate targets via a ~10 ms one-pole. At rest they settle to exactly the old values.
- **Warmth saturation rework** (was an early "harmonic hole" attempt, superseded musically by Direct
  routing + Density but still useful). Old design: hard −3 dB makeup step at 51% (the audible
  "switch-on"), full-insert tanh at drive up to 4, no post-filtering — buzzy, high-harmonic-forward.
  New design, all continuous in t = upper-half amount: **parallel** saturation with t² wet-mix ease-in
  (exactly 0 at 50% → below-half patches bit-identical), drive capped at 3, continuous −8·t² dB makeup,
  and a post-saturation **high-shelf cut (4.5 kHz, 0→−4 dB with t)** — the tape darkening that was
  missing. Voicing constants (`kMaxExtraDrive`, `kBias`, `kMakeupDB`, `kToneCutDB`, `kToneFreqHz`) are
  named in `WarmthProcessor` for ear-tuning. The duplicated per-processor `BiquadFilter` structs were
  unified (Warmth needed the high shelf only Punch's copy had).

---

## v0.6.1 — Hammond single-trigger percussion (F10) (3 Jul 2026)
First feature on `main` after the v0.6.0 forks merged. A per-drawbar **envelope trigger mode**: **Multi**
(default — retrigger the drawbar's envelope on every note, the old behaviour) or **Single** (fire only on
the first note of a phrase, re-arming once all keys lift). Single + a fast-decay envelope on a Direct
drawbar = authentic B3 single-trigger percussion: play detached to get the ping on each note; legato lines
percuss only the first note; a chord struck together shares one ping. Param `k{n}TrigSingle` (default 0 =
Multi) → existing patches unchanged.
- **Phrase gating in `PLANETVoiceManager`.** Re-arm is driven by **physical keys held**, tracked directly
  (128-key table + count, set on note-on / cleared on note-off), NOT `getActiveVoiceCount()` — a voice
  lingers through its amp-release tail, so voice-count would delay re-arm until the release finished and
  kill perc on detached playing. The first key of a phrase (no keys down) reloads a ~30 ms grace window,
  drained once per block in `advanceBlock()` so the window is measured in real elapsed audio samples
  without touching the per-sample path. Any note-on while the window has samples fires the Single drawbars
  (chord coherence). **Sustain pedal deliberately does NOT hold the phrase open** (the pedal holds the
  sound, not the key) — pedal-sustained + a new key re-fires the perc, a musically useful distinction
  between holding-by-keys (no retrigger) and holding-by-pedal (retrigger).
- **Voice.** `startNote` takes a per-drawbar fire mask: a firing drawbar resets its envelope to Attack; a
  non-firing Single drawbar is forced Idle/silent so a reused voice can't leak a stale envelope. Only the
  envelope is gated — the drawbar's base level is untouched.
- **GUI.** A third switch circle **"Perc"** above the Shape/Direct routing pair, shown and clickable ONLY
  when the drawbar's envelope is active (the same `|EnvelopeAmount|>0` test that reddens the fader thumb);
  on = Single, off = Multi. The routing stack is bottom-anchored (Direct legend at the fader bottom) so the
  Perc switch has room to appear at the top of the strip without hitting the F-number box; visibility
  tracks the thumb live (repaint on the active-state edge in `updateDrawbarColors`).
- Test patch: `Patches/Hammond Experiments/Hammond - Jazz Perc (Single Trigger).md`. Ear-approved
  ("that's a lock … functionality spot on").

---

## v0.6.0 — "Additive + Density" (2 Jul 2026)
The palette-expanding phase (Gerard: "10x'd the sound palette"). Built on forks off `main`
(`f8-additive-routing`, then `f5-carrier-morph` on top).

### F7 + F8 — per-drawbar Direct / additive routing (F7 mute unified into F8)
Two per-drawbar switches instead of a separate mute: **"Shape"** (→ the phase-distortion / PM path,
`k·sin(f·x)` added to the carrier phase) and **"Direct"** (→ summed straight to the output as an
additive partial *on* harmonic f). Both off = muted (subsumes the old F7 mute); both on = feeds both.
Params `k{n}ToPM` (default 1) / `k{n}ToOut` (default 0) → existing patches bit-identical. Each bar's
`k·sin` term is computed once and gated to each destination; additive amp = `clamp(k_eff, ±2)/2 ·
brilliance`, snapshotted at the carrier-cycle boundary so toggling is click-free (every
`sin(modPhases[i])` is 0 there). GUI = console channel-strip: two routing circles right of each fader
("Shape"/"Direct" legends, filled in the drawbar colour), muted column gets a faint wash. Directly fills
the mid harmonics (immune to the J₀ tax / Bessel cancellation — see memory `hole-in-middle-analysis`)
and turns ISHTAR into a live tonewheel — the basis of ENLIL. Ear-approved "the sound is fantastic".
Optional polish still open: fade an additive partial as `f·f₀` approaches Nyquist.

### F5 — Density carrier morph (sine → soft-saw)
The "Brilliance" zone became **"Colour"** (Brilliance + Density sliders). Offline analysis first (memory
`density-carrier-analysis`; tool at `N:\PLUGIN DEVELOPMENT\Tools\ISHTAR-analysis\`) chose a soft-saw LUT
**r=0.8, N=16**, built as a pure sine series (`SoftSawLUT` in `PLANETVoice.h`) so it is 0 at θ=0 →
click-free preserved. Carrier lookup is now `(1-m)·sine + m·soft-saw`, morph cached at the cycle boundary;
morph=0 is a bit-identical pure-sine fast path (old patches unaffected). **Carrier only** — additive/Direct
partials stay pure sines. High Density + a few drawbars = ISHTAR bright saw leads; low Density = ENLIL
tonewheel warmth (imperfect sine); partial morph (~0.5) = widest envelope-driven timbral range. Key
offline finding: a sine carrier can't reach a sharp saw (plateaus ~0.80 even with 6 modulators); the
soft-saw does it with 3.
- **Colour mod-wheel routing.** Each slider has a two-zone "MW" button — left half toggles Off↔On, right
  half (big ▲/▼ triangle) flips Normal↔Inverse. Inverse flips the wheel direction with the soft-takeover
  centre preserved. Switching Off while the value is off the thumb **latches** the effective value (held +
  heard, not snapped back); the diff indicators read the processor's *published* effective value so they
  always match the sound. All saved in patch (`brillianceModWheel` default Normal, `carrierMorphModWheel`
  default Off). *(The "link Brilliance↔Density" button from the concept is not built — a possible follow-up.)*

### ENLIL groundwork
F8's additive Direct mode = a tonewheel. Hammond footages land on the 0.5 F grid (16'→0.5, 5⅓'→1.5,
8'→1, 4'→2, 2⅔'→3, 2'→4, 1⅗'→5, 1⅓'→6, 1'→8); DB1–9 in that order mirror a real console left-to-right.
Convincing by ear, incl. through the UAD Waterfall Leslie sim — may make the mooted T202-sampling project
redundant. Starter patches in `Patches/Hammond Experiments/` (Jimmy Smith, Full Organ, Gospel Bright,
Mellow Flutes, Jazz Perc). Carrier-is-the-8' finding recorded (memory `f7-f8-unified-routing`). ENLIL
itself — percussion (F10), Leslie (F11), identity decision — remains open in TODO.

### Tooling
`ISHTAR-analysis` — offline Python engine model + carrier/fit analysis — promoted to
`N:\PLUGIN DEVELOPMENT\Tools\ISHTAR-analysis\` (reusable for F3/F3b/F5).

---

## v0.5.x — polish + features (Jun – 1 Jul 2026)

**GUI batch #6 (29 Jun).** Palette-03 drawbar hues; Ishtar-star redesign (independent inner-circle /
ray-tip ratios `INNER_CIRCLE_RATIO 0.4` / `RAY_TIP_RATIO 0.78`, curved ray joins to kill the miter spike
that caused the star-vs-orbit overlap, diagonal rays shortened `RAY_TIP_RATIO_DIAG 0.58` to de-cog); zone
labels moved to top-left; dynamic coloured "DRAWBAR N ENVELOPE" header; comet-tail value arcs replacing
the orbit rings (bipolar-aware, `drawCometTail`); selected-drawbar column outline; global-vs-per-drawbar
steel-grey (`#8a93a3`) colour scheme. Kept the functional drawbar thumb shapes (rejected the mockup's
rectangles); kept the ISHTAR wordmark on the bottom bar. Fixed a graph/value-box drift (paint() and
resized() now share `harmonicEnvBounds`/`ampEnvBounds`).

**ISHTAR rename (28 Jun).** Forked into a new plugin identity: PLANET2026 / code `PL26` **frozen** (tag
`pl26-frozen`, installed `.vst3` left intact so old projects keep loading it); going forward ISHTAR /
code `ISTA` builds a separate `ISHTAR.vst3`. Both coexist in `C:\Program Files\Common Files\VST3\`; patch
library shared. Source files keep the internal `PLANET` prefix. `bundleIdentifier` →
`com.gerardjohnson.ISHTAR`.

**Keyboard-focus fix (27 Jun).** DAW transport keys were swallowed by the editor. Root cause (JUCE 8.0.9):
`ComboBox` re-opens on Return, and after the popup closes `getAccessibilityHandler()->grabFocus()`
bypasses `setWantsKeyboardFocus(false)`. Fix: `FocuslessComboBox` (gives away focus in `focusGained` with
a re-entrancy guard; returns false from `keyPressed`/`keyStateChanged`) for the LFO combos, plus
focus-release on editable labels. All Cubase transport tests pass (spacebar, locators, keypad Enter, Load/Save).

**Pitch envelope (27 Jun).** Normalised finite-duration exponential `level = (1-e^(-k·t01))/(1-e^(-k))`,
k=5 — keeps the musical exponential sweep but lands exactly in tune at `pitchAttackTime` (no residual
detune on held notes). Time knob `NormalisableRange` skewed 0.35 for resolution at the fast end.

**Sample-accurate MIDI timing (1 Jul).** Split-block `processBlock`: walk MIDI events in sample order,
render segments up to each event's `samplePosition`, apply, render the tail. Fixes notes landing *ahead*
of the beat on quantised export; no added live latency (sample-0 events still render from 0); pitch wheel
now sub-block accurate. Gerard: "beautifully precise… a joy to behold."

**F1 — copy env / mod params between drawbars (1 Jul).** Drag a control's *background* onto a target
drawbar: the envelope-graph background copies ADSR + `EnvelopeAmount`; the LFO/velocity zone background
copies `LFOShape/Rate/Amount/Sync/SyncDiv` + `VelToHarmonic`. Copy cursor + target-column outline in the
source colour; auto-focus the target after the copy.

**F1b — exponential taper on drawbar / env-depth / LFO-amount controls (1 Jul).** Re-skewed for fine
control near 0, same min/max (patches byte-identical). Symmetric skew `kBipolarLowSkew 0.5` for
`k1..k10` and `k{n}LFOAmount`; custom power-law `makeBipolarLowRange()` (`kEnvLowExponent 2.0`) for the
asymmetric `k{n}EnvelopeAmount`.

**LFO visual feedback (1 Jul).** Per-drawbar "ping" ring indicators pulsing at the effective LFO rate
(white = free, amber = tempo-synced), plus a dot in the star's centre for the selected drawbar's rate.
GUI-side phase accumulators (`updateLfoPulses()`), animate with no note playing; solid-on when there's no
tempo. Tuning constants in `PLANETMainGui.h` (`LFO_PULSE_FLOOR` etc.).

**StereoSplitter moved out (1 Jul).** A general drag-and-drop stereo-WAV → mono utility, nothing to do
with ISHTAR → `N:\PLUGIN DEVELOPMENT\Tools\StereoSplitter\`. Was git-tracked but never compiled into the
plugin.

**Version label → v0.5.1 (27 Jun)** after LIFE locked/shipped (dropped the `-life` dev suffix).

**Envelope release-tail fix (9–12 Jun).** Decay/Release/Attack curves used a Padé `fastExp` that never
reached 0 (and rose again for k > ~4.4) → the tail snapped to silence and the attack snapped up.
Replaced with a normalised `std::exp` hitting exactly 1↔0 across the stage. Fixes the amp envelope + all
10 per-drawbar envelopes.

---

## Earlier
- **LIFE** — the per-partial doublet "beating string" engine — shipped before this file existed. See
  memory `life-feature-refinements` and git history.
- Pre-v0.4 history is in the dated `*.md` changelogs in the repo root (`VERSION_0.4.0_MERGE_COMPLETE.md`,
  `BUG_FIXES_v0.4.1.md`, etc.), from January 2026.

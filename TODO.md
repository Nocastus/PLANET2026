# ISHTAR — To-Do / Working Notes
**Living task list.** Last updated: 3 July 2026. Forward-looking only — **completed work is archived in
[DevHistory.md](DevHistory.md)**.

---

## Shipped so far
- **v0.6.1 "ENLIL percussion"** (3 Jul 2026): F10 Hammond single-trigger percussion — per-drawbar
  Single/Multi envelope trigger mode + "Perc" GUI switch. *Merged to main.*
- **v0.6.0 "Additive + Density"** (2 Jul 2026): per-drawbar Direct/additive routing (F7+F8), Density
  carrier morph + Colour zone + mod-wheel routing (F5), ENLIL groundwork, offline analysis tool.
  *Merged to main (3 Jul 2026): `f8-additive-routing` then `f5-carrier-morph`.*
- **v0.5.x** (Jun–Jul 2026): GUI batch #6, ISHTAR rename, keyboard-focus fix, pitch envelope,
  sample-accurate MIDI timing, F1 copy-params, F1b taper, LFO visual feedback, envelope-tail fix.
- **LIFE** — per-partial doublet engine (earlier release).

Detail for all of the above: [DevHistory.md](DevHistory.md).

---

## ▶ Next session
*(Done 3 Jul 2026: forks merged to main — `f8-additive-routing` then `f5-carrier-morph`; F10 Hammond
single-trigger percussion built, ear-approved, shipped as v0.6.1.)*

1. Pick a thread:
   - **ENLIL momentum:** **F11 Leslie sim** (T202 rotary — the big research-first task) or **F13
     Performance page** (fast MIDI patch switching — new, see Standalone features). With perc (F10) done,
     Leslie is the remaining ENLIL engine piece; the identity decision (preset family vs own mode) can be
     taken once F11 exists.
   - **Quick wins:** Density / ENLIL showcase presets (the "ISHTAR Sharp Saw" now falls straight out of
     the offline fit); **F3** classic-waveform preset fitter, now the `ISHTAR-analysis` tool exists.
   - **Arc A payoff:** **F4** filter sweeps via drawbar envelopes — Density made this more reachable
     (partial morph = wide envelope range). The capstone of the exploration arc.
   - **Polish:** **F12 output-saturation warning** (see Standalone features) — small, good pre-release.
   - **Bigger sink:** **F11 T202 Leslie sim** — the hardest remaining task (a psychoacoustics research
     project in its own right; see F11). Save for a session you can devote to it.
3. **Design-pass-first** (not code-ready): F2a portamento, F6 voice stacking, F2b alternate tunings.

## Towards a first public release (Gerard, 2 Jul 2026)
Versions stay **sequential** (no number-jump). Gerard's read: **90%+ of the feature list is done — we're
almost onto polishing.** Feature-wise it's nearly there; the remaining gap is packaging + polish + testing:
- **Merge the forks** to main.
- **Docs:** a **manual**, a **quickstart**, and a **credits page with open-source acknowledgements** (JUCE etc.).
- **Patch library:** some from us, but Gerard wants to get a feature-complete build to **beta testers**
  so they can contribute patches too.
- **Mac version:** test on Gerard's **M2 MacBook Air** — first make sure the Mac toolchain (Xcode/clang)
  is current, then build/verify the Mac plugin there.
- **Cross-host testing:** already on **Cubase and Cantabile**; widen to Reaper/Live/FL etc.
- **Standalone / ASIO:** research the **Steinberg ASIO SDK licence** — what can be shipped in the
  standalone build (the *plugin* just uses the host's ASIO; the *standalone* needs its own audio backend).
- **Branding / installer.**

---

## Feature backlog

Two threads: an **exploration arc** (can ISHTAR be pushed to sound like a classic subtractive synth?) and
a set of **standalone features**. `F#` numbers are stable IDs, grouped by theme not numeric order.
Completed items (F1, F1b, F5, F7, F8, F10) are in [DevHistory.md](DevHistory.md).

**Engine-modification items go on forks (Gerard, 2 Jul 2026):** F11 (Leslie sim) is an engine change —
gets its own branch/worktree, merged back only after ear-approval. (F5/F7/F8 shipped this way in v0.6.0.)
*Exception (3 Jul 2026): F10 was built directly on `main` — well-defined and low-risk enough that Gerard
judged a fork unnecessary; it earned its keep after an ear test. Fork when the risk warrants it, not by rote.*

---

## Exploration Arc A — "Can ISHTAR emulate a classic subtractive synth?"
**Framing (Gerard, 29 Jun 2026).** This is an *experiment, not a commitment*. ISHTAR is a harmonic
phase-distortion engine; a subtractive synth is source-wave-plus-filter. The question is what happens
when we push phase distortion to do something **outside its natural compass** — and we'll only know
whether that's worthwhile by building enough to *hear* it. The arc has three steps that build on each
other: **get the source wave (F3 → F5), then animate it like a filter (F4).** Treat each step's output
as evidence for whether to take the next one. **F5 (Density carrier morph) is DONE** — see DevHistory;
it answered the "source wave" question (a soft-saw carrier reaches a sharp saw with a few drawbars).

### Arc step 1 — F3. Classic-waveform presets via phase-distortion fitting (saw / square / pulse)
The engine is `output(x) = sin(x + Σᵢ Kᵢ·sin(fᵢ·x))` — i.e. each drawbar is a PM operator
(ratio `fᵢ` = F field, depth `Kᵢ` = drawbar fader). Goal: compute `(fᵢ, Kᵢ)` sets that
reproduce sawtooth, square, and pulse, and ship them as `.md` presets. **The offline fitter now exists**
(`N:\PLUGIN DEVELOPMENT\Tools\ISHTAR-analysis\`), so this is much closer to a turn-the-crank job.
- **Theory:** Jacobi–Anger gives `sin(x + K·sin(f·x)) = Σₙ Jₙ(K)·sin((1+nf)x)`. So `f=1` →
  all harmonics (saw/brass family), `f=2` → odd harmonics only (square/triangle family);
  integer multipliers only (non-integer = inharmonic). Bessel ripple + cross-modulation mean the
  exact `1/n` amplitudes need numerical fitting, not a closed form.
- **Method (offline):** model the engine in Python → FFT one cycle to read its harmonics →
  `scipy.optimize.least_squares` to solve the 10 `Kᵢ` against the target harmonic series
  (saw `1/n`; square `1/n` odd; pulse(d) `(2/nπ)sin(nπd)`) → emit `(fᵢ, Kᵢ)` straight into a
  patch. Maps 1:1 onto the F fields + drawbar faders.
- **Notes:** brilliance scales all Kᵢ, so one fitted preset gives a free sine→waveform morph on
  the brilliance knob. Expect convincing "saw-like/square-like" tones, not textbook band-limited
  classics (usually more organic). Watch aliasing up high — fit ~10–16 harmonics. Gerard already
  has a convincing square by hand, so the manual approach works; the fitter automates/refines it.
- **Deliverable:** ready-to-load saw / square / pulse(50%) / pulse(25%) `.md` presets, plus printed
  F-and-K tables for hand-dialling. Consider a Density=1 "Sharp Saw" preset from the F5 analysis.

### Arc follow-on — F3b. Smoothness-weighted fitting (new standard fitter objective)
**Finding (1–2 Jul 2026 "hole in the middle" analysis; full math in memory `hole-in-middle-analysis`,
scripts in that session's scratchpad).** Gerard's observation — strong fundamental + high-harmonic
cluster + weak/holey mids — is real, but it's a *findability* problem, not an engine limit:
- Band-only or amplitude-only fitting (what F3/v7 did) lands in lumpy solutions with 10–28 dB
  cancellation dips in individual mid harmonics (this is the v7 saw's "lumpiness").
- Adding a **local-dip penalty over h2–h20** to the objective finds bright + full + *smooth* patches
  within all existing limits: max dip 3.7 dB with 25% of energy in h9–24, on plain integer F=1..10,
  every |K| ≤ 1.4. The smooth solutions exist; nothing steers hand-voicing or naive fitters to them.

**Actions:**
1. Add the dip penalty (max local dip in dB over h2–h20, computed on integer harmonics) to the
   standard fitter objective alongside band energies / target amplitudes. Cheap, offline-only.
2. **Ear test pending:** the discovered patch is banked as
   `H:\OneDrive\Documents\PLANET2026\Patches\Hole Experiments\ISHTAR_BrightFull_SmoothMid.md` —
   load it and judge whether the mid fullness is audible vs the hand-voiced bright patches.
3. If the ear test passes, consider re-fitting the v7 saw with the dip penalty (and now, optionally,
   over the soft-saw carrier from F5).

**Hand-voicing rules of thumb from the same analysis (the J₀ tax):** every drawbar scales every
*other* drawbar's harmonics by J₀(K) — K=0.5 → −0.6 dB, K=1 → −2.3 dB, K=2 → **−13 dB**. So:
(a) brightness via **several bars at |K| ≲ 1**, never one bar slammed to 2; (b) after adding a
top-end bar, re-raise the mid Ks to repay the tax; (c) half-integer bars at modest K (~0.7) are a
*feature*, not leakage — f=0.5 = 16′ suboctave depth, f=1.5 = 5⅓′ quint density (doubles the line
count in the low mid for only −1.1 dB tax). They add subharmonic-series thickness, not integer-
harmonic fill — don't use them when fitting a specific classic waveform.

### Arc step 3 — F4. Simulate filter sweeps with the drawbar envelopes
Idea (Gerard): each drawbar has its own envelope, so per-drawbar envelope depths/times across the
harmonic series can emulate a filter's time-varying gain at each harmonic — e.g. a lowpass sweep =
upper drawbars attack later / decay faster than lower ones. With 10 drawbars you approximate the
filter's harmonic-gain envelope in 10 bands. Natural pairing with F3/F5: start from a fitted/alternate
saw or square, then shape per-drawbar envelopes to match a sweeping filter's per-harmonic gain over time.
Worth exploring how close 10 bands can get (resonance peak = boost the band at the cutoff). **The payoff
step — this is where "source wave + moving filter" actually becomes subtractive-synth-like; do it once a
usable saw/square exists.** F5's partial-Density finding helps: a softer carrier needs more drawbars to
sharpen, giving the envelopes a wider brightness range to sweep across.

---

## ENLIL — a Hammond-organ voice folded into ISHTAR
**Name (Gerard, 2 Jul 2026): ENLIL** — the Hammond-organ identity within ISHTAR (a sibling name to
ISHTAR itself), after Gerard's own instrument, a 1969 Hammond **T202** spinet. Groundwork (additive
Direct mode, footage mapping, starter patches, the carrier-is-the-8' finding) shipped in v0.6.0 — see
DevHistory. What remains:

**Open question:** is ENLIL just a preset family riding the additive engine, or does it get its own
mode/skin (Hammond-styled faceplate, footage labels on the drawbars, dedicated perc + Leslie controls)?
Decide once F10 (perc) and F11 (Leslie) exist and it's clear how Hammond-specific they need to be.

**Carrier-level idea (ties to F5):** in Direct-only patches the carrier is always a full 8' fundamental.
A "carrier level / carrier off" control would let pure-additive patches set the fundamental independently
— a natural companion to the F5 sine→soft-saw morph. Consider the two together.

### F10. Hammond percussion (single-trigger) — ✅ DONE (v0.6.1, 3 Jul 2026)
Shipped: per-drawbar **Single / Multi** envelope trigger toggle, re-arming on physical key-lift (not voice
release), ~30 ms chord grace window, "Perc" GUI switch that appears with the drawbar's envelope. Full write-up
in [DevHistory.md](DevHistory.md). *Optional authenticity extras not built (and maybe never needed): dedicated
B3 harmonic-2nd/3rd, decay fast/slow, soft/normal volume, the real-B3 1'-cancel + level drop when perc is on.*

**B3 controls (optional, deferred):** harmonic 2nd (4', f=2) / 3rd (2⅔', f=3); decay fast/slow; volume
soft/normal; real B3 cancels the 1' and drops level a touch when perc is on. Revisit only if ENLIL gets its
own Hammond-styled mode.

### F11. Leslie sim — T202 rotating-reflector type (NOT a 122 cabinet), integrated not standalone
**Idea (Gerard, 2 Jul 2026).** If ENLIL is real, ISHTAR wants its own Leslie — but modelled on the
**T202's built-in rotary**, which is unusual: a single **backward-facing loudspeaker with a rotating
polystyrene reflector**, NOT the bass-drum + twin-horn system of a 122/147 cabinet. Heard from the
player's seat it gives a **rotary sweep in a VERTICAL orientation, surrounding the player** — a very
different spatial image from the horizontal horn/drum throw of a 122.
- **Character to capture:** one rotating source (no separate bass-rotor / treble-horn split, no crossover),
  so a more uniform Doppler + amplitude sweep, plus that enveloping vertical motion. Likely amplitude
  tremolo + pitch/Doppler wobble + a moving reflection/comb, single rotor, with the vertical/surround
  image rendered into the stereo field. Speed switch with realistic **ramp up / down (chorale ↔ tremolo,
  and brake)**.
- **The hard part — vertical / height perception (Gerard, 2 Jul 2026).** The T202 rotary moves in a
  **vertical** plane and *surrounds the player*, not just side-to-side. Rendering elevation is a genuine
  psychoacoustics problem. **Start with a pure research session** combing the literature — binaural
  rendering, **pinna (HRTF) reflection modelling** for height cues, possibly **Zucarelli holophonics** —
  *before* attempting any code, then puzzle out how to implement it. Conceptually the most difficult
  remaining task, and not originally in scope, but Gerard: "difficult, but worth it."
- **UI concept (Gerard):** an Ishtar star with an **orbiting comet** circling at the Leslie speed,
  visualising the way the sound swirls around the player's head.
- **Integrated, not standalone** (Gerard) — a built-in ISHTAR/ENLIL effect in the Effects zone alongside
  detune / warmth / punch, not a separate Leslie plugin.
- **Relationship to LIFE:** LIFE already supplies some aperiodic, Leslie-*like* motion (irregular doublet
  beating) — a genuine partial substitute, noted by Gerard as "the motion the Leslie gives without the
  regularity." F11 is the true spatial rotary; LIFE stays the no-two-notes-alike shimmer. They stack.
- **Research-first, then engine/DSP change → fork.**

---

## Standalone features (not part of the arc)

### F2. Oscillator pitch — portamento + alternate tunings
**Gerard (29 Jun 2026): portamento is "a nice thing to have" once we define the logic.** Two related
pitch features. Current pitch is hardcoded equal temperament at
[`Source/PLANETVoice.cpp:44`](Source/PLANETVoice.cpp#L44): `440 * 2^((note-69)/12)`.

**F2a. Portamento (glide).** ✅ *Designed & built 3 Jul 2026 (compiles clean; not yet ear-tested).*
Chosen: **per-voice history** (each voice glides from its own last pitch — the deliberately
less-predictable counterpart to the pitch envelope's parallel slide), **always** glide, **Rate/Time**
a user toggle. Glide reuses the pitch-envelope machinery (signed per-voice offset decaying to 0). The
glide origin is passed into `PLANETVoice::startNote` and computed in `PLANETVoiceManager::startNote`
(`glideFromHz = voice->getCurrentGlidePitchHz()`) — **that one line is the pivot to a legato/nearest-held
model** if the per-voice feel proves too chaotic. UI: Porta knob in the pitch zone (Distance/Time/Porta)
+ Rate/Time toggle above the Colour-zone MW buttons. Next: ear-test (chord bloom, big leaps), tune
curve/range. See memory `portamento-design`. Original design notes below.

- **Gerard's first thought (2 Jul 2026):** start with **"each voice slides to the next use of the same
  voice"** — poly glide where each voice glides from *its own* previous pitch. Note the **Pitch Envelope
  already gives the "all voices slide in parallel" flavour**, so portamento should target the per-voice
  history behaviour the pitch envelope can't do.
- Other questions to settle: glide source (last-played vs nearest held note); constant-*time* vs
  constant-*rate* glide; always-glide vs legato-only; retrigger behaviour and what each voice glides
  *from* on note-on. Likely a per-voice current→target pitch with a glide ramp in `PLANETVoice`, plus a
  glide-time (and maybe glide-mode) parameter. **Book a design pass with Gerard first.**

**F2b. Alternate tunings — private fork, not for public release (Gerard, 2 Jul 2026).** For a private
project of Gerard's own, so it'll be a **fork that never merges to main.** Replace the hardcoded
equal-temperament formula at `PLANETVoice.cpp:44` with a tuning table lookup (fixed microtuning tables,
Scala `.scl`/`.kbm` import, or MTS / MTS-ESP). Keep the integration point isolated so the fork is easy to
maintain.

### F6. Voice stacking / unison detune
**Idea (Gerard, 29 Jun 2026) — genuinely uncertain: "might be great or might not suit the sound — we'll
just have to experiment and see."** Currently 16 voices = one voice per note = 16-note polyphony
(`PLANETVoiceManager`). Add a **stack** option that assigns multiple detuned voices per note, trading
polyphony for thickness. **Gerard (2 Jul 2026): hopefully simple** — trigger **2 / 3 / 4 voices**
simultaneously with a **programmable detune that averages to zero** (symmetric spread, no net pitch shift).
- **Modes:** 1× (current, 16-note poly), **2× stacked** (8-note poly), **4× stacked** (4-note poly).
- **Detune:** spread the stacked voices around the note pitch (a detune-spread parameter, cents), summing
  to zero net offset. Pitch source is `PLANETVoice.cpp:44` — apply per-stacked-voice offsets at trigger.
- **Implementation sketch:** on note-on, allocate `N` voices for the note instead of 1, each with its
  detune offset; cap simultaneous notes at `16 / N`. Voice-stealing / allocation in `PLANETVoiceManager`
  need to understand the grouping. Consider whether stack voices also get a stereo spread (pan) for width.
- **Open questions:** detune law (linear cents vs musical spread); odd-count centre voice at pitch vs all
  detuned; per-stack stereo spread yes/no; stack count as a patch parameter (likely yes).

### F12. Output-saturation warning light (Gerard, 2 Jul 2026)
We deliberately have **no dynamic or fixed output scaling** — just the manual output-level control (the
additive engine can run hot, by design; see the F8 notes in DevHistory). To catch clipping *without*
adding gain-riding, add a **green / amber / red saturation warning**. Simplest: the **master-volume slider
thumb** (currently a blue dot) changes colour with the output level — green (safe) → amber (approaching) →
red (clipping) — with a **sensible time constant / peak-hold** so even very short red flashes register.
Needs the processor to publish a peak level atomic to the GUI (the same pattern as the mod-wheel effective
values) and the thumb draw to read it. Small, self-contained, good pre-release polish. *(Not referenced
anywhere else in this doc.)*

### F13. Performance page — fast patch switching via MIDI Program Change
**Idea (Gerard, 3 Jul 2026).** A live-performance layer. Assign some number of patches (**~16?**) to
**performance slots**, held **in memory (not re-read from disc)** so switching between them is as fast as
possible, and select between them with **MIDI Program Change** messages. The **Performance Page** takes over
the **Drawbar Envelope / LFO zone** when invoked — the **drawbars and global params stay visible**, so the
live sound is still shown (and tweakable); only the envelope/LFO editor is replaced by the slot grid.
- **Open questions:** slot count (16?); how slots get populated (drag patches in / capture current state /
  pick from library); where the in-memory bank is persisted (folded into plugin state? a separate
  performance file? both?); PC→slot mapping (1:1, bank offset, PC 0-based vs 1-based); click-free switching
  (hard switch vs short crossfade) and what happens to notes ringing when a PC lands mid-phrase; whether the
  slot grid also shows patch name/category per slot.
- **Why in-memory:** the point is instant switching for live use — pre-load all slot patches' parameter sets
  at assign-time so a PC is just a parameter-bank swap, never a disc read/parse.

### F9. Feedback phase term — ❌ effectively rejected (Gerard, 2 Jul 2026)
**Gerard (2 Jul 2026): "I honestly think this is a non-starter. Everything we wanted to achieve with this
is handled better by the carrier morph"** (F5 Density + F8 additive routing, added in v0.6.0). The earlier
click concern stands too: at the carrier cycle boundary the phase becomes `kFB·y[n−1]` ≠ 0, so output is
no longer exactly 0 there — every coefficient promotion becomes a potential click and the 0.5-grid F-snap
rationale weakens (plus DX7-style high-feedback instability needing a two-sample average). Left on record,
not planned. *(Original idea: `distortedPhase += kFB·previousSample` for a dense ~1/n comb — the classic
FM route to a bright saw — now covered by F5 / F8 / F3b.)*

---

## Parked / decided against (for now)
- **Skip null drawbars in `applyPhaseDistortion`** — *parked 9 Jun 2026, possibly permanently.*
  Proposed: `if (activeCoeffs[i] == 0.0f) continue;` in the per-sample 10-tap loop to skip the
  sine lookup for null drawbars. **Decided not to do it now.** It only lowers *average* cost (sparse
  patches), not the **worst case** (all 10 up at full polyphony) — and a real-time/hardware budget is set
  by the worst case. It also trades away **consistent CPU load across patches**, which we consider a
  feature. Trivial and reversible, so defer it to an actual hardware-port moment when we'd profile on the
  real target anyway.
  - *If the ceiling ever becomes the real constraint, use a worst-case lever instead:* (1) **SIMD the
    10-tap distortion loop** (lowers worst case + keeps load consistent; best fit, most work); (2) cheaper
    LUT (drop interpolation / smaller table); (3) fewer coefficients, sound permitting. The null-skip is
    the *wrong* lever for the ceiling.

## Housekeeping
- The dated `*.md` changelogs in the repo root (`VERSION_0.4.0_MERGE_COMPLETE.md`, `BUG_FIXES_v0.4.1.md`,
  etc.) are Jan-2026 history, now superseded by DevHistory.md — fold into it or delete when convenient.

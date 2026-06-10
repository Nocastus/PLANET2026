# PLANET2026 — To-Do / Working Notes
**Living task list.** Last updated: 9 June 2026

> The dated `*.md` files in this folder (`VERSION_0.4.0_MERGE_COMPLETE.md`,
> `BUG_FIXES_v0.4.1.md`, etc.) are historical changelogs from January 2026 and are
> now stale. This file is the current source of truth for outstanding work.

---

## Active tasks

### 1. Pitch envelope — restore exponential shape *without* the residual detune ✅ IMPLEMENTED (needs ear test)
**File:** [`Source/PLANETVoice.cpp`](Source/PLANETVoice.cpp) — `PITCH ATTACK ENVELOPE` block, ~lines 216–243.

**Status (9 Jun 2026):** Replaced smoothstep with the normalised finite-duration exponential
(approach 1 below), `k = 5.0`. Builds on the existing exact-arrival guard (`pitchEnvTime >=
pitchAttackTime → 1.0`). Still needs the A/B listening test before closing, and `k` may want
tuning by ear. If `k` proves worth tweaking live, consider exposing it as a parameter.

**Background.** The pitch envelope starts at an offset of `pitchEnvDistance` semitones and
should glide to the target (0 offset) over `pitchAttackTime`. The offset applied each cycle is:
```cpp
float pitchEnvOffset = (1.0f - pitchEnvValue) * pitchEnvDistance;
```
So the curve is "done" only when `pitchEnvValue` reaches exactly **1.0**.

**The problem with the two shapes tried so far:**
- **Exponential** (`1 - e^(-t/τ)`) — musically the best: fast initial sweep, slows gracefully
  into the target. BUT it's *asymptotic* — it never actually reaches 1.0, so for long/held
  notes the pitch sits **slightly out of tune** forever after the envelope action is
  perceptually over. This is the bug we need to kill.
- **Smoothstep** (`3t² − 2t³`, current code, commit 4099d76) — reaches exactly 1.0 at
  `t = pitchAttackTime`, so tuning is correct, BUT the shape is symmetric/sluggish at the
  start and feels far less musical than the exponential.

**Goal:** keep the exponential's musical curvature AND guarantee the offset lands at exactly
0 (perfect tuning) by the end of `pitchAttackTime`, with no audible snap.

**Candidate approaches (in rough order of preference):**

1. **Normalised finite-duration exponential** (recommended). Tie the curve to the actual
   `pitchAttackTime` and scale it so it hits exactly 1.0 at the end:
   ```cpp
   // k = curvature (higher = more "exponential" feel). t01 = pitchEnvTime / pitchAttackTime, clamped [0,1]
   const float k = 5.0f;                       // tune by ear
   float t01 = juce::jlimit(0.0f, 1.0f, (float)(pitchEnvTime / pitchAttackTime));
   pitchEnvLevel = (1.0f - std::exp(-k * t01)) / (1.0f - std::exp(-k));
   // at t01=0 -> 0 ; at t01=1 -> exactly 1.0
   ```
   Keeps exponential character, arrives exactly on pitch at the end. `k` becomes the
   "shape" tuning knob (could even be exposed as a parameter later).

2. **Exponential with a snap threshold.** Run the natural exponential but force
   `pitchEnvLevel = 1.0f` once it crosses a perceptually-inaudible threshold (e.g. residual
   offset < ~1 cent, i.e. level > ~0.999, or after ~5τ). Simpler, but risks a tiny audible
   step if the threshold is too loose — choose it against `pitchEnvDistance` so worst-case
   residual is < 1 cent.

3. Stay on smoothstep — rejected: not musical enough.

**Acceptance test:** on a long held note with a large `pitchEnvDistance`, the sustained pitch
must be perfectly in tune (no measurable cents offset) once the envelope settles, while the
initial sweep still feels like the old exponential. Compare A/B against commit history
(exponential was pre-4099d76).

---

### 2. ISHTAR editor window steals keyboard focus from the DAW ⭐ PRIORITY (the big one)
**Symptom (from testing):** while the PLANET/ISHTAR plugin window is focused, the host's
transport key commands (play, stop, cycle, etc.) don't reach the DAW. Breaks muscle-memory
DAW operation while the editor is open.

**Likely cause:** classic JUCE/VST3 issue — the editor window (and/or its child components)
grabs and *consumes* keystrokes, so unhandled keys never bubble back up to the host. The
behaviour is host- and format-dependent (VST3 vs AU vs standalone; Cubase/Live/Reaper/FL all
differ), so expect to test against the target DAW(s) specifically. **Target host: Cubase** (VST3;
Gerard's main DAW, picky about VST compatibility). JamStix exhibits the same issue — confirms this
is the generic JUCE/VST focus model, not specific to our code.

**Grounded findings (9 Jun 2026), from the JUCE source:** Sliders/knobs already use
`setWantsKeyboardFocus(false)` (juce_Slider.cpp:1461) — never the culprit. The focus-holders are
`Button`/`TextButton` (true), `ComboBox` (true when label non-editable), and the many **editable
`Label`** value fields (true). No focus management existed in our code.

**Status — Layer 1 DONE (9 Jun 2026), awaiting Cubase test:** added `setWantsKeyboardFocus(false)`
to the Load/Save buttons and both LFO combos in `PLANETMainGui.cpp` (mouse-operated, don't need
focus). Editable value labels deliberately left as-is (they need focus only while typing).
**Test protocol in Cubase** — try transport (space/cycle) in 3 states: (1) after clicking the
plugin background, (2) after clicking a knob/button, (3) after typing into a numeric field + Return.
- If only (3) fails → editable labels are the remaining offender. Targeted fix: on each editable
  label, `onEditorHide` → move focus back to the editor / `giveAwayKeyboardFocus()` so it doesn't
  linger. (Could centralise in the label-setup helper.)
- If (1) or (2) still fail → Layer 1 insufficient, escalate to Layer 2 (native Win32 key
  forwarding to the host window).

**Round-1 Cubase test results (9 Jun 2026):** Spacebar (start/stop) ✅ consistent. Numeric 1/2
(left/right locator) ✅ consistent, even after keypad input into ISHTAR. Load/Save ✅ no ill
effects. **Remaining:** keypad **Enter** (play) works until you (a) type into a numeric field —
then it does nothing, or (b) use an LFO dropdown — then it re-opens the dropdown. So: editable
labels and combos still hold focus / consume Enter, exactly the predicted hole.

**Round-2 fix (9 Jun 2026, awaiting test):** in `PLANETMainGui.cpp` —
(1) combos also get `setMouseClickGrabsKeyboardFocus(false)` (the `setWantsKeyboardFocus(false)`
alone didn't stop the *click* focusing them, hence Enter re-opened the popup);
(2) constructor-end loop sets `onEditorHide = [lbl]{ lbl->giveAwayKeyboardFocus(); }` on every
Label child, so finishing a value edit releases focus back to the host. Re-test keypad Enter
after typing a value and after using a dropdown. If a combo still re-opens on Enter, next step is
to subclass ComboBox and give away focus in `focusGained`. If the editable-label case persists,
Layer 2 (native key forwarding) is the fallback.

**Round-2 Cubase test results (9 Jun 2026):** Numeric-field case ✅ FIXED — keypad Enter plays
after typing a value. **Still open:** the LFO combo still opens on keypad Enter while ISHTAR has
focus (the `setMouseClickGrabsKeyboardFocus(false)` wasn't enough — focus is being restored to the
combo some other way, likely when the popup dismisses). **Next step (not yet done):** subclass
`ComboBox`, override `focusGained` to immediately `giveAwayKeyboardFocus()`, and use that subclass
for `lfoShapeCombo`/`lfoSyncCombo`. Everything else in the focus work is confirmed working in Cubase.

**Directions to investigate (cheapest first):**
1. **Audit keyboard-focus ownership.** Call `setWantsKeyboardFocus(false)` on the top-level
   editor and every child component that does NOT need typed input. Only genuine text-entry
   fields should keep focus — i.e. the patch-name editor and the per-drawbar F-value editors
   (`PLANETMainGui`). Components that don't want focus won't consume keystrokes, which often
   lets the host receive transport keys again. Start here — most likely a top-level component
   defaulting to wanting focus.
2. **Don't consume unhandled keys.** Ensure any `keyPressed`/`keyStateChanged` overrides return
   `false` for keys we don't actually handle, so they propagate instead of being swallowed.
3. **Format-specific forwarding (last resort).** If the host still doesn't get them, look at
   forwarding unhandled key events to the host window. On Windows VST3 this is host-specific and
   hacky (e.g. posting `WM_KEYDOWN` to the parent HWND); confirm whether JUCE's current VST3
   wrapper already bubbles unconsumed keys to the host before going native.

**Acceptance test:** with the editor open and focused in the target DAW, spacebar (play/stop)
and the cycle key operate the transport as normal; patch-name and F-value text fields still
accept typing when clicked.

### 3. Version string is stale
[`Source/PluginEditor.cpp:28`](Source/PluginEditor.cpp#L28) still reads `"v0.4.0"` despite the
v0.4.1 bug-fix work and everything since (master volume, transpose, random LFO, tempo-sync LFO).
Bump it to reflect reality before the next release build.

### 4. Decide the fate of `StereoSplitter.cpp`
[`Source/StereoSplitter.cpp`](Source/StereoSplitter.cpp) is an untracked standalone console
utility (drag-and-drop stereo WAV → `_L`/`_R` mono files + metadata dump). It is **not part of
the plugin** and is not referenced in `PLANET2026.jucer`. It currently sits loose in the plugin
source tree. Either move it to its own folder/project, or commit it deliberately with its own
build target — but don't let it get compiled into the plugin.

### 5. LFO visual feedback on drawbars (deferred since v0.4.1)
Infrastructure is in place — `updateDrawbarColors()` sets a `hasActiveLFO` property on each
drawbar slider — but nothing renders it. Needs a custom `LookAndFeel` that reads the property
and draws an indicator (e.g. pulsing border) on `drawLinearSliderThumb()`. See
`BUG_FIXES_v0.4.1.md` Bug #2 for the original write-up.

---

## Feature backlog

### F1. Copy envelope parameters between drawbars (drag & drop)
**From testing — quality-of-life.** Setting up 10 drawbar envelopes by hand is tedious.

**Proposed UX (Gerard's):** a small draggable box in each drawbar's envelope-control area.
Drag it onto a different drawbar → that target drawbar's envelope is instantly set to match
the source drawbar's.

**Implementation sketch (JUCE):**
- Put a `DragAndDropContainer` on `PLANETMainGui` (or a suitable parent).
- The little box is the drag source — `startDragging(sourceDrawbarIndex, this)` on drag.
- Each drawbar acts as a `DragAndDropTarget` (`isInterestedInDragSource` / `itemDropped`).
- On drop, copy the source drawbar's envelope params to the target by writing through the
  APVTS (`setValueNotifyingHost`) so changes are automatable, undoable, and refresh the GUI.

**Open question — exactly which params copy?** Gerard said "envelope parameters", so at minimum
the per-drawbar ADSR (`k{n}AttackTime/DecayTime/SustainLevel/ReleaseTime`) + `k{n}EnvelopeAmount`.
**Decide:** include the per-drawbar LFO (shape/rate/amount/sync) too? And the `k{n}` coefficient
value itself? Suggest: envelope-only by default, with LFO copied only on a modifier (e.g. hold a
key while dropping) if we want it. Confirm with Gerard before building.

### F2. Oscillator pitch — portamento + alternate tunings
Two related pitch features. Current pitch is hardcoded equal temperament at
[`Source/PLANETVoice.cpp:44`](Source/PLANETVoice.cpp#L44): `440 * 2^((note-69)/12)`.

**F2a. Portamento (glide).** ⚠️ Needs a design discussion before coding — polyphonic glide is
not one obvious thing. Questions to settle:
- Mono/legato glide (single gliding voice) vs full poly glide (every voice glides from its
  previous pitch)?
- Glide source pitch: from the last-played note, or nearest held note?
- Constant-*time* glide (always takes N ms) vs constant-*rate* glide (fixed semitones/sec)?
- Always-glide vs legato-only (glide only when notes overlap)?
- Retrigger behaviour and what each voice glides *from* on note-on.
- Likely needs a per-voice current→target pitch with a glide ramp in `PLANETVoice`, plus a
  glide-time (and maybe glide-mode) parameter. **Book a design pass with Gerard first.**

**F2b. Alternate tunings.** For a specific project — **may live on a fork/branch** rather than
mainline. Replace the hardcoded equal-temperament formula at `PLANETVoice.cpp:44` with a tuning
table lookup. Options to evaluate: fixed microtuning tables, Scala `.scl`/`.kbm` import, or
MTS / MTS-ESP support. Keep the integration point isolated so a fork is easy to maintain.

### F3. Classic-waveform presets via phase-distortion fitting (saw / square / pulse)
The engine is `output(x) = sin(x + Σᵢ Kᵢ·sin(fᵢ·x))` — i.e. each drawbar is a PM operator
(ratio `fᵢ` = F field, depth `Kᵢ` = drawbar fader). Goal: compute `(fᵢ, Kᵢ)` sets that
reproduce sawtooth, square, and pulse, and ship them as `.md` presets.
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
- **Deliverable:** Python fitter + ready-to-load saw / square / pulse(50%) / pulse(25%) `.md`
  presets, plus printed F-and-K tables for hand-dialling. **Queued for next session.**

### F4. Simulate filter sweeps with the drawbar envelopes
Idea (Gerard): each drawbar has its own envelope, so per-drawbar envelope depths/times across the
harmonic series can emulate a filter's time-varying gain at each harmonic — e.g. a lowpass sweep =
upper drawbars attack later / decay faster than lower ones. With 10 drawbars you approximate the
filter's harmonic-gain envelope in 10 bands. Natural pairing with F3: start from a fitted saw/square,
then shape per-drawbar envelopes to match a sweeping filter's per-harmonic gain over time. Worth
exploring how close 10 bands can get (resonance peak = boost the band at the cutoff). **Next session.**

---

## Recently fixed (awaiting ear test)
- **Amp/coefficient envelope release tail snapped to silence** — ✅ fixed 9 Jun 2026 in
  [`Source/PLANETVoice.cpp`](Source/PLANETVoice.cpp) `processEnvelope()`. The Decay and Release
  curves used `FastMath::fastExpDecay(k·progress)` (= `e^(-x)`), which never reached 0 and, worse,
  rose again for `k > ~4.4` due to the Padé approximation — so the tail was still at 15–30% level
  when the stage flipped to Idle and snapped to silence. Replaced with a normalised `std::exp`
  curve that hits exactly 1.0→0.0 across the stage. Shared engine, so it also fixes all 10
  per-drawbar envelopes. **Follow-up:** the Attack case has the same structure (`fastExpAttack(k)`
  ends at `1 - e^(-k)` then snaps up to 1.0) — small/masked at the transient peak so left as-is;
  normalise it too if any attack click shows up.
  - *If the two per-sample `std::exp` calls ever flag on CPU:* two viable levers.
    (a) **Incremental exp** — precompute `step = exp(-k·sampleDelta/releaseTime)` once at stage
    start, then `expTerm *= step` per sample (one multiply, no per-sample exp, full per-sample
    smoothness). (b) **Per-cycle amp envelope** — the synth is phase distortion and the output is
    *exactly 0* at every cycle boundary (`sin(0)=0`, see PLANETVoice.cpp:413/384), so updating the
    gain at the zero crossing is click-free, same as the coefficient grid already does. Caveat:
    per-cycle ties the envelope's time resolution to the note period, which quantises **fast
    attacks at low pitch** (a 3 ms attack < one 20 ms cycle at 50 Hz) and makes attack timing
    pitch-dependent — so keep Attack per-sample and run only Decay/Sustain/Release per-cycle.
    NB: neither is high-value — the real per-sample cost is the 10-tap `applyPhaseDistortion`
    loop (must stay per-sample), not the envelope.

## Parked / decided against (for now)
- **Skip null drawbars in `applyPhaseDistortion`** — *parked 9 Jun 2026, possibly permanently.*
  Proposed: `if (activeCoeffs[i] == 0.0f) continue;` in the per-sample 10-tap loop to skip the
  sine lookup for null drawbars (those with no env/LFO/seed). **Decided not to do it now.** It
  only lowers *average* cost (sparse patches), not the **worst case** (all 10 up at full
  polyphony is unchanged) — and a real-time/hardware budget is set by the worst case, so it
  doesn't help "does it fit on the target?". It also trades away **consistent CPU load across
  patches**, which we consider a feature (predictable voice budgeting). The edit is trivial and
  fully reversible, so there's no cost to deferring it to an actual hardware-port moment, when
  we'd profile on the real target anyway.
  - *If the ceiling ever becomes the real constraint, use a worst-case lever instead:*
    (1) **SIMD the 10-tap distortion loop** — lowers the worst case AND keeps load consistent
    across patches (best fit for our goals; most work, platform-specific); (2) cheaper LUT
    (drop interpolation / smaller table) — uniform saving; (3) fewer coefficients, sound
    permitting. The null-skip is the *wrong* lever for the ceiling.

## Housekeeping
- Working tree has a trivial uncommitted BOM-removal in `PLANETVoice.cpp` (line 1) — commit or discard.
- Consider folding the dated `*.md` changelogs into a single `CHANGELOG.md` once they're no longer needed for reference.

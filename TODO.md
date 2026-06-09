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
differ), so expect to test against the target DAW(s) specifically.

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

---

## Housekeeping
- Working tree has a trivial uncommitted BOM-removal in `PLANETVoice.cpp` (line 1) — commit or discard.
- Consider folding the dated `*.md` changelogs into a single `CHANGELOG.md` once they're no longer needed for reference.

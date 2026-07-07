# ISHTAR Resynth

Standalone resynthesizer for ISHTAR. Give it a short piece of audio and it
searches ISHTAR's static parameter space for the drawbar settings that come
closest to reproducing it.

Double-click `ISHTAR Resynth.bat` to launch (or `py ishtar_resynth.py`).
Run `py ishtar_resynth.py --selftest` for a headless engine verification.

## Workflow

1. **Open WAV** - the file is displayed; pitch is auto-detected in the region
   between the green (start) and red (end) markers. Drag the markers to choose
   the region; use the toolbar to zoom. "Snap markers to zero-crossings" tidies
   placement. In **Manual mode** the two markers define exactly one cycle
   (period = marker span); in **Auto mode** the detector finds the period and
   the markers just bound the analysis region.
2. **Cycles to average** - N consecutive cycles are extracted from the start
   marker, circularly aligned, and averaged into the target cycle. More cycles
   = less noise, but keep the region steady (no vibrato onset).
3. **Fit controls**:
   - *Drawbars* (1-10): the bar budget. The result panel shows the error after
     each added bar, so you can see where diminishing returns set in.
   - *Granularity* (20-200): candidate steps per continuous parameter, spaced
     evenly in normalised knob travel through the plugin's skew law (finer
     resolution near zero, like the real knobs).
   - *Harmonic fingerprint*: matches harmonic magnitudes, phase ignored -
     the "sounds the same" criterion. *Literal waveform*: matches the shape
     after free rotation/gain - the "looks the same on a scope" criterion.
     Both residuals are always reported whichever drives the fit.
   - *Allow additive routing*: lets bars route direct-to-output (F8 partials).
   - *Allow Carrier Density*: searches the sine->soft-saw carrier morph
     (the F5 Density knob, `carrierMorph`).
   - *Allow Per-drawbar Density*: searches each bar's modulator sine->soft-saw
     morph (`kNDensity`) - a parameter that exists **only on the experimental
     per-drawbar-density fork** of ISHTAR. OFF (the default): every bar's
     density is pinned to 0, the search never relies on the fork, and the
     exported patch has no `kNDensity` lines - it loads and sounds right in
     mainline ISHTAR. ON: the patch gains `kNDensity` lines and needs the
     fork build (mainline loads it but ignores those lines). Fit the same
     waveform once with it off and once with it on to compare mainline vs
     fork on equal terms.
   - *Half-integer f*: enables x.5 multipliers (sub-octave content); analyses
     a two-cycle window, auto mode only.
4. **Export patch** writes a loadable `.md` patch (default folder
   `Patches\Resynth`). Fitted levels are stored as **envelope depth** with a
   gate envelope (A 0, D 0, S 100%, R max) and k sliders at zero, so transient
   movement can be added later by simply reshaping the envelopes. Release is
   at maximum, not zero - a zero release collapses the tail to a bare sine
   while the amp envelope is still sounding. Brilliance is exported at 50%
   (performance control); the fit targets that setting.
5. **Render audition WAV** writes target loop / gap / fitted patch for A/B.

## How the search works

Static ISHTAR is memoryless: one wavecycle is
`carrier(theta + B*sum k_i mod_i(theta)) + B*sum (k_i/2) mod_i(theta)`
where `mod_i` is the bar's modulator (a sine, morphable toward soft-saw by
per-drawbar Density on the fork) and the carrier has its own Density morph.
The fitter runs beam-search matching pursuit over (f, level, route) per bar -
with per-drawbar density tried at a coarse spread when enabled, then
fine-swept during descent - with carrier Density swept per state, then
coordinate descent from several beam states, then basin hopping (random
f/route jumps, re-descended). A fine-grid waveform-mode "scout" fit always
runs alongside and seeds the main descent - the waveform objective sees
harmonic phases, which makes its landscape much sharper than the phase-blind
fingerprint.

Note: everything ISHTAR synthesizes statically is an odd-symmetric waveform
(all partials sine-phased at the cycle origin). Real-world sounds generally
are not, so **literal waveform mode has an irreducible floor** on most real
material - that is the engine's phase reachability, not a fitter failure.
The fingerprint mode is the musically meaningful one; the comparison between
the two is the interesting part.

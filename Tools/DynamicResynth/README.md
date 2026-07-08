# ISHTAR Dynamic Resynth

Time-varying resynthesizer for ISHTAR, built on top of the static Resynth
tool (which stays untouched in `Tools\Resynth` as the fallback). Instead of
fitting one wavecycle, it analyses a row of snapshots over the first part of
the note and reproduces how the harmonic fingerprint EVOLVES by shaping the
per-drawbar envelopes - the "decay toward sine" work, automated.

Double-click `ISHTAR Dynamic Resynth.bat` to launch (or
`py ishtar_dynamic_resynth.py`). Run with `--selftest` for a headless
verification. The static tool's `ishtar_resynth.py` is imported for the
engine model and fitter, so the two cannot drift apart.

## Workflow

1. **Open WAV**, place the **green marker where the note settles** after the
   attack transient (or at the very start of a slow bowed attack - the
   fitted attacks will then follow the swell). The green marker is snapshot
   1 and t = 0 for every fitted envelope. Pitch detection, manual mode and
   marker snapping work exactly as in the static tool.
2. **Snapshots** (2-16, default 8) and **Spacing** (25-200 ms, default 50)
   set the analysis timeline: 8 x 50 ms covers the first ~0.4 s; 25 ms suits
   a harpsichord pluck, 150-200 ms a slow bowed swell. Snapshot positions
   are drawn as yellow lines on the waveform once a fit starts.
   *Cycles/snapshot* averages consecutive cycles per snapshot (clamped so a
   window never exceeds the spacing).
3. **DYNAMIC FIT** runs the pipeline:
   - the full static search (F, level, route, Density) runs ONCE, on the
     snapshot with the most harmonics - decaying instruments lose harmonics
     over time, so the richest snapshot's layout covers the rest;
   - with that layout frozen, only the LEVELS are re-fitted against every
     other snapshot (fast coordinate descent, warm-started from the
     neighbouring snapshot) - this handles the PM nonlinearity exactly
     instead of assuming any drawbar-to-harmonic attribution;
   - each bar's level trajectory is then fitted with the engine's own
     envelope curve: level(t) = k + EnvelopeAmount x ADSR(t), using
     exponentialControl = 0.50 (the fit assumes that curve, and the export
     pins it);
   - the amp envelope is fitted to the snapshots' loudness (RMS) trajectory,
     so the patch follows the source's decay or swell as well as its timbre.
4. **Results**: the left plots show the reference snapshot's cycle and
   spectrum (as in the static tool); the right plot shows each bar's fitted
   level trajectory (dots = per-snapshot refits, lines = the exported
   envelopes) plus the normalised amp envelope. The table reports two
   errors per snapshot: *refit err* (the best the frozen layout can do at
   that moment) and *env err* (what the exported envelopes actually
   reproduce) - if env err is much worse than refit err, the trajectory has
   a shape one ADSR can't express (e.g. a dip-then-swell).
5. **Export patch** writes a loadable `.md` patch: per-bar k +
   EnvelopeAmount + Attack/Decay/Sustain (Release stays at 10 s - a zero
   release collapses the tail to a bare sine), the fitted Amplitude
   Envelope section, `exponentialControl = 0.50`, Brilliance at noon.
6. **Render audition WAV** writes source segment / gap / fitted patch WITH
   its envelopes running (coefficients promoted per carrier cycle, as the
   engine does), each clip peak-normalised.

## Notes and limits

- **On decaying material, untick Allow Carrier Density.** carrierMorph has
  no envelope in the engine, so whatever brightness the carrier carries is
  frozen for the whole note - a soft-saw carrier chosen for the bright
  attack leaves an unrealistic top end in the tail that no level decay can
  remove (first Wurli test: refit err climbed 6% -> 16% down the note with
  the carrier at 1.0; plateaued at ~9% with it off, and the reference
  residual even improved). Brightness that must decay has to live in the
  BARS: per-drawbar Density contributes through the bar's level, so it
  fades with the envelope. Keep the carrier morph for sustained material
  (organ-like) where a static floor timbre is right.
- Snapshot cycles are normalised, so the drawbar envelopes capture TIMBRE
  motion; overall loudness lives in the fitted amp envelope. This mirrors
  how the engine (and the manual patch-making practice) divides the work.
- carrierMorph has no envelope in the engine, so Density is static - taken
  from the reference-snapshot fit and held.
- A per-bar ADSR is monotonic after its peak: a harmonic that dips and then
  swells back can only be approximated (watch env err vs refit err).
- Very near the ictus the signal is inharmonic; keep the green marker just
  past it. The Noise drawbar remains the tool for the transient itself.
- Fingerprint mode compares magnitudes per snapshot (snapshots are not
  phase-coherent with each other), which is the musically meaningful
  criterion here; waveform mode is available and gain/shift-free per
  snapshot, same as the static tool.

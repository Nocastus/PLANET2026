"""
ISHTAR Dynamic Resynth - time-varying resynthesizer for the ISHTAR
(PLANET2026) engine, built on top of the static ISHTAR Resynth.

Where static Resynth fits ONE wavecycle, Dynamic Resynth analyses a row of
snapshots (default 8, spacing settable 25-200 ms) starting at the green
marker, and reproduces how the harmonic fingerprint evolves over that span
by shaping the per-drawbar envelopes:

  1. Extract a harmonic snapshot at each point (same cycle-averaging as the
     static tool; every snapshot is normalised, so it captures TIMBRE).
  2. Run the full static fit (F, level, route, Density) once, on the
     snapshot with the most harmonics - decaying instruments lose harmonics
     over time, so the richest snapshot's layout covers the others too.
  3. Freeze that layout and re-fit ONLY the levels against every snapshot
     (cheap coordinate descent, warm-started from the neighbouring frame).
     This handles the PM nonlinearity exactly - no linear attribution.
  4. Fit each bar's ADSR (k, EnvelopeAmount, Attack, Decay, Sustain) to its
     level trajectory, using the engine's exact envelope curve
     (exponentialControl pinned at 0.50 -> curve constant K = 4).
  5. Fit the AMP envelope to the snapshots' loudness (RMS) trajectory, so
     the patch also follows the source's decay/swell.

Conventions (in addition to the static tool's):
  * t = 0 is the FIRST snapshot (the green marker). Put the marker right
    where the note settles after the ictus - or at the very start of a slow
    bowed attack, which the drawbar/amp attacks will then fit.
  * carrierMorph has no envelope in the engine, so Density is static: it
    comes from the reference-snapshot fit and is held for all frames.
  * Drawbar ReleaseTime stays at 10 s (a 0 release collapses the tail to a
    bare sine while the amp envelope is still sounding - Gerard, 6 Jul).
  * exponentialControl is exported explicitly (0.50) because the envelope
    fits assume that curve.

Run with --selftest for a headless verification.
"""

import os
import sys
import queue
import threading
import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
_STATIC_DIR = os.path.normpath(os.path.join(_HERE, '..', 'Resynth'))
if _STATIC_DIR not in sys.path:
    sys.path.insert(0, _STATIC_DIR)

import ishtar_resynth as core
from ishtar_resynth import (TWO_PI, BRILLIANCE, M_CYCLE, PATCH_EXPORT_DIR,
                            softsaw, carrier, synth_cycle, level_grid,
                            detect_period, extract_target, Objective, Fitter)

# --------------------------------------------------------------------------
# ENGINE ENVELOPE MODEL (mirrors PLANETVoice::processEnvelope exactly)
# --------------------------------------------------------------------------
ENV_CURVE_CONTROL = 0.5                    # exponentialControl, pinned
ENV_K = 1.0 + ENV_CURVE_CONTROL * 6.0      # PLANETVoice::refreshEnvCurveCache
_EK = float(np.exp(-ENV_K))
_ONE_MINUS_EK = 1.0 - _EK

SPACING_MIN_MS, SPACING_MAX_MS = 25, 200
FRAMES_MIN, FRAMES_MAX, FRAMES_DEFAULT = 2, 16, 8
RICHNESS_THRESH_DB = -40.0                 # harmonic counts above this rel dB


def env_alpha_beta(t, A, D):
    """Decompose the held-note engine envelope as env(t) = alpha + S*beta,
    which is LINEAR in the sustain level S (the attack and decay shapes do
    not depend on S). Vectorised over t. A, D are stage times in seconds."""
    t = np.asarray(t, dtype=float)
    alpha = np.zeros_like(t)
    beta = np.zeros_like(t)
    att = t < A
    if np.any(att):
        p = t[att] / A
        alpha[att] = (1.0 - np.exp(-ENV_K * p)) / _ONE_MINUS_EK
    dec = (~att) & (t < A + D)
    if np.any(dec):
        p = (t[dec] - A) / D
        c = (np.exp(-ENV_K * p) - _EK) / _ONE_MINUS_EK
        alpha[dec] = c
        beta[dec] = 1.0 - c
    beta[t >= A + D] = 1.0
    return alpha, beta


def env_value(t, A, D, S):
    """Engine envelope value(s) at time t (note held)."""
    a, b = env_alpha_beta(np.atleast_1d(t), A, D)
    v = a + S * b
    return v if np.ndim(t) else float(v[0])


def bar_level(t, prm):
    """Effective drawbar level under an env fit dict: k + amount * env(t)."""
    return prm['base'] + prm['amount'] * env_value(t, prm['attack'],
                                                   prm['decay'], prm['sustain'])


# --------------------------------------------------------------------------
# ADSR FIT (trajectory -> engine envelope parameters)
# --------------------------------------------------------------------------
def fit_adsr(times, levels, s_lo=0.0, s_hi=2.0, allow_base=True,
             amount_lo=-5.0, amount_hi=20.0, base_lo=-2.0, base_hi=2.0):
    """Fit level(t) ~ k + amount * env(t; A, D, S) to sampled points.

    Grid over (A, D) log-spaced, then over S linearly; for each triple the
    remaining (k, amount) problem is linear least squares in closed form.
    A base-pinned (k = 0) variant is preferred unless the free-base fit is
    clearly better, keeping the static tool's k-sliders-at-zero convention.
    Returns dict(base, amount, attack, decay, sustain, err)."""
    t = np.asarray(times, dtype=float)
    s = np.asarray(levels, dtype=float)
    n = len(t)
    # Constant trajectory -> the static gate convention, exactly.
    if np.ptp(s) < max(1e-4, 0.02 * float(np.max(np.abs(s), initial=0.0))):
        return dict(base=0.0, amount=float(np.mean(s)), attack=0.001,
                    decay=0.001, sustain=1.0, err=float(np.std(s)))

    t_end = max(float(t[-1]), 0.02)
    A_grid = np.unique(np.concatenate([[0.001],
                                       np.geomspace(0.005, t_end, 22)]))
    D_grid = np.unique(np.concatenate([[0.001],
                                       np.geomspace(0.01, 10.0, 30)]))
    S_grid = np.linspace(s_lo, s_hi, 41)

    def solve(G, free_base):
        """Closed-form LS of s ~ k + u*g per row of G; returns (err, u, k)."""
        if free_base:
            gm = G.mean(axis=1)
            sm = float(s.mean())
            cov = (G @ s) / n - gm * sm
            var = (G * G).mean(axis=1) - gm * gm
            u = cov / np.maximum(var, 1e-12)
            u = np.clip(u, amount_lo, amount_hi)
            k = np.clip(sm - u * gm, base_lo, base_hi)
            bad = var < 1e-10
        else:
            gg = (G * G).sum(axis=1)
            u = np.clip((G @ s) / np.maximum(gg, 1e-12), amount_lo, amount_hi)
            k = np.zeros_like(u)
            bad = gg < 1e-10
        resid = k[:, None] + u[:, None] * G - s[None, :]
        err = np.sqrt(np.mean(resid * resid, axis=1))
        err[bad] = np.inf
        return err, u, k

    def eval_triple(A, D, S, free_base):
        alpha, beta = env_alpha_beta(t, A, D)
        G = (alpha + S * beta)[None, :]
        err, u, k = solve(G, free_base)
        return float(err[0]), float(k[0]), float(u[0])

    def polish(state, free_base, rounds=4):
        """Continuous local refinement of (A, D, S) around the grid winner;
        (k, amount) re-solved in closed form at every step."""
        e, k, u, A, D, S = state
        for _ in range(rounds):
            improved = False
            for cand in A * np.geomspace(0.6, 1.6, 9):
                cand = float(np.clip(cand, 0.001, 10.0))
                ce, ck, cu = eval_triple(cand, D, S, free_base)
                if ce < e - 1e-12:
                    e, k, u, A = ce, ck, cu, cand
                    improved = True
            for cand in D * np.geomspace(0.6, 1.6, 9):
                cand = float(np.clip(cand, 0.001, 10.0))
                ce, ck, cu = eval_triple(A, cand, S, free_base)
                if ce < e - 1e-12:
                    e, k, u, D = ce, ck, cu, cand
                    improved = True
            for cand in S + np.linspace(-0.08, 0.08, 9):
                cand = float(np.clip(cand, s_lo, s_hi))
                ce, ck, cu = eval_triple(A, D, cand, free_base)
                if ce < e - 1e-12:
                    e, k, u, S = ce, ck, cu, cand
                    improved = True
            if not improved:
                break
        return (e, k, u, A, D, S)

    best_pin = None    # (err, base, amount, A, D, S)
    best_free = None
    for A in A_grid:
        for D in D_grid:
            alpha, beta = env_alpha_beta(t, A, D)
            G = alpha[None, :] + S_grid[:, None] * beta[None, :]
            err, u, k = solve(G, free_base=False)
            j = int(np.argmin(err))
            if best_pin is None or err[j] < best_pin[0]:
                best_pin = (float(err[j]), 0.0, float(u[j]), float(A),
                            float(D), float(S_grid[j]))
            if allow_base:
                err, u, k = solve(G, free_base=True)
                j = int(np.argmin(err))
                if best_free is None or err[j] < best_free[0]:
                    best_free = (float(err[j]), float(k[j]), float(u[j]),
                                 float(A), float(D), float(S_grid[j]))

    best_pin = polish(best_pin, free_base=False)
    if allow_base and best_free is not None:
        best_free = polish(best_free, free_base=True)

    # Keep the k-slider-at-zero convention unless a free base is DECISIVELY
    # better in both relative AND absolute terms (it is genuinely needed for
    # sign-crossing trajectories, where the pinned model structurally cannot
    # follow; on mere noise it produces confusing equivalent forms like
    # k=1.6 with a negative amount).
    chosen = best_pin
    if allow_base and best_free is not None \
            and best_pin[0] > 2.0 * best_free[0] \
            and best_pin[0] - best_free[0] > 0.02:
        chosen = best_free
    e, k, u, A, D, S = chosen
    return dict(base=k, amount=u, attack=A, decay=D, sustain=S, err=e)


# --------------------------------------------------------------------------
# FRAME ANALYSIS
# --------------------------------------------------------------------------
def analyse_frames(audio, sr, start, period, C, n_frames, spacing_s, n_avg):
    """Extract up to n_frames snapshots, spacing_s apart, from `start`.
    Each is a normalised averaged cycle (same machinery as the static tool)
    plus the RAW RMS of its analysis span (for the amp-envelope fit).
    Cycle averaging is clamped so a window never exceeds the spacing.

    Each snapshot's time t is its analysis-window CENTRE (that is what the
    averaged cycles represent), never the window start: the engine envelope
    is exactly 0 at t = 0, so a t = 0 observation would be unfittable."""
    spacing = spacing_s * sr
    fit_cycles = max(1, int(spacing // (C * period)))
    n_eff = max(1, min(n_avg, fit_cycles))
    frames = []
    for k in range(n_frames):
        s0 = start + k * spacing
        cyc, used = extract_target(audio, s0, period, n_eff, C)
        if cyc is None:
            break
        span = int(used * C * period)
        raw = audio[int(s0):int(s0) + max(span, 1)]
        rms = float(np.sqrt(np.mean(raw ** 2))) if len(raw) else 0.0
        t = k * spacing_s + 0.5 * used * C * period / sr
        frames.append(dict(t=t, start=s0, cycle=cyc, used=used, rms=rms))
    return frames, n_eff


def frame_richness(ob):
    """Number of band bins above RICHNESS_THRESH_DB rel. the frame's peak."""
    mags = np.abs(ob.Tb)
    peak = float(mags.max())
    if peak <= 0.0:
        return 0
    return int(np.sum(mags > peak * (10.0 ** (RICHNESS_THRESH_DB / 20.0))))


# --------------------------------------------------------------------------
# LEVEL-ONLY REFIT (frozen layout, one snapshot at a time)
# --------------------------------------------------------------------------
class LevelRefitter:
    """Coordinate descent over drawbar levels with (f, route, d) and the
    carrier Density frozen. Scores real synthesized spectra, so PM
    interactions are handled exactly. The candidate grid is fine (the
    envelope stores continuous values - no knob quantisation applies)."""

    def __init__(self, layout, m, C, mode, G_fine=160):
        self.layout = layout            # [(f, route, d)]
        self.m = m
        self.mode = mode
        self.CM = C * M_CYCLE
        self.theta = TWO_PI * np.arange(self.CM) / M_CYCLE
        self.W = []
        for f, route, d in layout:
            w = np.sin(f * self.theta)
            if d > 0.0:
                w = (1.0 - d) * w + d * softsaw(f * self.theta)
            self.W.append(w)
        self.grid = np.unique(np.concatenate([level_grid(G_fine), [0.0]]))

    def _sums(self, s, exclude=None):
        pm = np.zeros(self.CM)
        add = np.zeros(self.CM)
        for j, (f, route, d) in enumerate(self.layout):
            if j == exclude:
                continue
            if route == 'PM':
                pm += BRILLIANCE * s[j] * self.W[j]
            else:
                add += BRILLIANCE * 0.5 * np.clip(s[j], -2.0, 2.0) * self.W[j]
        return pm, add

    def eval_err(self, ob, s):
        pm, add = self._sums(s)
        y = carrier(self.theta + pm, self.m) + add
        Yb = np.fft.rfft(y)[1:ob.K + 1][None, :]
        return float(ob.err(Yb, self.mode)[0])

    # Fingerprint error is (near-)blind to a bar's SIGN: a mirrored level is
    # often degenerate to within ~0.01%. Left free, the refit can hop to the
    # mirror solution mid-trajectory, planting a wild outlier that wrecks
    # that bar's envelope fit. The anchor term pulls candidates toward the
    # warm-start (neighbouring snapshot's) levels - far stronger than the
    # degeneracy gap, far weaker than genuine spectral structure. Reported
    # errors are always computed WITHOUT the penalty.
    ANCHOR_WEIGHT = 0.25      # error-% per unit of level distance

    def refit(self, ob, s0, sweeps=3):
        s = np.asarray(s0, dtype=float).copy()
        anchor = s.copy()
        for _ in range(sweeps):
            improved = False
            for j, (f, route, d) in enumerate(self.layout):
                pm, add = self._sums(s, exclude=j)
                cand = np.append(self.grid, s[j])   # last = current (monotone)
                if route == 'PM':
                    phi = (self.theta + pm)[None, :] \
                        + (BRILLIANCE * cand)[:, None] * self.W[j][None, :]
                    y = carrier(phi, self.m) + add[None, :]
                    Yb = np.fft.rfft(y, axis=-1)[:, 1:ob.K + 1]
                else:
                    ybase = carrier(self.theta + pm, self.m) + add
                    Ybase = np.fft.rfft(ybase)[1:ob.K + 1]
                    Wspec = np.fft.rfft(self.W[j])[1:ob.K + 1]
                    amp = BRILLIANCE * 0.5 * np.clip(cand, -2.0, 2.0)
                    Yb = Ybase[None, :] + amp[:, None] * Wspec[None, :]
                errs = ob.err(Yb, self.mode) \
                    + self.ANCHOR_WEIGHT * np.abs(cand - anchor[j])
                b = int(np.argmin(errs))
                if b != len(cand) - 1 and errs[b] < errs[-1] - 1e-9:
                    s[j] = float(cand[b])
                    improved = True
            if not improved:
                break
        return s, self.eval_err(ob, s)


# --------------------------------------------------------------------------
# PIPELINE
# --------------------------------------------------------------------------
def run_dynamic_fit(frames, C, f0, sr, cfg, progress=None):
    """Full Dynamic Resynth pipeline over prepared frames.
    progress: callable(frac, msg) -> bool (False = cancel).
    Returns the result dict, or None if cancelled."""
    progress = progress or (lambda frac, msg: True)
    N = len(frames)
    obs = [Objective(fr['cycle'], C, f0, sr) for fr in frames]
    rich = [frame_richness(ob) for ob in obs]
    ref = int(np.argmax(rich))

    # ---- stage 1: full static fit on the richest snapshot ----
    def p1(fr, msg):
        return progress(0.72 * fr, f"[ref #{ref + 1}] {msg}" if msg else '')

    fitter = Fitter(obs[ref], cfg, p1)
    fit = fitter.run()
    if fitter.cancelled:
        return None
    layout = [(f, route, d) for f, s, route, d in fit['bars']]
    s_ref = [s for f, s, route, d in fit['bars']]
    m = fit['density']

    # ---- stage 2: level-only refit per snapshot, warm-started outward ----
    rf = LevelRefitter(layout, m, C, cfg['mode'])
    levels = [None] * N
    err_refit = [0.0] * N
    order = sorted(range(N), key=lambda k: (abs(k - ref), k))
    for c, k in enumerate(order):
        if not progress(0.72 + 0.20 * c / max(N, 1),
                        f"Refitting levels, snapshot {k + 1}/{N}..."):
            return None
        if k == ref:
            init = s_ref
        else:
            src = k + 1 if k < ref else k - 1
            init = levels[src] if levels[src] is not None else s_ref
        levels[k], err_refit[k] = rf.refit(obs[k], init)

    # ---- stage 3: per-bar ADSR fits ----
    if not progress(0.93, "Fitting drawbar envelopes..."):
        return None
    times = np.array([fr['t'] for fr in frames])
    bar_env = []
    for j in range(len(layout)):
        traj = np.array([levels[k][j] for k in range(N)])
        bar_env.append(fit_adsr(times, traj))

    # ---- stage 4: amp envelope from the loudness trajectory ----
    if not progress(0.96, "Fitting amplitude envelope..."):
        return None
    rms = np.array([fr['rms'] for fr in frames])
    rms_n = rms / max(float(rms.max()), 1e-12)
    amp_env = fit_adsr(times, rms_n, s_lo=0.0, s_hi=1.0, allow_base=False,
                       amount_lo=0.0, amount_hi=5.0)

    # ---- stage 5: honest reconstruction residual per snapshot ----
    levels_env = np.array([[bar_level(t, prm) for prm in bar_env]
                           for t in times])
    err_env = [rf.eval_err(obs[k], levels_env[k]) for k in range(N)]

    progress(1.0, "Done.")
    return {
        'layout': layout, 'density': m, 'ref': ref, 'richness': rich,
        'times': times.tolist(), 'levels': [list(map(float, lv)) for lv in levels],
        'levels_env': levels_env.tolist(), 'bar_env': bar_env,
        'amp_env': amp_env, 'rms_norm': rms_n.tolist(),
        'err_refit': [float(e) for e in err_refit],
        'err_env': [float(e) for e in err_env],
        'ref_fit': fit, 'mode': cfg['mode'], 'G': cfg['G'],
        'bar_density': bool(cfg.get('allow_bar_density')),
        'n_frames': N,
    }


# --------------------------------------------------------------------------
# PATCH EXPORT
# --------------------------------------------------------------------------
def make_dynamic_patch_markdown(name, dyn, f0, source_name, spacing_ms):
    layout = dyn['layout']
    bar_env = dyn['bar_env']
    amp = dyn['amp_env']
    bar_density = dyn.get('bar_density', False)
    fit = dyn['ref_fit']
    mean_env_err = float(np.mean(dyn['err_env']))
    max_env_err = float(np.max(dyn['err_env']))
    lines = []
    ap = lines.append
    ap(f"# {name}")
    ap("")
    mode_name = ("harmonic fingerprint" if dyn['mode'] == 'spec'
                 else "literal waveform")
    ap(f"Resynthesized from {source_name} (f0 ~ {f0:.1f} Hz) by ISHTAR "
       f"Dynamic Resynth: {dyn['n_frames']} snapshots at {spacing_ms:.0f} ms, "
       f"layout fitted on snapshot {dyn['ref'] + 1} (richest), drawbar "
       f"envelopes fitted to the level trajectories, amp envelope to the "
       f"loudness trajectory. Fit mode: {mode_name}, {len(layout)} drawbars, "
       f"granularity {dyn['G']}. Reference residual: fingerprint "
       f"{fit['err_spec']:.1f}%, waveform {fit['err_wave']:.1f}%. "
       f"Envelope reconstruction residual across snapshots: mean "
       f"{mean_env_err:.1f}%, worst {max_env_err:.1f}%. Brilliance is at "
       f"noon by design; exponentialControl is pinned at 0.50 (the envelope "
       f"fits assume that curve). All parameters not listed load as defaults.")
    if bar_density:
        ap("")
        ap("NOTE: fitted WITH per-drawbar Density (kNDensity) - needs the "
           "per-drawbar-density fork build of ISHTAR. Mainline ISHTAR loads "
           "this patch but ignores the kNDensity lines, so it will sound "
           "different.")
    ap("")
    ap("Tags: resynth, dynamic" + (", per-drawbar-density" if bar_density else ""))
    ap("")
    ap("---")
    ap("")
    ap("## Amplitude Envelope")
    ap(f"ampEnvAttackTime = {amp['attack']:.3f}  (0.001 to 10.0)")
    ap(f"ampEnvDecayTime = {amp['decay']:.3f}  (0.001 to 10.0)")
    ap(f"ampEnvSustainLevel = {amp['sustain']:.3f}  (0.000 to 1.0)")
    ap("ampEnvReleaseTime = 2.00  (0.001 to 10.0)")
    ap("exponentialControl = 0.50  (0.000 to 1.0)")
    ap("")
    ap("## Colour")
    ap("brilliance = 0.50  (0.000 to 1.0)")
    ap(f"carrierMorph = {dyn['density']:.3f}  (0.000 to 1.0)")
    ap("")
    ap("---")
    ap("")
    ap("## Drawbar Parameters Reference")
    ap("# k = -2.0 to 2.0")
    ap("# EnvelopeAmount = -5.0 to 20.0")
    ap("# input_f = 0.5 to 30.0 (0.5 steps)")
    ap("# ToPM = 1 -> phase-distortion path, ToOut = 1 -> additive partial")
    if bar_density:
        ap("# Density = 0.0 to 1.0 (modulator sine -> soft-saw; FORK BUILD ONLY)")
    ap("")

    for i in range(1, 11):
        p = f"k{i}"
        ap(f"## Drawbar {i}")
        if i <= len(layout):
            f, route, d = layout[i - 1]
            prm = bar_env[i - 1]
            to_pm, to_out = (1, 0) if route == 'PM' else (0, 1)
            ap(f"{p} = {prm['base']:.4f}  (-2.000 to 2.0)")
            ap(f"{p}EnvelopeAmount = {prm['amount']:.4f}  (-5.000 to 20.0)")
            ap(f"{p}LFOAmount = 0.00  (-5.000 to 5.0)")
            ap(f"{p}VelToHarmonic = 0.00  (-100.000 to 100.0)")
            ap(f"input_f{i} = {f:.2f}  (0.500 to 30.0)")
            if bar_density:
                ap(f"{p}Density = {d:.3f}  (0.000 to 1.0)")
            ap(f"{p}AttackTime = {prm['attack']:.3f}  (0.001 to 10.0)")
            ap(f"{p}DecayTime = {prm['decay']:.3f}  (0.001 to 10.0)")
            ap(f"{p}SustainLevel = {prm['sustain']:.3f}  (0.000 to 2.0)")
            # Release = max, NOT 0: a 0 release collapses the timbre to a bare
            # sine the instant the amp envelope enters its tail (Gerard, 6 Jul)
            ap(f"{p}ReleaseTime = 10.00  (0.001 to 10.0)")
            ap(f"{p}LFOShape = 1  (1.000 to 3.0)")
            ap(f"{p}LFORate = 4.00  (0.050 to 1000.0)")
            ap(f"{p}LFOSync = 0  (0.000 to 1.0)")
            ap(f"{p}LFOSyncDiv = 7  (0.000 to 12.0)")
            ap(f"{p}ToPM = {to_pm}  (0.000 to 1.0)")
            ap(f"{p}ToOut = {to_out}  (0.000 to 1.0)")
            ap(f"{p}TrigSingle = 0  (0.000 to 1.0)")
        else:
            ap(f"{p} = 0.00  (-2.000 to 2.0)")
            ap(f"{p}EnvelopeAmount = 0.00  (-5.000 to 20.0)")
            ap(f"{p}LFOAmount = 0.00  (-5.000 to 5.0)")
            ap(f"{p}VelToHarmonic = 0.00  (-100.000 to 100.0)")
            ap(f"input_f{i} = {float(i):.2f}  (0.500 to 30.0)")
            if bar_density:
                ap(f"{p}Density = 0.000  (0.000 to 1.0)")
            ap(f"{p}AttackTime = 0.10  (0.001 to 10.0)")
            ap(f"{p}DecayTime = 0.50  (0.001 to 10.0)")
            ap(f"{p}SustainLevel = 0.50  (0.000 to 2.0)")
            ap(f"{p}ReleaseTime = 2.00  (0.001 to 10.0)")
            ap(f"{p}LFOShape = 1  (1.000 to 3.0)")
            ap(f"{p}LFORate = 4.00  (0.050 to 1000.0)")
            ap(f"{p}LFOSync = 0  (0.000 to 1.0)")
            ap(f"{p}LFOSyncDiv = 7  (0.000 to 12.0)")
            ap(f"{p}ToPM = 1  (0.000 to 1.0)")
            ap(f"{p}ToOut = 0  (0.000 to 1.0)")
            ap(f"{p}TrigSingle = 0  (0.000 to 1.0)")
        ap("")
    return "\n".join(lines)


# --------------------------------------------------------------------------
# DYNAMIC AUDITION RENDER
# --------------------------------------------------------------------------
def render_dynamic(dyn, C, f0, sr, seconds):
    """Render the fitted patch with its envelopes running: coefficients are
    promoted once per C-cycle block (the engine promotes per carrier cycle),
    each block's cycle is resampled to the true period and scaled by the
    fitted amp envelope."""
    layout = dyn['layout']
    bar_env = dyn['bar_env']
    amp = dyn['amp_env']
    L = C * sr / f0                 # block length in output samples (float)
    CM = C * M_CYCLE
    n = int(seconds * sr)
    out = np.zeros(n)
    xp = np.arange(CM + 1)
    b = 0
    while True:
        start = int(round(b * L))
        if start >= n:
            break
        stop = min(int(round((b + 1) * L)), n)
        t = b * L / sr
        bars = [(f, bar_level(t, bar_env[j]), route, d)
                for j, (f, route, d) in enumerate(layout)]
        cyc = synth_cycle(bars, dyn['density'], cycles=C)
        fp = np.append(cyc, cyc[0])
        pos = (np.arange(start, stop) - b * L) * (CM / L)
        seg = np.interp(np.clip(pos, 0.0, CM), xp, fp)
        out[start:stop] = seg * env_value(t, amp['attack'], amp['decay'],
                                          amp['sustain'])
        b += 1
    return out


def render_dynamic_ab(audio, sr, start, dyn, C, f0, seconds, gap=0.4):
    """Source segment (from the first snapshot), silence, fitted patch with
    envelopes running - one WAV for A/B by ear. Each clip is peak-normalised
    so the internal dynamics stay honest."""
    n = int(seconds * sr)
    src = np.array(audio[int(start):int(start) + n], dtype=float)
    fit = render_dynamic(dyn, C, f0, sr, len(src) / sr)

    def norm(y):
        peak = float(np.max(np.abs(y))) + 1e-12
        return y * (0.9 / peak)

    fade = max(int(0.01 * sr), 1)
    for y in (src, fit):
        y[-fade:] *= np.linspace(1, 0, fade)
    sil = np.zeros(int(gap * sr))
    return np.concatenate([norm(src), sil, norm(fit)]).astype(np.float32)


# --------------------------------------------------------------------------
# SELF-TEST (headless)
# --------------------------------------------------------------------------
def selftest():
    print("ISHTAR Dynamic Resynth self-test")
    print("=" * 60)
    ok = True

    # ---- 1) ADSR curve fitter in isolation ----
    # Times mimic analyse_frames: window CENTRES, so t = 0 (where the engine
    # envelope is pinned at 0) is never observed. Curves are compared over
    # the observed span - the attack shape before the first observation is
    # unobservable by construction.
    print("\n[1] ADSR trajectory fitter")
    times = np.arange(8) * 0.05 + 0.005
    cases = [
        ("EP-style decay", dict(base=0.0, amount=1.2, attack=0.001,
                                decay=0.35, sustain=0.4)),
        ("bowed swell", dict(base=0.0, amount=0.9, attack=0.25,
                             decay=2.0, sustain=0.8)),
        ("sign-crossing", dict(base=-0.5, amount=1.5, attack=0.001,
                               decay=0.20, sustain=0.2)),
        ("static bar", dict(base=0.0, amount=0.7, attack=0.001,
                            decay=0.001, sustain=1.0)),
    ]
    for desc, truth in cases:
        traj = np.array([bar_level(t, truth) for t in times])
        fitp = fit_adsr(times, traj)
        t_dense = np.linspace(times[0], times[-1], 200)
        c_true = np.array([bar_level(t, truth) for t in t_dense])
        c_fit = np.array([bar_level(t, fitp) for t in t_dense])
        rms = float(np.sqrt(np.mean((c_true - c_fit) ** 2)))
        scale = max(float(np.max(np.abs(c_true))), 1e-9)
        print(f"  {desc:16s} curve RMS {rms:.4f} (scale {scale:.2f}) "
              f"A {fitp['attack']:.3f} D {fitp['decay']:.3f} "
              f"S {fitp['sustain']:.2f} amt {fitp['amount']:+.2f} "
              f"k {fitp['base']:+.2f}")
        if rms > 0.05 * scale:
            ok = False
            print("  ** FAIL: envelope curve mismatch above 5%")

    # ---- 2) end-to-end on a synthetic evolving target ----
    print("\n[2] End-to-end: synthetic 3-bar target with known envelopes")
    layout_truth = [(1.0, 'PM', 0.0), (4.0, 'PM', 0.0), (7.0, 'ADD', 0.0)]
    env_truth = [
        dict(base=0.0, amount=0.8, attack=0.001, decay=0.001, sustain=1.0),
        dict(base=0.0, amount=1.0, attack=0.001, decay=0.25, sustain=0.3),
        dict(base=0.0, amount=0.6, attack=0.15, decay=0.60, sustain=0.5),
    ]
    amp_truth = dict(base=0.0, amount=1.0, attack=0.001, decay=0.5,
                     sustain=0.4)
    f0, sr, C = 220.0, 44100.0, 1
    frames = []
    for k in range(8):
        t = k * 0.05 + 0.005          # window-centre times, as analyse_frames
        bars = [(f, bar_level(t, env_truth[j]), route, d)
                for j, (f, route, d) in enumerate(layout_truth)]
        cyc = synth_cycle(bars, 0.0, cycles=C)
        cyc = cyc - cyc.mean()
        cyc /= np.linalg.norm(cyc)
        frames.append(dict(t=t, start=0, cycle=cyc, used=1,
                           rms=0.2 * env_value(t, amp_truth['attack'],
                                               amp_truth['decay'],
                                               amp_truth['sustain'])))
    cfg = dict(n_bars=3, G=120, mode='spec', allow_add=True,
               allow_density=False, allow_bar_density=False, half_f=False)
    dyn = run_dynamic_fit(frames, C, f0, sr, cfg,
                          progress=lambda fr, msg: True)
    print(f"  reference snapshot: #{dyn['ref'] + 1} "
          f"(richness {dyn['richness']})")
    print(f"  layout: {[(f, r) for f, r, d in dyn['layout']]} "
          f"m={dyn['density']:.3f}")
    for k in range(len(frames)):
        print(f"  snap {k + 1}: t {frames[k]['t'] * 1000:4.0f} ms  "
              f"refit err {dyn['err_refit'][k]:5.2f}%  "
              f"env err {dyn['err_env'][k]:5.2f}%")
        if dyn['err_env'][k] > 6.0:
            ok = False
            print("  ** FAIL: envelope reconstruction above 6% on a "
                  "reachable target")
    amp_fit = dyn['amp_env']
    t_dense = np.linspace(frames[0]['t'], frames[-1]['t'], 200)
    a_true = env_value(t_dense, amp_truth['attack'], amp_truth['decay'],
                       amp_truth['sustain'])
    a_fit = amp_fit['amount'] * env_value(t_dense, amp_fit['attack'],
                                          amp_fit['decay'],
                                          amp_fit['sustain'])
    arms = float(np.sqrt(np.mean((a_true - a_fit) ** 2)))
    print(f"  amp envelope curve RMS {arms:.4f} "
          f"(A {amp_fit['attack']:.3f} D {amp_fit['decay']:.3f} "
          f"S {amp_fit['sustain']:.2f})")
    if arms > 0.06:
        ok = False
        print("  ** FAIL: amp envelope mismatch above 6%")

    # ---- 3) export + render smoke test ----
    print("\n[3] Export / render smoke test")
    md = make_dynamic_patch_markdown("selftest", dyn, f0, "selftest", 50)
    for needle in ("ampEnvAttackTime", "exponentialControl = 0.50",
                   "k1EnvelopeAmount", "Tags: resynth, dynamic"):
        if needle not in md:
            ok = False
            print(f"  ** FAIL: '{needle}' missing from patch export")
    has_density_lines = any(ln.startswith("k") and "Density =" in ln
                            for ln in md.splitlines())
    if has_density_lines:
        ok = False
        print("  ** FAIL: kNDensity lines present with per-drawbar "
              "density off")
    wav = render_dynamic(dyn, C, f0, sr, 0.5)
    if not np.all(np.isfinite(wav)) or float(np.max(np.abs(wav))) <= 0.0:
        ok = False
        print("  ** FAIL: dynamic render produced silence or non-finite "
              "samples")
    else:
        print(f"  render: {len(wav)} samples, peak "
              f"{float(np.max(np.abs(wav))):.3f}")

    print("\n" + ("SELF-TEST PASSED" if ok else "SELF-TEST FAILED"))
    return 0 if ok else 1


# --------------------------------------------------------------------------
# GUI
# --------------------------------------------------------------------------
def run_gui():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    import soundfile as sf
    import matplotlib
    matplotlib.use("TkAgg")
    from matplotlib.figure import Figure
    from matplotlib.backends.backend_tkagg import (FigureCanvasTkAgg,
                                                   NavigationToolbar2Tk)

    BAR_COLOURS = ['#ffb050', '#58a8ff', '#50e050', '#ff6060', '#c080ff',
                   '#50d8d8', '#ffe060', '#ff90c0', '#a0a0ff', '#80ff80']

    class App:
        def __init__(self, root):
            self.root = root
            root.title("ISHTAR Dynamic Resynth")
            root.geometry("1240x900")

            self.audio = None
            self.sr = None
            self.wav_name = ""
            self.period = None
            self.markers = [0.0, 0.0]
            self.drag_idx = None
            self.result = None
            self.frames = None
            self.frame_marks = []
            self.fit_thread = None
            self.msg_q = queue.Queue()
            self.cancel_flag = threading.Event()

            self._build_widgets()
            root.after(100, self._poll_queue)

        # ---------------- layout ----------------
        def _build_widgets(self):
            top = ttk.Frame(self.root)
            top.pack(fill='x', padx=6, pady=4)
            ttk.Button(top, text="Open WAV...", command=self.open_wav).pack(side='left')
            self.lbl_file = ttk.Label(top, text="(no file)")
            self.lbl_file.pack(side='left', padx=8)
            ttk.Button(top, text="Detect pitch in region",
                       command=self.detect).pack(side='left', padx=4)
            ttk.Button(top, text="Snap markers to zero-crossings",
                       command=self.snap_markers).pack(side='left', padx=4)
            self.lbl_pitch = ttk.Label(top, text="f0: -")
            self.lbl_pitch.pack(side='left', padx=10)

            self.mode_var = tk.StringVar(value='auto')
            ttk.Radiobutton(top, text="Auto (detect pitch in region)",
                            variable=self.mode_var, value='auto').pack(side='left', padx=4)
            ttk.Radiobutton(top, text="Manual (markers = one cycle)",
                            variable=self.mode_var, value='manual').pack(side='left')

            # waveform figure
            self.fig_wave = Figure(figsize=(11, 2.4), dpi=90)
            self.ax_wave = self.fig_wave.add_subplot(111)
            self.ax_wave.set_facecolor('#101018')
            self.fig_wave.patch.set_facecolor('#181820')
            self.canvas_wave = FigureCanvasTkAgg(self.fig_wave, master=self.root)
            self.canvas_wave.get_tk_widget().pack(fill='x', padx=6)
            tbf = ttk.Frame(self.root)
            tbf.pack(fill='x', padx=6)
            NavigationToolbar2Tk(self.canvas_wave, tbf)
            self.canvas_wave.mpl_connect('button_press_event', self._on_press)
            self.canvas_wave.mpl_connect('motion_notify_event', self._on_motion)
            self.canvas_wave.mpl_connect('button_release_event', self._on_release)

            # fit controls
            ctl = ttk.LabelFrame(self.root, text="Dynamic fit")
            ctl.pack(fill='x', padx=6, pady=4)
            ttk.Label(ctl, text="Drawbars:").grid(row=0, column=0, padx=4)
            self.bars_var = tk.IntVar(value=5)
            ttk.Spinbox(ctl, from_=1, to=10, width=4,
                        textvariable=self.bars_var).grid(row=0, column=1)
            ttk.Label(ctl, text="Granularity:").grid(row=0, column=2, padx=(12, 2))
            self.gran_var = tk.IntVar(value=60)
            gran = ttk.Scale(ctl, from_=20, to=200, orient='horizontal', length=140,
                             command=lambda v: self.gran_var.set(int(float(v))))
            gran.set(60)
            gran.grid(row=0, column=3)
            self.lbl_gran = ttk.Label(ctl, text="60")
            self.lbl_gran.grid(row=0, column=4, padx=2)
            self.gran_var.trace_add('write',
                                    lambda *a: self.lbl_gran.config(text=str(self.gran_var.get())))

            ttk.Label(ctl, text="Snapshots:").grid(row=0, column=5, padx=(12, 2))
            self.frames_var = tk.IntVar(value=FRAMES_DEFAULT)
            ttk.Spinbox(ctl, from_=FRAMES_MIN, to=FRAMES_MAX, width=4,
                        textvariable=self.frames_var).grid(row=0, column=6)
            ttk.Label(ctl, text="Spacing (ms):").grid(row=0, column=7, padx=(12, 2))
            self.spacing_var = tk.IntVar(value=50)
            ttk.Spinbox(ctl, from_=SPACING_MIN_MS, to=SPACING_MAX_MS,
                        increment=5, width=5,
                        textvariable=self.spacing_var).grid(row=0, column=8)
            ttk.Label(ctl, text="Cycles/snapshot:").grid(row=0, column=9, padx=(12, 2))
            self.avg_var = tk.IntVar(value=4)
            ttk.Spinbox(ctl, from_=1, to=64, width=4,
                        textvariable=self.avg_var).grid(row=0, column=10)

            self.obj_var = tk.StringVar(value='spec')
            ttk.Radiobutton(ctl, text="Harmonic fingerprint", variable=self.obj_var,
                            value='spec').grid(row=1, column=0, columnspan=2, sticky='w', padx=4)
            ttk.Radiobutton(ctl, text="Literal waveform", variable=self.obj_var,
                            value='wave').grid(row=1, column=2, sticky='w')
            self.add_var = tk.BooleanVar(value=True)
            self.den_var = tk.BooleanVar(value=True)
            self.bar_den_var = tk.BooleanVar(value=False)
            self.half_var = tk.BooleanVar(value=False)
            ttk.Checkbutton(ctl, text="Allow additive routing (F8)",
                            variable=self.add_var).grid(row=1, column=3, columnspan=2, sticky='w')
            ttk.Checkbutton(ctl, text="Allow Carrier Density",
                            variable=self.den_var).grid(row=1, column=5, columnspan=2, sticky='w')
            ttk.Checkbutton(ctl, text="Half-integer f (auto mode)",
                            variable=self.half_var).grid(row=1, column=7, columnspan=2, sticky='w')
            ttk.Checkbutton(ctl, text="Per-drawbar Density (fork build only)",
                            variable=self.bar_den_var).grid(row=1, column=9,
                                                            columnspan=3, sticky='w')

            self.btn_fit = ttk.Button(ctl, text="DYNAMIC FIT", command=self.start_fit)
            self.btn_fit.grid(row=0, column=11, rowspan=2, padx=12, ipadx=10, ipady=4)
            self.btn_cancel = ttk.Button(ctl, text="Cancel", command=self.cancel_fit,
                                         state='disabled')
            self.btn_cancel.grid(row=0, column=12, rowspan=2, padx=2)
            self.prog = ttk.Progressbar(ctl, length=200, mode='determinate')
            self.prog.grid(row=0, column=13, rowspan=2, padx=6)
            self.lbl_status = ttk.Label(ctl, text="")
            self.lbl_status.grid(row=0, column=14, rowspan=2, padx=4)

            # results
            res = ttk.Frame(self.root)
            res.pack(fill='both', expand=True, padx=6, pady=4)
            self.fig_res = Figure(figsize=(8.6, 3.4), dpi=90)
            self.ax_cycle = self.fig_res.add_subplot(131)
            self.ax_spec = self.fig_res.add_subplot(132)
            self.ax_traj = self.fig_res.add_subplot(133)
            self.fig_res.patch.set_facecolor('#181820')
            for ax in (self.ax_cycle, self.ax_spec, self.ax_traj):
                ax.set_facecolor('#101018')
            self.canvas_res = FigureCanvasTkAgg(self.fig_res, master=res)
            self.canvas_res.get_tk_widget().pack(side='left', fill='both', expand=True)

            right = ttk.Frame(res)
            right.pack(side='left', fill='both', padx=6)
            self.txt = tk.Text(right, width=58, height=18, font=("Consolas", 9),
                               bg='#101018', fg='#d8d8e8')
            self.txt.pack(fill='both', expand=True)
            bts = ttk.Frame(right)
            bts.pack(fill='x', pady=4)
            ttk.Button(bts, text="Export patch...",
                       command=self.export_patch).pack(side='left', padx=2)
            ttk.Button(bts, text="Render audition WAV...",
                       command=self.render_audition).pack(side='left', padx=2)

        # ---------------- file / markers ----------------
        def open_wav(self):
            path = filedialog.askopenfilename(
                filetypes=[("Audio", "*.wav *.flac *.aiff *.aif *.ogg"), ("All", "*.*")])
            if not path:
                return
            try:
                data, sr = sf.read(path, always_2d=True)
            except Exception as e:
                messagebox.showerror("ISHTAR Dynamic Resynth",
                                     f"Could not read file:\n{e}")
                return
            self.audio = data.mean(axis=1).astype(np.float64)
            self.sr = sr
            self.wav_name = os.path.basename(path)
            self.lbl_file.config(text=f"{self.wav_name}  ({sr} Hz, "
                                      f"{len(self.audio) / sr:.2f} s)")
            n = len(self.audio)
            self.markers = [n * 0.35, n * 0.65]
            self.frame_marks = []
            self.detect()
            self._draw_wave()

        def detect(self):
            if self.audio is None:
                return
            a, b = sorted(int(m) for m in self.markers)
            region = self.audio[max(0, a):min(len(self.audio), b)]
            p = detect_period(region, self.sr)
            if p is None:
                self.lbl_pitch.config(text="f0: not found")
                self.period = None
            else:
                self.period = p
                self.lbl_pitch.config(text=f"f0: {self.sr / p:.1f} Hz "
                                           f"({p:.1f} smp/cycle)")

        def snap_markers(self):
            if self.audio is None:
                return
            for i in (0, 1):
                s = int(self.markers[i])
                lo, hi = max(1, s - 400), min(len(self.audio) - 1, s + 400)
                seg = self.audio[lo:hi]
                zc = np.where((seg[:-1] <= 0) & (seg[1:] > 0))[0]
                if len(zc):
                    j = zc[np.argmin(np.abs(zc + lo - s))]
                    self.markers[i] = float(j + lo)
            self._draw_wave()

        def _draw_wave(self):
            self.ax_wave.clear()
            self.ax_wave.set_facecolor('#101018')
            if self.audio is not None:
                n = len(self.audio)
                step = max(1, n // 60000)
                x = np.arange(0, n, step)
                self.ax_wave.plot(x, self.audio[::step], lw=0.5, color='#58a8ff')
                for fm in self.frame_marks:
                    self.ax_wave.axvline(fm, color='#e0d040', lw=0.8, alpha=0.55)
                for i, c in ((0, '#50e050'), (1, '#ff6060')):
                    self.ax_wave.axvline(self.markers[i], color=c, lw=1.4, alpha=0.9)
                self.ax_wave.set_xlim(0, n)
            self.ax_wave.set_yticks([])
            self.ax_wave.tick_params(colors='#8888aa', labelsize=7)
            self.canvas_wave.draw_idle()

        def _on_press(self, ev):
            if ev.inaxes != self.ax_wave or self.audio is None:
                return
            if self.canvas_wave.toolbar.mode:
                return
            span = self.ax_wave.get_xlim()
            tol = (span[1] - span[0]) * 0.01
            d = [abs(ev.xdata - m) for m in self.markers]
            i = int(np.argmin(d))
            if d[i] < tol:
                self.drag_idx = i

        def _on_motion(self, ev):
            if self.drag_idx is None or ev.inaxes != self.ax_wave:
                return
            self.markers[self.drag_idx] = float(np.clip(ev.xdata, 0,
                                                        len(self.audio) - 1))
            self._draw_wave()

        def _on_release(self, ev):
            if self.drag_idx is not None:
                self.drag_idx = None
                if self.mode_var.get() == 'auto':
                    self.detect()

        # ---------------- fitting ----------------
        def start_fit(self):
            if self.audio is None:
                messagebox.showinfo("ISHTAR Dynamic Resynth", "Open a WAV first.")
                return
            if self.fit_thread and self.fit_thread.is_alive():
                return
            manual = (self.mode_var.get() == 'manual')
            a, b = sorted(self.markers)
            if manual:
                period = b - a
                if period < 8:
                    messagebox.showinfo("ISHTAR Dynamic Resynth",
                                        "Manual mode: markers must span one cycle.")
                    return
                self.period = period
                self.lbl_pitch.config(text=f"f0: {self.sr / period:.1f} Hz (manual)")
            else:
                self.detect()
            if self.period is None:
                messagebox.showinfo("ISHTAR Dynamic Resynth",
                                    "No pitch found - set markers manually.")
                return

            half_f = self.half_var.get()
            if half_f and manual:
                messagebox.showinfo("ISHTAR Dynamic Resynth",
                                    "Half-integer f needs auto mode (2-cycle analysis). "
                                    "Fitting with integer f instead.")
                half_f = False
            C = 2 if half_f else 1

            spacing_s = self.spacing_var.get() / 1000.0
            frames, n_eff = analyse_frames(self.audio, self.sr, a, self.period,
                                           C, self.frames_var.get(), spacing_s,
                                           max(1, self.avg_var.get()))
            if len(frames) < 3:
                messagebox.showinfo(
                    "ISHTAR Dynamic Resynth",
                    f"Only {len(frames)} snapshot(s) fit inside the audio from "
                    f"the green marker. Move the marker earlier, shorten the "
                    f"spacing, or use fewer snapshots (need at least 3).")
                return
            self.frames = frames
            self.C = C
            self.f0 = self.sr / self.period
            self.spacing_ms = self.spacing_var.get()
            self.frame_marks = [fr['start'] for fr in frames]
            self._draw_wave()

            cfg = dict(n_bars=self.bars_var.get(), G=self.gran_var.get(),
                       mode=self.obj_var.get(), allow_add=self.add_var.get(),
                       allow_density=self.den_var.get(),
                       allow_bar_density=self.bar_den_var.get(), half_f=half_f)
            self.cancel_flag.clear()
            self.btn_fit.config(state='disabled')
            self.btn_cancel.config(state='normal')
            self.lbl_status.config(text=f"{len(frames)} snapshots, "
                                        f"{n_eff} cycle(s) each")

            def progress(frac, msg):
                self.msg_q.put(('progress', frac, msg))
                return not self.cancel_flag.is_set()

            def work():
                try:
                    result = run_dynamic_fit(self.frames, C, self.f0, self.sr,
                                             cfg, progress)
                    self.msg_q.put(('done', result))
                except Exception as e:
                    import traceback
                    self.msg_q.put(('error', f"{e}\n{traceback.format_exc()}"))

            self.fit_thread = threading.Thread(target=work, daemon=True)
            self.fit_thread.start()

        def cancel_fit(self):
            self.cancel_flag.set()

        def _poll_queue(self):
            try:
                while True:
                    msg = self.msg_q.get_nowait()
                    if msg[0] == 'progress':
                        self.prog['value'] = msg[1] * 100
                        if msg[2]:
                            self.lbl_status.config(text=msg[2])
                    elif msg[0] == 'done':
                        self.btn_fit.config(state='normal')
                        self.btn_cancel.config(state='disabled')
                        if msg[1] is None:
                            self.lbl_status.config(text="cancelled")
                        else:
                            self.result = msg[1]
                            self._show_result()
                    elif msg[0] == 'error':
                        messagebox.showerror("ISHTAR Dynamic Resynth", msg[1])
                        self.btn_fit.config(state='normal')
                        self.btn_cancel.config(state='disabled')
            except queue.Empty:
                pass
            self.root.after(100, self._poll_queue)

        # ---------------- results ----------------
        def _show_result(self):
            r = self.result
            ref = r['ref']
            ob = Objective(self.frames[ref]['cycle'], self.C, self.f0, self.sr)
            ref_bars = r['ref_fit']['bars']
            y = synth_cycle(ref_bars, r['density'], cycles=ob.C)
            y_al, t_band = ob.align_for_display(y)

            for ax in (self.ax_cycle, self.ax_spec, self.ax_traj):
                ax.clear()
                ax.set_facecolor('#101018')
                ax.tick_params(colors='#8888aa', labelsize=7)

            self.ax_cycle.plot(t_band, color='#58a8ff', lw=1.2, label='target')
            self.ax_cycle.plot(y_al, color='#ffb050', lw=1.0, label='fit')
            self.ax_cycle.legend(fontsize=7, facecolor='#181820',
                                 labelcolor='#d8d8e8', edgecolor='#333')
            self.ax_cycle.set_title(f"ref snapshot #{ref + 1} cycle",
                                    color='#d8d8e8', fontsize=9)

            Yb = np.fft.rfft(y)[1:ob.K + 1]
            tm = np.abs(ob.Tb)
            fm = np.abs(Yb)
            if fm.max() > 0:
                fm = fm * (tm.max() / fm.max())
            hx = np.arange(1, ob.K + 1) / ob.C
            self.ax_spec.bar(hx - 0.12 / ob.C, tm, width=0.24 / ob.C,
                             color='#58a8ff', label='target')
            self.ax_spec.bar(hx + 0.12 / ob.C, fm, width=0.24 / ob.C,
                             color='#ffb050', label='fit')
            self.ax_spec.legend(fontsize=7, facecolor='#181820',
                                labelcolor='#d8d8e8', edgecolor='#333')
            self.ax_spec.set_title("harmonics (ref)", color='#d8d8e8', fontsize=9)

            times_ms = np.array(r['times']) * 1000.0
            t_dense = np.linspace(0, r['times'][-1], 200)
            for j, (f, route, d) in enumerate(r['layout']):
                col = BAR_COLOURS[j % len(BAR_COLOURS)]
                traj = [r['levels'][k][j] for k in range(len(times_ms))]
                self.ax_traj.plot(times_ms, traj, 'o', ms=3.5, color=col)
                curve = [bar_level(t, r['bar_env'][j]) for t in t_dense]
                self.ax_traj.plot(t_dense * 1000.0, curve, '-', lw=1.0,
                                  color=col, label=f"f={f:g} {route}")
            self.ax_traj.plot(times_ms, r['rms_norm'], 's', ms=3,
                              color='#d8d8e8')
            amp = r['amp_env']
            ac = amp['amount'] * env_value(t_dense, amp['attack'],
                                           amp['decay'], amp['sustain'])
            self.ax_traj.plot(t_dense * 1000.0, ac, '--', lw=1.0,
                              color='#d8d8e8', label='amp (norm)')
            self.ax_traj.axhline(0.0, color='#333344', lw=0.6)
            self.ax_traj.legend(fontsize=6, facecolor='#181820',
                                labelcolor='#d8d8e8', edgecolor='#333')
            self.ax_traj.set_title("level trajectories + envelopes",
                                   color='#d8d8e8', fontsize=9)
            self.ax_traj.set_xlabel("ms", color='#8888aa', fontsize=7)
            self.canvas_res.draw_idle()

            lines = [f"f0 = {self.f0:.1f} Hz   mode = "
                     f"{'fingerprint' if r['mode'] == 'spec' else 'waveform'}",
                     f"{r['n_frames']} snapshots at {self.spacing_ms} ms, "
                     f"reference = #{ref + 1} "
                     f"(richness {r['richness'][ref]} harmonics)",
                     f"Carrier Density (carrierMorph) = {r['density']:.3f}",
                     "Brilliance pinned at 0.50, envCurve at 0.50",
                     "",
                     " bar    f    route     k      amt     A      D      S",
                     " ---  -----  -----  ------  ------  -----  -----  -----"]
            for j, (f, route, d) in enumerate(r['layout']):
                p = r['bar_env'][j]
                lines.append(f"  {j + 1:2d}  {f:5.1f}   {route:3s}   "
                             f"{p['base']:+6.2f}  {p['amount']:+6.2f}  "
                             f"{p['attack']:5.3f}  {p['decay']:5.3f}  "
                             f"{p['sustain']:5.2f}")
            amp = r['amp_env']
            lines += ["",
                      f" amp env:              A {amp['attack']:5.3f}  "
                      f"D {amp['decay']:5.3f}  S {amp['sustain']:5.2f}",
                      "",
                      " snap    t(ms)   refit err   env err",
                      " ----   ------   ---------   -------"]
            for k in range(r['n_frames']):
                mark = ' *' if k == ref else '  '
                lines.append(f"  {k + 1:2d}{mark} {r['times'][k] * 1000:7.0f}"
                             f"   {r['err_refit'][k]:8.2f}%"
                             f"   {r['err_env'][k]:6.2f}%")
            lines += ["",
                      f"ref fit residual: fingerprint "
                      f"{r['ref_fit']['err_spec']:.2f}%   waveform "
                      f"{r['ref_fit']['err_wave']:.2f}%",
                      "",
                      "refit err = layout limit at that snapshot;",
                      "env err = what the exported envelopes reproduce.",
                      "Export: k + EnvelopeAmount x ADSR per bar, fitted",
                      "amp envelope, exponentialControl pinned at 0.50."]
            self.txt.delete('1.0', 'end')
            self.txt.insert('1.0', "\n".join(lines))

        # ---------------- export ----------------
        def export_patch(self):
            if self.result is None:
                messagebox.showinfo("ISHTAR Dynamic Resynth", "Run a fit first.")
                return
            init_dir = PATCH_EXPORT_DIR
            try:
                drive = os.path.splitdrive(init_dir)[0]
                drive_ok = (not drive) or os.path.isdir(drive + os.sep)
                if drive_ok:
                    os.makedirs(init_dir, exist_ok=True)
                if not (drive_ok and os.path.isdir(init_dir)):
                    init_dir = os.path.expanduser("~")
            except OSError:
                init_dir = os.path.expanduser("~")

            default = os.path.splitext(self.wav_name)[0] or "Dynamic Resynth patch"
            path = filedialog.asksaveasfilename(
                initialdir=init_dir, initialfile=default + ".md",
                defaultextension=".md", filetypes=[("ISHTAR patch", "*.md")])
            if not path:
                return
            name = os.path.splitext(os.path.basename(path))[0]
            md = make_dynamic_patch_markdown(name, self.result, self.f0,
                                             self.wav_name, self.spacing_ms)
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(md)
            self.lbl_status.config(text=f"patch saved: {os.path.basename(path)}")

        def render_audition(self):
            if self.result is None or self.frames is None:
                messagebox.showinfo("ISHTAR Dynamic Resynth", "Run a fit first.")
                return
            import soundfile as sf
            default = os.path.splitext(self.wav_name)[0] + "_dynAB.wav"
            path = filedialog.asksaveasfilename(
                initialfile=default, defaultextension=".wav",
                filetypes=[("WAV", "*.wav")])
            if not path:
                return
            start = self.frames[0]['start']
            span = self.result['times'][-1] + self.spacing_ms / 1000.0
            avail = (len(self.audio) - start) / self.sr
            # Long enough to hear the decay past the analysed span - the amp
            # envelope keeps running beyond the last snapshot.
            seconds = min(max(3.0, 2.5 * span), 8.0, avail)
            out = render_dynamic_ab(self.audio, int(self.sr), start,
                                    self.result, self.C, self.f0, seconds)
            sf.write(path, out, int(self.sr))
            self.lbl_status.config(text=f"A/B saved: source {seconds:.1f}s, "
                                        f"gap, fit {seconds:.1f}s")

    root = __import__('tkinter').Tk()
    App(root)
    root.mainloop()


if __name__ == '__main__':
    if '--selftest' in sys.argv:
        sys.exit(selftest())
    run_gui()

"""
ISHTAR Resynth - standalone resynthesizer for the ISHTAR (PLANET2026) engine.

Takes a short audio snippet, extracts a single wavecycle (auto pitch detection
or manual markers), and searches ISHTAR's static parameter space for the
combination that comes closest to reproducing it.

Engine model replicated here (see Source/PLANETVoice.cpp):

    y(theta) = carrier( theta + B * sum_pm  s_i * sin(f_i * theta) )
             +           B * sum_add clamp(s_i,-2,2)/2 * sin(f_i * theta)

    carrier(phi) = (1-m) * sin(phi) + m * softsaw(phi)     [F5 Density morph]
    softsaw = sum_{n=1..16} (0.8^(n-1)/n) sin(n*phi)

Fitting conventions (agreed with Gerard, 6 Jul 2026):
  * Brilliance B is pinned at 0.5 - it is a performance control. The fit
    therefore targets the patch's sound at Brilliance noon.
  * Levels are fitted in [-2, 2] and exported as drawbar ENVELOPE DEPTH with a
    gate envelope (A 0.001, D 0.001, S 1.0, R 0.001) and k slider at 0, so
    transient motion can later be added by simply reshaping the envelope.
  * Density m is searched (0..1); granularity G sets the number of candidate
    steps per continuous parameter, spaced evenly in normalised knob travel
    through the plugin's symmetric skew law (skew 0.5 -> value = 2*sign(d)*d^2).

Run with --selftest for a headless engine/fitter verification.
"""

import os
import sys
import queue
import threading
import numpy as np

# --------------------------------------------------------------------------
# ENGINE CONSTANTS (mirror PLANETVoice / PluginProcessor exactly)
# --------------------------------------------------------------------------
TWO_PI = 2.0 * np.pi
BRILLIANCE = 0.5          # performance control, pinned at noon for fitting
F_MIN, F_MAX = 0.5, 30.0  # input_f range (0.5 steps)
S_MIN, S_MAX = -2.0, 2.0  # fitted level range (exported as env depth)
SS_TAPER, SS_HARMONICS = 0.8, 16
LUT_SIZE = 8192

M_CYCLE = 2048            # samples per fundamental cycle in analysis domain
H_MAX = 40                # harmonics compared in fingerprint mode
SPEC_COMPRESS = 0.5       # magnitude compression exponent (sqrt ~ perceptual)

PATCH_EXPORT_DIR = r"H:\OneDrive\Documents\PLANET2026\Patches\Resynth"


def _build_softsaw_table():
    theta = TWO_PI * np.arange(LUT_SIZE) / LUT_SIZE
    v = np.zeros(LUT_SIZE)
    rp = 1.0
    for n in range(1, SS_HARMONICS + 1):
        v += (rp / n) * np.sin(n * theta)
        rp *= SS_TAPER
    return v


_SS_TABLE = _build_softsaw_table()


def softsaw(phase):
    """Vectorised soft-saw lookup, same 8192-entry linear-interp LUT as the plugin."""
    p = np.mod(phase, TWO_PI) * (LUT_SIZE / TWO_PI)
    i0 = p.astype(np.int64)
    fr = p - i0
    i0 &= (LUT_SIZE - 1)
    i1 = (i0 + 1) & (LUT_SIZE - 1)
    return _SS_TABLE[i0] * (1.0 - fr) + _SS_TABLE[i1] * fr


def carrier(phi, m):
    if m <= 0.0:
        return np.sin(phi)
    return (1.0 - m) * np.sin(phi) + m * softsaw(phi)


def synth_cycle(bars, m, cycles=1, M=M_CYCLE):
    """Render one analysis window (C fundamental cycles, C*M samples).
    bars: list of (f, s, route) with route 'PM' or 'ADD'."""
    theta = TWO_PI * np.arange(cycles * M) / M
    phi = theta.copy()
    add = np.zeros_like(theta)
    for f, s, route in bars:
        w = np.sin(f * theta)
        if route == 'PM':
            phi += BRILLIANCE * s * w
        else:  # ADD: engine clamps stagedCoeff to +/-2, then *0.5, then *B
            add += BRILLIANCE * 0.5 * np.clip(s, -2.0, 2.0) * w
    return carrier(phi, m) + add


# --------------------------------------------------------------------------
# GRANULARITY GRIDS (match the plugin's knob law)
# --------------------------------------------------------------------------
def level_grid(G):
    """G candidate levels, evenly spaced in normalised knob travel, mapped
    through JUCE symmetric skew 0.5 over [-2,2]: value = 2*sign(d)*d^2."""
    t = np.linspace(0.0, 1.0, G)
    d = 2.0 * t - 1.0
    s = 2.0 * np.sign(d) * d * d
    return s[np.abs(s) > 1e-9]   # zero = 'bar off', not a candidate


def density_grid(G):
    return np.linspace(0.0, 1.0, G)


# --------------------------------------------------------------------------
# TARGET PREPARATION
# --------------------------------------------------------------------------
def detect_period(x, sr, fmin=25.0, fmax=2500.0):
    """Autocorrelation pitch detector with octave-error guard and parabolic
    refinement. Returns fractional period in samples, or None."""
    x = np.asarray(x, dtype=np.float64)
    x = x - x.mean()
    n = len(x)
    if n < 64 or np.max(np.abs(x)) < 1e-6:
        return None
    nfft = 1 << int(np.ceil(np.log2(2 * n)))
    X = np.fft.rfft(x, nfft)
    ac = np.fft.irfft(X * np.conj(X))[:n]
    denom = np.maximum(n - np.arange(n), 1)
    r = ac / denom                       # unbiased autocorrelation
    r = r / (r[0] + 1e-30)
    lag_min = max(2, int(sr / fmax))
    lag_max = min(n // 2, int(sr / fmin))
    if lag_max <= lag_min + 2:
        return None
    seg = r[lag_min:lag_max]
    tau = int(np.argmax(seg)) + lag_min
    peak = r[tau]
    if peak < 0.2:
        return None
    # Octave-error guard: prefer the smallest sub-lag whose local peak is
    # nearly as strong (harmonically rich sounds peak at 2P, 3P too).
    for k in (4, 3, 2):
        cand = tau / k
        if cand < lag_min:
            continue
        c0 = int(round(cand))
        lo, hi = max(lag_min, c0 - 3), min(lag_max, c0 + 4)
        if hi <= lo:
            continue
        local = int(np.argmax(r[lo:hi])) + lo
        if r[local] > 0.85 * peak:
            tau = local
            break
    # Parabolic refinement
    if 1 <= tau < n - 1:
        a, b, c = r[tau - 1], r[tau], r[tau + 1]
        den = a - 2 * b + c
        if abs(den) > 1e-12:
            tau = tau + 0.5 * (a - c) / den
    return float(tau)


def extract_target(x, start, period, n_cycles, C, M=M_CYCLE):
    """Extract and average n_cycles windows of C*period samples from `start`,
    each resampled to C*M points. Cycles are circularly aligned to the first
    before averaging (protects against slight period drift / vibrato).
    Returns (cycle, n_used)."""
    x = np.asarray(x, dtype=np.float64)
    win = C * period
    CM = C * M
    grid = np.arange(CM) * (win / CM)
    acc, ref_spec, used = None, None, 0
    for i in range(n_cycles):
        s0 = start + i * win
        if s0 + win + 1 >= len(x):
            break
        c = np.interp(s0 + grid, np.arange(len(x)), x)
        c = c - c.mean()
        if acc is None:
            acc = c.copy()
            ref_spec = np.fft.rfft(c)
        else:
            corr = np.fft.irfft(np.conj(np.fft.rfft(c)) * ref_spec)
            c = np.roll(c, int(np.argmax(corr)))
            acc += c
        used += 1
    if acc is None or used == 0:
        return None, 0
    cyc = acc / used
    cyc -= cyc.mean()
    norm = np.linalg.norm(cyc)
    if norm < 1e-12:
        return None, 0
    return cyc / norm, used


# --------------------------------------------------------------------------
# OBJECTIVES
# --------------------------------------------------------------------------
class Objective:
    """Holds the prepared target spectra and scores candidate spectra batches.
    All comparison happens on rfft bins 1..K of the C*M window (DC excluded,
    band-limited to the target's Nyquist)."""

    def __init__(self, target_cycle, C, f0, sr):
        self.C = C
        self.CM = len(target_cycle)
        h_nyq = int(np.floor((sr / 2.0) / max(f0, 1.0)))
        self.H = int(max(4, min(H_MAX, h_nyq)))
        self.K = self.H * C                       # compared bins
        T = np.fft.rfft(target_cycle)
        self.Tb = T[1:self.K + 1].copy()
        # normalise band energy to 1 for the waveform metric
        e = np.sqrt(2.0 * np.sum(np.abs(self.Tb) ** 2) / self.CM)
        self.Tb /= max(e, 1e-30)
        # fingerprint vector: compressed magnitudes, unit norm
        a = np.abs(self.Tb) ** SPEC_COMPRESS
        self.Ta = a / max(np.linalg.norm(a), 1e-30)
        # band-limited target waveform (for display / audition)
        Tfull = np.zeros(self.CM // 2 + 1, dtype=complex)
        Tfull[1:self.K + 1] = self.Tb
        self.t_band = np.fft.irfft(Tfull, n=self.CM)

    def band_spec(self, Y):
        """Y: batch rfft (B x CM/2+1) -> band bins (B x K)."""
        return Y[..., 1:self.K + 1]

    def err_fingerprint(self, Yb):
        a = np.abs(Yb) ** SPEC_COMPRESS
        n = np.linalg.norm(a, axis=-1, keepdims=True)
        a = a / np.maximum(n, 1e-30)
        return np.linalg.norm(a - self.Ta, axis=-1) * 100.0 / np.sqrt(2.0)

    def err_waveform(self, Yb):
        """Best-case residual over circular shift and (positive) gain."""
        Yb = np.atleast_2d(Yb)
        B = Yb.shape[0]
        X = np.zeros((B, self.CM // 2 + 1), dtype=complex)
        X[:, 1:self.K + 1] = np.conj(Yb) * self.Tb[None, :]
        corr = np.fft.irfft(X, n=self.CM, axis=-1)
        cmax = np.max(corr, axis=-1)
        ny2 = 2.0 * np.sum(np.abs(Yb) ** 2, axis=-1) / self.CM
        frac = np.clip(1.0 - (np.maximum(cmax, 0.0) ** 2) / np.maximum(ny2, 1e-30), 0.0, 1.0)
        return np.sqrt(frac) * 100.0

    def err(self, Yb, mode):
        return self.err_fingerprint(Yb) if mode == 'spec' else self.err_waveform(Yb)

    def align_for_display(self, y):
        """Return (shifted, gain-matched y, band-limited target) for overlay."""
        Y = np.fft.rfft(y)
        Yb = Y[1:self.K + 1]
        X = np.zeros(self.CM // 2 + 1, dtype=complex)
        X[1:self.K + 1] = np.conj(Yb) * self.Tb
        corr = np.fft.irfft(X, n=self.CM)
        shift = int(np.argmax(corr))
        ny2 = 2.0 * np.sum(np.abs(Yb) ** 2) / self.CM
        gain = max(corr[shift], 0.0) / max(ny2, 1e-30)
        y_al = np.roll(y - y.mean(), shift) * gain
        return y_al, self.t_band


# --------------------------------------------------------------------------
# FITTER (beam-search matching pursuit + on-grid coordinate descent)
# --------------------------------------------------------------------------
class Fitter:
    BEAM_W = 6        # states kept alive per pursuit stage
    N_SWEEPS = 3      # coordinate-descent sweeps after pursuit
    DESCENT_TOP = 3   # candidates per bar tried (each with Density re-swept)
    DESCENT_SEEDS = 3  # beam states descended independently (best kept)
    BASIN_HOPS = 10   # random f/route perturbations of the best fit, re-descended

    def __init__(self, objective, cfg, progress=None):
        """cfg keys: n_bars, G, mode ('spec'|'wave'), allow_add, allow_density,
        half_f. progress: callable(frac, msg) -> bool (False = cancel)."""
        self.ob = objective
        self.cfg = cfg
        self.progress = progress or (lambda frac, msg: True)
        C = objective.C
        self.C = C
        self.CM = objective.CM
        M = self.CM // C
        self.theta = TWO_PI * np.arange(self.CM) / M
        step = 0.5 if cfg['half_f'] else 1.0
        fmax = min(F_MAX, float(objective.H))
        self.f_list = np.arange(step, fmax + 1e-9, step)
        self.SINF = np.sin(self.f_list[:, None] * self.theta[None, :])
        self.FSPEC = np.fft.rfft(self.SINF, axis=-1)[:, 1:objective.K + 1]
        self.s_grid = level_grid(cfg['G'])
        self.m_grid = density_grid(cfg['G']) if cfg['allow_density'] else np.array([0.0])
        self.err_curve = []     # (n_bars, err_spec, err_wave)
        self.cancelled = False
        self.active_mode = cfg['mode']
        self._done = 0
        self._total = 1

    # ---- state helpers (bars = list of (f_idx, s, route)) -------------------
    def _sums(self, bars, exclude=None):
        pm = np.zeros(self.CM)
        add = np.zeros(self.CM)
        for j, (fi, s, route) in enumerate(bars):
            if j == exclude:
                continue
            if route == 'PM':
                pm += BRILLIANCE * s * self.SINF[fi]
            else:
                add += BRILLIANCE * 0.5 * np.clip(s, -2, 2) * self.SINF[fi]
        return pm, add

    def _spectrum(self, pm, add, m):
        phi = self.theta + pm
        y = carrier(phi, m) + add
        return np.fft.rfft(y)[1:self.ob.K + 1]

    def _errs_both(self, bars, m):
        pm, add = self._sums(bars)
        Yb = self._spectrum(pm, add, m)[None, :]
        return (float(self.ob.err_fingerprint(Yb)[0]),
                float(self.ob.err_waveform(Yb)[0]))

    def _err_mode(self, bars, m):
        pm, add = self._sums(bars)
        Yb = self._spectrum(pm, add, m)[None, :]
        return float(self.ob.err(Yb, self.active_mode)[0])

    def _tick(self, msg=''):
        self._done += 1
        if not self.progress(min(self._done / self._total, 1.0), msg):
            self.cancelled = True

    # ---- candidate scan ----------------------------------------------------
    def _scan(self, bars, m, top_j, exclude=None, s_grid=None):
        """Try every (f, s, route) against `bars` (minus `exclude`) at density m.
        Returns up to top_j candidates [(err, fi, s, route)], best s per (f,route),
        distinct (f,route) between entries."""
        mode = self.active_mode
        grid = self.s_grid if s_grid is None else s_grid
        pm, add = self._sums(bars, exclude=exclude)
        used = {(fi, r) for j, (fi, _, r) in enumerate(bars) if j != exclude}
        nf = len(self.f_list)
        routes = ['PM'] + (['ADD'] if self.cfg['allow_add'] else [])
        found = []   # (err, fi, s, route) - one per (f,route)
        phi_base = self.theta + pm

        for route in routes:
            if self.cancelled:
                break
            if route == 'ADD':
                # Purely spectral: Y(s) = Ycar + B*0.5*s*Fspec(f)
                Ycar = self._spectrum(pm, add, m)
                for fi in range(nf):
                    if (fi, 'ADD') in used:
                        continue
                    Yb = Ycar[None, :] + (BRILLIANCE * 0.5 * grid)[:, None] \
                        * self.FSPEC[fi][None, :]
                    errs = self.ob.err(Yb, mode)
                    j = int(np.argmin(errs))
                    found.append((float(errs[j]), fi, float(grid[j]), 'ADD'))
            else:
                for fi in range(nf):
                    if (fi, 'PM') in used:
                        continue
                    phi = phi_base[None, :] + \
                        (BRILLIANCE * grid)[:, None] * self.SINF[fi][None, :]
                    y = carrier(phi, m) + add[None, :]
                    Yb = np.fft.rfft(y, axis=-1)[:, 1:self.ob.K + 1]
                    errs = self.ob.err(Yb, mode)
                    j = int(np.argmin(errs))
                    found.append((float(errs[j]), fi, float(grid[j]), 'PM'))
        found.sort(key=lambda c: c[0])
        return found[:top_j]

    def _sweep_density(self, bars, m):
        """1D grid sweep of Density. Nearly free: Y(m) = (1-m)S1 + m*S2 + A."""
        if len(self.m_grid) <= 1:
            return m
        pm, add = self._sums(bars)
        phi = self.theta + pm
        S1 = np.fft.rfft(np.sin(phi))[1:self.ob.K + 1]
        S2 = np.fft.rfft(softsaw(phi))[1:self.ob.K + 1]
        A = np.fft.rfft(add)[1:self.ob.K + 1]
        mg = self.m_grid[:, None]
        Yb = (1.0 - mg) * S1[None, :] + mg * S2[None, :] + A[None, :]
        errs = self.ob.err(Yb, self.active_mode)
        return float(self.m_grid[int(np.argmin(errs))])

    # ---- main --------------------------------------------------------------
    def run(self):
        n_bars = self.cfg['n_bars']
        W = self.BEAM_W
        n_mstarts = 4 if len(self.m_grid) > 1 else 1
        pursuit_scans = n_mstarts + max(0, n_bars - 1) * W
        # Cross-mode seeding: the waveform objective sees harmonic PHASES, so
        # its landscape is much sharper than the phase-blind fingerprint. When
        # fitting by fingerprint, a waveform-mode pursuit is run as well and
        # its winner descended under the fingerprint metric - on reachable
        # targets it usually lands in the true basin.
        # A fine-grid WAVE-mode scout runs a COMPLETE fit alongside the user's
        # search: the waveform objective sees harmonic phases (sharp landscape)
        # and a fine level grid sees basins a coarse one ranks wrongly. Its
        # result is snapped to the user's granularity before joining the seed
        # pool, so the user's G still defines the solution vocabulary.
        scout = (not self.cfg.get('_scout')) \
            and not (self.cfg['mode'] == 'wave' and self.cfg['G'] >= 120)
        n_seeds = self.DESCENT_SEEDS + (1 if scout else 0)
        self._total = pursuit_scans * (2 if scout else 1) \
            + n_seeds * self.N_SWEEPS * max(n_bars, 1)
        self._done = 0

        beam = self._pursuit(self.cfg['mode'], n_bars, record_curve=True)
        seeds = beam[:self.DESCENT_SEEDS]
        if scout and not self.cancelled:
            scfg = dict(self.cfg, mode='wave', G=max(self.cfg['G'], 120),
                        _scout=True)
            fr0 = self._done / self._total
            fr1 = min(1.0, (self._done + pursuit_scans) / self._total)

            def sub_progress(fr, msg):
                ok = self.progress(fr0 + (fr1 - fr0) * fr,
                                   f"scout: {msg}" if msg else '')
                return ok

            sub = Fitter(self.ob, scfg, sub_progress)
            sres = sub.run()
            self._done += pursuit_scans
            if sub.cancelled:
                self.cancelled = True
            elif sres['bars']:
                sbars = [(int(np.argmin(np.abs(self.f_list - f))), s, r)
                         for f, s, r in sres['bars']]
                sbars, sm = self._snap(sbars, sres['density'])
                seeds = seeds + [(sbars, sm, 0.0)]

        self.active_mode = self.cfg['mode']
        results = []
        for si, (bars, m, _) in enumerate(seeds):
            if self.cancelled:
                break
            bars, m = self._descend(list(bars), m, f"seed {si + 1}/{len(seeds)}")
            results.append((self._err_mode(bars, m), bars, m))
        results.sort(key=lambda r: r[0])
        best_err, bars, m = results[0] if results else \
            (self._err_mode(list(beam[0][0]), beam[0][1]), list(beam[0][0]), beam[0][1])

        # ---- basin hopping: perturb f/route of the best fit, re-descend ----
        # Greedy pursuit can't see how PM bars will interact once combined;
        # random reassignment jumps escape basins no local move can leave.
        rng = np.random.default_rng(0x15147A2)  # fixed seed - reproducible fits
        self._total += self.BASIN_HOPS * 2 * max(len(bars), 1)
        for h in range(self.BASIN_HOPS):
            if self.cancelled or not bars:
                break
            trial = self._perturb(bars, rng)
            tm = self._sweep_density(trial, m)
            trial, tm = self._descend(trial, tm, f"hop {h + 1}/{self.BASIN_HOPS}",
                                      sweeps=2)
            te = self._err_mode(trial, tm)
            if te < best_err - 1e-9:
                best_err, bars, m = te, trial, tm

        es, ew = self._errs_both(bars, m)
        if self.err_curve:
            self.err_curve[-1] = (self.err_curve[-1][0], es, ew)
        self.progress(1.0, "Done.")
        return self._finish(bars, m, es, ew)

    def _pursuit(self, mode, n_bars, record_curve, s_grid=None):
        """Beam-search matching pursuit under `mode`. Stage 1 multi-starts
        across a coarse Density spread: candidates that only work with the
        soft-saw carrier engaged must get a beam lane too, otherwise a
        sine-carrier start locks the whole search into one basin."""
        self.active_mode = mode
        W = self.BEAM_W
        if len(self.m_grid) > 1:
            beam = [([], m0, float('inf')) for m0 in (0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0)]
        else:
            beam = [([], 0.0, float('inf'))]   # (bars, m, err)
        for b in range(n_bars):
            if self.cancelled:
                break
            candidates = []
            for bars, m, _ in beam:
                if self.cancelled:
                    break
                self.progress(self._done / self._total,
                              f"Drawbar {b + 1} of {n_bars} "
                              f"({'wave-seed, ' if not record_curve else ''}"
                              f"exploring {len(beam)} path{'s' if len(beam) > 1 else ''})...")
                tops = self._scan(bars, m, top_j=W, s_grid=s_grid)
                self._tick()
                for err, fi, s, route in tops:
                    nb = bars + [(fi, s, route)]
                    nm = self._sweep_density(nb, m)
                    candidates.append((nb, nm, self._err_mode(nb, nm)))
            if not candidates:
                break
            # dedup on the bar set, keep the W best states
            candidates.sort(key=lambda st: st[2])
            beam, seen = [], set()
            for nb, nm, e in candidates:
                key = frozenset((fi, round(s, 6), r) for fi, s, r in nb)
                if key in seen:
                    continue
                seen.add(key)
                beam.append((nb, nm, e))
                if len(beam) >= W:
                    break
            if record_curve:
                es, ew = self._errs_both(beam[0][0], beam[0][1])
                self.err_curve.append((b + 1, es, ew))
        return beam

    def _perturb(self, bars, rng):
        """Random jump for basin hopping: move 1-2 bars' f by up to +/-3 steps,
        sometimes flipping the route, keeping (f, route) pairs unique."""
        nf = len(self.f_list)
        trial = list(bars)
        k = 1 + int(rng.integers(0, 2)) if len(bars) > 1 else 1
        for j in rng.choice(len(trial), size=min(k, len(trial)), replace=False):
            fi, s, route = trial[j]
            others = {(f, r) for i, (f, _, r) in enumerate(trial) if i != j}
            for _ in range(12):
                nfi = int(np.clip(fi + int(rng.integers(-3, 4)), 0, nf - 1))
                nroute = route
                if self.cfg['allow_add'] and rng.random() < 0.3:
                    nroute = 'ADD' if route == 'PM' else 'PM'
                if (nfi, nroute) not in others and (nfi, nroute) != (fi, route):
                    trial[j] = (nfi, s, nroute)
                    break
        return trial

    def _snap(self, bars, m):
        """Snap levels and Density to the user's granularity grids."""
        sg = self.s_grid
        bars = [(fi, float(sg[int(np.argmin(np.abs(sg - s)))]), r)
                for fi, s, r in bars]
        mg = self.m_grid
        m = float(mg[int(np.argmin(np.abs(mg - m)))])
        return bars, m

    def _descend(self, bars, m, label, sweeps=None, s_grid=None):
        """On-grid coordinate descent: re-optimise each bar with the others
        held, judging every candidate together with its re-swept Density."""
        for sweep in range(sweeps if sweeps is not None else self.N_SWEEPS):
            if self.cancelled:
                break
            improved = False
            for j in range(len(bars)):
                if self.cancelled:
                    break
                self.progress(self._done / self._total,
                              f"Refining {label} "
                              f"(sweep {sweep + 1}, drawbar {j + 1})...")
                cur = self._err_mode(bars, m)
                tops = self._scan(bars, m, top_j=self.DESCENT_TOP, exclude=j,
                                  s_grid=s_grid)
                self._tick()
                best = (cur, None, m)
                for _, fi, s, route in tops:
                    trial = list(bars)
                    trial[j] = (fi, s, route)
                    tm = self._sweep_density(trial, m)
                    te = self._err_mode(trial, tm)
                    if te < best[0] - 1e-9:
                        best = (te, (fi, s, route), tm)
                if best[1] is not None:
                    bars[j] = best[1]
                    m = best[2]
                    improved = True
                else:
                    m = self._sweep_density(bars, m)
            if not improved:
                break
        return bars, m

    def _finish(self, bars, m, es, ew):
        out_bars = [(float(self.f_list[fi]), s, route) for fi, s, route in bars]
        out_bars.sort(key=lambda b: b[0])
        return {
            'bars': out_bars, 'density': m,
            'err_spec': es, 'err_wave': ew,
            'err_curve': self.err_curve,
            'mode': self.cfg['mode'], 'G': self.cfg['G'],
        }


# --------------------------------------------------------------------------
# PATCH EXPORT
# --------------------------------------------------------------------------
def make_patch_markdown(name, result, f0, source_name):
    bars = result['bars']
    lines = []
    ap = lines.append
    ap(f"# {name}")
    ap("")
    mode_name = "harmonic fingerprint" if result['mode'] == 'spec' else "literal waveform"
    ap(f"Resynthesized from {source_name} (f0 ~ {f0:.1f} Hz) by ISHTAR Resynth. "
       f"Fit mode: {mode_name}, {len(bars)} drawbars, granularity {result['G']}. "
       f"Residual: fingerprint {result['err_spec']:.1f}%, waveform {result['err_wave']:.1f}%. "
       f"Levels live in envelope depth (gate envelope) so transients can be shaped; "
       f"Brilliance is at noon by design. All parameters not listed load as defaults.")
    ap("")
    ap("Tags: resynth")
    ap("")
    ap("---")
    ap("")
    ap("## Colour")
    ap("brilliance = 0.50  (0.000 to 1.0)")
    ap(f"carrierMorph = {result['density']:.3f}  (0.000 to 1.0)")
    ap("")
    ap("---")
    ap("")
    ap("## Drawbar Parameters Reference")
    ap("# k = -2.0 to 2.0")
    ap("# EnvelopeAmount = -5.0 to 20.0 (resynth uses -2 to 2)")
    ap("# input_f = 0.5 to 30.0 (0.5 steps)")
    ap("# ToPM = 1 -> phase-distortion path, ToOut = 1 -> additive partial")
    ap("")

    # Fitted bars occupy drawbars 1..n in ascending f; the rest stay default.
    for i in range(1, 11):
        p = f"k{i}"
        ap(f"## Drawbar {i}")
        if i <= len(bars):
            f, s, route = bars[i - 1]
            to_pm, to_out = (1, 0) if route == 'PM' else (0, 1)
            ap(f"{p} = 0.00  (-2.000 to 2.0)")
            ap(f"{p}EnvelopeAmount = {s:.4f}  (-5.000 to 20.0)")
            ap(f"{p}LFOAmount = 0.00  (-5.000 to 5.0)")
            ap(f"{p}VelToHarmonic = 0.00  (-100.000 to 100.0)")
            ap(f"input_f{i} = {f:.2f}  (0.500 to 30.0)")
            ap(f"{p}AttackTime = 0.001  (0.001 to 10.0)")
            ap(f"{p}DecayTime = 0.001  (0.001 to 10.0)")
            ap(f"{p}SustainLevel = 1.00  (0.000 to 2.0)")
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
# AUDITION RENDER
# --------------------------------------------------------------------------
def render_ab(target_cycle, fit_bars, m, C, f0, sr=44100, seconds=2.0, gap=0.4):
    """Target loop, silence, fitted patch - one WAV for A/B by ear."""
    fit_cycle = synth_cycle(fit_bars, m, cycles=C)

    def tile(cycle, secs):
        CM = len(cycle)
        n = int(secs * sr)
        # phase in cycle-domain samples: CM samples span C periods of f0
        pos = np.arange(n) * (f0 * CM / (C * sr))
        pos = np.mod(pos, CM)
        i0 = pos.astype(np.int64)
        fr = pos - i0
        i1 = (i0 + 1) % CM
        y = cycle[i0] * (1.0 - fr) + cycle[i1] * fr
        fade = int(0.01 * sr)
        env = np.ones(n)
        env[:fade] = np.linspace(0, 1, fade)
        env[-fade:] = np.linspace(1, 0, fade)
        rms = np.sqrt(np.mean(y ** 2)) + 1e-12
        return y * env * (0.2 / rms)

    a = tile(target_cycle, seconds)
    b = tile(fit_cycle, seconds)
    sil = np.zeros(int(gap * sr))
    out = np.concatenate([a, sil, b])
    peak = np.max(np.abs(out)) + 1e-12
    if peak > 0.98:
        out *= 0.98 / peak
    return out.astype(np.float32)


# --------------------------------------------------------------------------
# SELF-TEST (headless)
# --------------------------------------------------------------------------
def selftest():
    print("ISHTAR Resynth self-test")
    print("=" * 60)
    rng_cases = [
        # (description, bars [(f, s, route)], density)
        ("2 PM bars, sine carrier",
         [(2.0, 0.9, 'PM'), (5.0, -0.45, 'PM')], 0.0),
        ("PM + additive mix",
         [(1.0, 0.7, 'PM'), (3.0, 0.5, 'PM'), (7.0, 0.8, 'ADD')], 0.0),
        ("Density involved",
         [(2.0, 0.6, 'PM'), (9.0, 0.35, 'ADD')], 0.4),
    ]
    ok = True
    for desc, bars, m in rng_cases:
        target = synth_cycle(bars, m, cycles=1)
        target = target - target.mean()
        target /= np.linalg.norm(target)
        f0, sr = 220.0, 44100.0
        ob = Objective(target, C=1, f0=f0, sr=sr)
        for mode in ('spec', 'wave'):
            cfg = dict(n_bars=len(bars), G=120, mode=mode,
                       allow_add=True, allow_density=(m > 0), half_f=False)
            fit = Fitter(ob, cfg).run()
            print(f"\n[{desc}] mode={mode}")
            print(f"  truth : {[(f, round(s, 3), r) for f, s, r in bars]} m={m}")
            print(f"  fitted: {[(f, round(s, 3), r) for f, s, r in fit['bars']]} "
                  f"m={fit['density']:.3f}")
            print(f"  residual: fingerprint {fit['err_spec']:.2f}%  "
                  f"waveform {fit['err_wave']:.2f}%")
            key = 'err_spec' if mode == 'spec' else 'err_wave'
            if fit[key] > 5.0:
                ok = False
                print("  ** FAIL: residual above 5% on a reachable target")
    # Error-vs-N sanity: a rich target, watch the curve fall
    print("\nError-vs-N curve on a 5-bar target (fingerprint mode):")
    bars = [(1.0, 0.8, 'PM'), (2.0, -0.5, 'PM'), (4.0, 0.4, 'PM'),
            (6.0, 0.55, 'ADD'), (11.0, 0.3, 'ADD')]
    target = synth_cycle(bars, 0.2, cycles=1)
    target -= target.mean()
    target /= np.linalg.norm(target)
    ob = Objective(target, C=1, f0=220.0, sr=44100.0)
    cfg = dict(n_bars=5, G=120, mode='spec', allow_add=True,
               allow_density=True, half_f=False)
    fit = Fitter(ob, cfg).run()
    for n, es, ew in fit['err_curve']:
        print(f"  {n} bar(s): fingerprint {es:6.2f}%   waveform {ew:6.2f}%")
    print("\n" + ("SELF-TEST PASSED" if ok else "SELF-TEST FAILED"))
    return 0 if ok else 1


# --------------------------------------------------------------------------
# GUI
# --------------------------------------------------------------------------
def run_gui():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox, simpledialog
    import soundfile as sf
    import matplotlib
    matplotlib.use("TkAgg")
    from matplotlib.figure import Figure
    from matplotlib.backends.backend_tkagg import (FigureCanvasTkAgg,
                                                   NavigationToolbar2Tk)

    class App:
        def __init__(self, root):
            self.root = root
            root.title("ISHTAR Resynth")
            root.geometry("1180x860")

            self.audio = None
            self.sr = None
            self.wav_name = ""
            self.period = None      # fractional samples
            self.markers = [0.0, 0.0]
            self.drag_idx = None
            self.result = None
            self.target_cycle = None
            self.objective = None
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
            ctl = ttk.LabelFrame(self.root, text="Fit")
            ctl.pack(fill='x', padx=6, pady=4)
            ttk.Label(ctl, text="Drawbars:").grid(row=0, column=0, padx=4)
            self.bars_var = tk.IntVar(value=5)
            ttk.Spinbox(ctl, from_=1, to=10, width=4,
                        textvariable=self.bars_var).grid(row=0, column=1)
            ttk.Label(ctl, text="Granularity:").grid(row=0, column=2, padx=(12, 2))
            self.gran_var = tk.IntVar(value=60)
            gran = ttk.Scale(ctl, from_=20, to=200, orient='horizontal', length=160,
                             command=lambda v: self.gran_var.set(int(float(v))))
            gran.set(60)
            gran.grid(row=0, column=3)
            self.lbl_gran = ttk.Label(ctl, text="60")
            self.lbl_gran.grid(row=0, column=4, padx=2)
            self.gran_var.trace_add('write',
                                    lambda *a: self.lbl_gran.config(text=str(self.gran_var.get())))

            ttk.Label(ctl, text="Cycles to average:").grid(row=0, column=5, padx=(12, 2))
            self.avg_var = tk.IntVar(value=8)
            ttk.Spinbox(ctl, from_=1, to=64, width=4,
                        textvariable=self.avg_var).grid(row=0, column=6)

            self.obj_var = tk.StringVar(value='spec')
            ttk.Radiobutton(ctl, text="Harmonic fingerprint", variable=self.obj_var,
                            value='spec').grid(row=1, column=0, columnspan=2, sticky='w', padx=4)
            ttk.Radiobutton(ctl, text="Literal waveform", variable=self.obj_var,
                            value='wave').grid(row=1, column=2, sticky='w')
            self.add_var = tk.BooleanVar(value=True)
            self.den_var = tk.BooleanVar(value=True)
            self.half_var = tk.BooleanVar(value=False)
            ttk.Checkbutton(ctl, text="Allow additive routing (F8)",
                            variable=self.add_var).grid(row=1, column=3, sticky='w')
            ttk.Checkbutton(ctl, text="Allow Density",
                            variable=self.den_var).grid(row=1, column=4, sticky='w')
            ttk.Checkbutton(ctl, text="Half-integer f (needs auto mode)",
                            variable=self.half_var).grid(row=1, column=5, columnspan=2, sticky='w')

            self.btn_fit = ttk.Button(ctl, text="FIT", command=self.start_fit)
            self.btn_fit.grid(row=0, column=7, rowspan=2, padx=12, ipadx=14, ipady=4)
            self.btn_cancel = ttk.Button(ctl, text="Cancel", command=self.cancel_fit,
                                         state='disabled')
            self.btn_cancel.grid(row=0, column=8, rowspan=2, padx=2)
            self.prog = ttk.Progressbar(ctl, length=220, mode='determinate')
            self.prog.grid(row=0, column=9, rowspan=2, padx=6)
            self.lbl_status = ttk.Label(ctl, text="")
            self.lbl_status.grid(row=0, column=10, rowspan=2, padx=4)

            # results
            res = ttk.Frame(self.root)
            res.pack(fill='both', expand=True, padx=6, pady=4)
            self.fig_res = Figure(figsize=(7.4, 3.4), dpi=90)
            self.ax_cycle = self.fig_res.add_subplot(121)
            self.ax_spec = self.fig_res.add_subplot(122)
            self.fig_res.patch.set_facecolor('#181820')
            for ax in (self.ax_cycle, self.ax_spec):
                ax.set_facecolor('#101018')
            self.canvas_res = FigureCanvasTkAgg(self.fig_res, master=res)
            self.canvas_res.get_tk_widget().pack(side='left', fill='both', expand=True)

            right = ttk.Frame(res)
            right.pack(side='left', fill='both', padx=6)
            self.txt = tk.Text(right, width=52, height=18, font=("Consolas", 9),
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
                messagebox.showerror("ISHTAR Resynth", f"Could not read file:\n{e}")
                return
            self.audio = data.mean(axis=1).astype(np.float64)
            self.sr = sr
            self.wav_name = os.path.basename(path)
            self.lbl_file.config(text=f"{self.wav_name}  ({sr} Hz, "
                                      f"{len(self.audio) / sr:.2f} s)")
            n = len(self.audio)
            self.markers = [n * 0.35, n * 0.65]
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
                for i, c in ((0, '#50e050'), (1, '#ff6060')):
                    self.ax_wave.axvline(self.markers[i], color=c, lw=1.4, alpha=0.9)
                self.ax_wave.set_xlim(0, n)
            self.ax_wave.set_yticks([])
            self.ax_wave.tick_params(colors='#8888aa', labelsize=7)
            self.canvas_wave.draw_idle()

        def _on_press(self, ev):
            if ev.inaxes != self.ax_wave or self.audio is None:
                return
            if self.canvas_wave.toolbar.mode:      # zoom/pan active
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
                messagebox.showinfo("ISHTAR Resynth", "Open a WAV first.")
                return
            if self.fit_thread and self.fit_thread.is_alive():
                return
            manual = (self.mode_var.get() == 'manual')
            a, b = sorted(self.markers)
            if manual:
                period = b - a
                if period < 8:
                    messagebox.showinfo("ISHTAR Resynth",
                                        "Manual mode: markers must span one cycle.")
                    return
                self.period = period
                self.lbl_pitch.config(text=f"f0: {self.sr / period:.1f} Hz (manual)")
            else:
                self.detect()
            if self.period is None:
                messagebox.showinfo("ISHTAR Resynth",
                                    "No pitch found - set markers manually.")
                return

            half_f = self.half_var.get()
            if half_f and self.avg_var.get() < 2 and not manual:
                pass  # allowed; extraction handles it
            if half_f and manual:
                messagebox.showinfo("ISHTAR Resynth",
                                    "Half-integer f needs auto mode (2-cycle analysis). "
                                    "Fitting with integer f instead.")
                half_f = False
            C = 2 if half_f else 1

            cycle, used = extract_target(self.audio, a, self.period,
                                         max(1, self.avg_var.get()), C)
            if cycle is None:
                messagebox.showinfo("ISHTAR Resynth",
                                    "Could not extract cycles - check markers/region.")
                return
            f0 = self.sr / self.period
            self.target_cycle = cycle
            self.C = C
            self.f0 = f0
            self.objective = Objective(cycle, C, f0, self.sr)

            cfg = dict(n_bars=self.bars_var.get(), G=self.gran_var.get(),
                       mode=self.obj_var.get(), allow_add=self.add_var.get(),
                       allow_density=self.den_var.get(), half_f=half_f)
            self.cancel_flag.clear()
            self.btn_fit.config(state='disabled')
            self.btn_cancel.config(state='normal')
            self.lbl_status.config(text=f"target: {used} cycle(s) averaged")

            def progress(frac, msg):
                self.msg_q.put(('progress', frac, msg))
                return not self.cancel_flag.is_set()

            def work():
                try:
                    fitter = Fitter(self.objective, cfg, progress)
                    result = fitter.run()
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
                        self.result = msg[1]
                        self._show_result()
                        self.btn_fit.config(state='normal')
                        self.btn_cancel.config(state='disabled')
                    elif msg[0] == 'error':
                        messagebox.showerror("ISHTAR Resynth", msg[1])
                        self.btn_fit.config(state='normal')
                        self.btn_cancel.config(state='disabled')
            except queue.Empty:
                pass
            self.root.after(100, self._poll_queue)

        # ---------------- results ----------------
        def _show_result(self):
            r = self.result
            ob = self.objective
            y = synth_cycle(r['bars'], r['density'], cycles=ob.C)
            y_al, t_band = ob.align_for_display(y)

            self.ax_cycle.clear()
            self.ax_spec.clear()
            for ax in (self.ax_cycle, self.ax_spec):
                ax.set_facecolor('#101018')
                ax.tick_params(colors='#8888aa', labelsize=7)
            self.ax_cycle.plot(t_band, color='#58a8ff', lw=1.2, label='target')
            self.ax_cycle.plot(y_al, color='#ffb050', lw=1.0, label='fit')
            self.ax_cycle.legend(fontsize=7, facecolor='#181820',
                                 labelcolor='#d8d8e8', edgecolor='#333')
            self.ax_cycle.set_title("cycle (aligned)", color='#d8d8e8', fontsize=9)

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
            self.ax_spec.set_title("harmonic magnitudes", color='#d8d8e8', fontsize=9)
            self.canvas_res.draw_idle()

            lines = [f"f0 = {self.f0:.1f} Hz   mode = "
                     f"{'fingerprint' if r['mode'] == 'spec' else 'waveform'}",
                     f"Density (carrierMorph) = {r['density']:.3f}",
                     f"Brilliance pinned at 0.50 (performance control)",
                     "",
                     " bar    f      level    route",
                     " ---  -----   -------   -----"]
            for i, (f, s, route) in enumerate(r['bars']):
                lines.append(f"  {i + 1:2d}  {f:5.1f}   {s:+7.3f}   "
                             f"{'PM' if route == 'PM' else 'ADD'}")
            lines += ["",
                      f"residual: fingerprint {r['err_spec']:.2f}%   "
                      f"waveform {r['err_wave']:.2f}%",
                      "",
                      "error vs number of drawbars:"]
            for n, es, ew in r['err_curve']:
                lines.append(f"  {n:2d} bar(s):  fp {es:6.2f}%   wave {ew:6.2f}%")
            lines += ["",
                      "Levels export as ENV DEPTH with gate envelope",
                      "(A0 D0 S100% R0), k sliders at 0."]
            self.txt.delete('1.0', 'end')
            self.txt.insert('1.0', "\n".join(lines))

        # ---------------- export ----------------
        def export_patch(self):
            if self.result is None:
                messagebox.showinfo("ISHTAR Resynth", "Run a fit first.")
                return
            os.makedirs(PATCH_EXPORT_DIR, exist_ok=True)
            default = os.path.splitext(self.wav_name)[0] or "Resynth patch"
            path = filedialog.asksaveasfilename(
                initialdir=PATCH_EXPORT_DIR, initialfile=default + ".md",
                defaultextension=".md", filetypes=[("ISHTAR patch", "*.md")])
            if not path:
                return
            name = os.path.splitext(os.path.basename(path))[0]
            md = make_patch_markdown(name, self.result, self.f0, self.wav_name)
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(md)
            self.lbl_status.config(text=f"patch saved: {os.path.basename(path)}")

        def render_audition(self):
            if self.result is None or self.target_cycle is None:
                messagebox.showinfo("ISHTAR Resynth", "Run a fit first.")
                return
            import soundfile as sf
            default = os.path.splitext(self.wav_name)[0] + "_AB.wav"
            path = filedialog.asksaveasfilename(
                initialfile=default, defaultextension=".wav",
                filetypes=[("WAV", "*.wav")])
            if not path:
                return
            out = render_ab(self.objective.t_band, self.result['bars'],
                            self.result['density'], self.C, self.f0,
                            sr=int(self.sr))
            sf.write(path, out, int(self.sr))
            self.lbl_status.config(text=f"A/B saved: target 2s, gap, fit 2s")

    root = __import__('tkinter').Tk()
    App(root)
    root.mainloop()


if __name__ == '__main__':
    if '--selftest' in sys.argv:
        sys.exit(selftest())
    run_gui()

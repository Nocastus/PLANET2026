/*
  ==============================================================================
    PLANETEffects.h - Modular Effects Processing
    Template-based design for consistent two-parameter effect modules
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

//==============================================================================
// MODULAR EFFECTS PROCESSOR
//==============================================================================

class PLANETEffects {
public:
    PLANETEffects();

    // Setup
    void prepareToPlay(double sampleRate);

    // Main processing - converts mono input to stereo output with effects
    std::pair<float, float> processStereoSample(float monoInput);

    // Parameter updates (call once per audio block for efficiency)
    void updateDetuneParams(float amount, float mix);
    void updateWarmthParams(float amount);
    void updatePunchParams(float amount, float frequency);


private:
    //==========================================================================
    // SHARED BIQUAD (RBJ shelves) - used by Warmth (low shelf + tone cut) and
    // Punch (presence). Was duplicated per-processor; unified when Warmth grew
    // a second (high-shelf) filter for the post-saturation tape darkening.
    //==========================================================================
    struct BiquadFilter {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;

        void setLowShelf(double sampleRate, float freq, float q, float gainDB) {
            float A = std::pow(10.0f, gainDB / 40.0f);
            float w0 = 2.0f * juce::MathConstants<float>::pi * freq / (float)sampleRate;
            float cosw0 = std::cos(w0);
            float sinw0 = std::sin(w0);
            float alpha = sinw0 / (2.0f * q);

            float sqrtA = std::sqrt(A);
            float beta = 2.0f * sqrtA * alpha;

            float a0 = (A + 1.0f) + (A - 1.0f) * cosw0 + beta;

            b0 = (A * ((A + 1.0f) - (A - 1.0f) * cosw0 + beta)) / a0;
            b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0)) / a0;
            b2 = (A * ((A + 1.0f) - (A - 1.0f) * cosw0 - beta)) / a0;
            a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0)) / a0;
            a2 = ((A + 1.0f) + (A - 1.0f) * cosw0 - beta) / a0;
        }

        void setHighShelf(double sampleRate, float freq, float q, float gainDB) {
            float A = std::pow(10.0f, gainDB / 40.0f);
            float w0 = 2.0f * juce::MathConstants<float>::pi * freq / (float)sampleRate;
            float cosw0 = std::cos(w0);
            float sinw0 = std::sin(w0);
            float alpha = sinw0 / (2.0f * q);

            float sqrtA = std::sqrt(A);
            float beta = 2.0f * sqrtA * alpha;

            float a0 = (A + 1.0f) - (A - 1.0f) * cosw0 + beta;

            b0 = (A * ((A + 1.0f) + (A - 1.0f) * cosw0 + beta)) / a0;
            b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0)) / a0;
            b2 = (A * ((A + 1.0f) + (A - 1.0f) * cosw0 - beta)) / a0;
            a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0)) / a0;
            a2 = ((A + 1.0f) - (A - 1.0f) * cosw0 - beta) / a0;
        }

        float process(float input) {
            float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

            x2 = x1;
            x1 = input;
            y2 = y1;
            y1 = output;

            return output;
        }

        void reset() {
            x1 = x2 = y1 = y2 = 0.0f;
        }
    };

    //==========================================================================
    // DUAL DETUNE EFFECT
    //==========================================================================
    struct DetuneProcessor {
        static constexpr int BUFFER_SIZE = 4096;  // history depth for splice alignment; the
                                                  // taps themselves stay inside the corridor
                                                  // below (~3-40ms), NOT the whole buffer
        static constexpr float MAX_DETUNE_CENTS = 50.0f;  // More obvious effect
        static constexpr float BASE_DELAY_SAMPLES = 512.0f;  // initial tap delay (mid corridor)

        // ---- Splice-on-approach wrap declick (9 Jul 2026, v2) ----
        // The taps drift relative to the write head; letting one collide snapped the
        // delay by a whole buffer = the periodic "bump" (worst on bass). Instead, when
        // a tap reaches a corridor edge we jump it by a correlation-picked distance
        // (a whole number of waveform periods) and crossfade over SPLICE_FADE_SAMPLES.
        // Between splices the signal path is exactly the pre-fix single-tap read.
        //
        // v1 POSTMORTEM: window 64 was locally linear on bass, so every candidate
        // scored ~1 and jumps landed at random phase = irregular random bumps (worse
        // than the periodic ones). Phase-locking a bass note needs BOTH a window that
        // spans a good fraction of its period AND a jump range of >= one full period
        // (43Hz period = 1025 samples = the entire old 1024 buffer, which is why the
        // old buffer could never splice bass cleanly, crossfade or not).
        //
        // Corridor geometry (samples; fixed, so ms halve at 96k - beta is 44.1/48k):
        // taps live in delay [SPLICE_MARGIN, SPLICE_CORRIDOR_HIGH]; a jump traverses
        // it, so corridor width == SPLICE_SEARCH_MAX >= one period down to ~27Hz.
        // Validity: corridor high (1792) + corr window (512) < BUFFER_SIZE.
        static constexpr int SPLICE_MARGIN        = 128;   // corridor low edge (fast-tap trigger)
        static constexpr int SPLICE_CORRIDOR_HIGH = 1792;  // corridor high edge (slow-tap BACKSTOP)
        static constexpr int SPLICE_FADE_SAMPLES  = 256;   // ~6ms at 44.1kHz
        static constexpr int SPLICE_SEARCH_MIN    = 256;   // shortest allowed jump
        static constexpr int SPLICE_SEARCH_MAX    = 1664;  // >= one period down to ~27Hz @ 44.1k
        static constexpr int SPLICE_CORR_WINDOW   = 1024;  // full period down to ~43Hz: chord-safe
                                                           // corr estimates (512 leaked spurious
                                                           // >0.9 reads through the early gate)
        static constexpr float SPLICE_SHORT_BIAS  = 0.02f; // prefer the SHORTEST aligned jump (v4)
        static constexpr int SPLICE_TRIGGER_SLACK = 32;    // slow-tap landing slack above MARGIN
        static constexpr float SPLICE_EARLY_CORR  = 0.9f;  // min raw corr for an EARLY slow splice
        static constexpr int SPLICE_VALID_FLOOR   = 64;    // candidate window validity margin

        // v4 LAG GEOMETRY (9 Jul 2026 night): jumps prefer ~one period and the slow
        // tap splices as soon as its planned jump is known, so BOTH taps hug the
        // low-lag end of the corridor (~3ms..1-2 periods; average wet lag ~= the
        // pre-fix version's ~12ms). The v3 policy (mid-length jumps, slow tap
        // triggering at the corridor top) parked the slow tap at ~25-30ms lag, which
        // Gerard heard as a stronger Spread with extra wobble on moving pitches.
        // CORRIDOR_HIGH stays as the physics backstop: content whose period demands
        // a long jump (deep bass, CHORDS) still gets one - an early slow splice is
        // only allowed when its jump's RAW correlation clears SPLICE_EARLY_CORR.
        // (Sim-caught chord regression without the gate: at low delay the long jump
        // a chord needs isn't measurable yet - its comparison window would reach
        // past the write head - so the search would commit to a short jump that
        // aligns only the strongest note. maxJump clamps scoring to measurable
        // candidates; the corr gate makes "wait for the long jump" the default.)

        // The jump search is SLICED across the approach (9 Jul 2026 evening): one
        // candidate scored per sample, started SEARCH_SAMPLES + SLACK samples of
        // drift before the tap reaches its corridor edge. Drift cancels out of that
        // lead, so at ANY Spread the search finishes ~SLACK samples early and the
        // chosen jump is at most a few ms stale - inaudible on periodic content.
        // This replaces the one-shot search, whose ~80us single-sample spike bounced
        // the DAW CPU meter at small ASIO buffers (fixed-budget principle restored:
        // worst per-sample cost is ONE 512-tap correlation, ~0.8us).
        static constexpr int SPLICE_SEARCH_SAMPLES = 362;  // 353 coarse + up to 9 refine steps
        static constexpr int SPLICE_SEARCH_SLACK   = 64;   // finish margin (samples of drift lead)
        static constexpr int SPLICE_HARD_FLOOR     = 48;   // force-finish depth if automation outruns it

        std::array<float, BUFFER_SIZE> buffer;
        int writeIndex = 0;

        // Read positions track write position with rate offsets. writeIndex starts at
        // 0, so an initial offset of BUFFER_SIZE - BASE_DELAY_SAMPLES puts both taps
        // BASE_DELAY_SAMPLES behind the write head, mid corridor.
        float leftReadOffset = (float)BUFFER_SIZE - BASE_DELAY_SAMPLES;
        float rightReadOffset = (float)BUFFER_SIZE - BASE_DELAY_SAMPLES;

        float leftPlaybackRate = 1.0f;
        float rightPlaybackRate = 1.0f;

        // Per-tap splice crossfade state. fadeRemaining == 0 means not fading;
        // fadeOldOffset is the retiring trajectory, still advancing at the tap's rate.
        int   leftFadeRemaining = 0;
        int   rightFadeRemaining = 0;
        float leftFadeOldOffset = 0.0f;
        float rightFadeOldOffset = 0.0f;

        // Per-tap sliced jump search. The reference window is frozen at begin time
        // (the anchor); candidate windows read the live buffer, but every position
        // involved is old enough that nothing scored is overwritten mid-search.
        struct SpliceSearch {
            enum Stage { Idle, Coarse, Refine, Done };
            Stage stage = Idle;
            int   anchor = 0;        // int tap position the reference window was taken at
            bool  jumpBack = true;   // fast tap jumps back, slow tap jumps forward
            int   nextJump = 0;      // next candidate to score (coarse: step 4; refine: step 1)
            int   refineHi = 0;
            int   maxJump = SPLICE_SEARCH_MAX; // validity clamp: longest measurable candidate
            int   bestJump = SPLICE_SEARCH_MIN;
            float bestScore = -1.0e9f;
            float bestCorr = -1.0f;  // raw correlation of the winner (early-splice gate)
            int   doneAge = 0;       // samples since Done - re-anchor guard for stalled approaches
            float x[SPLICE_CORR_WINDOW];
        };
        SpliceSearch leftSearch;
        SpliceSearch rightSearch;

        // Equal-power crossfade gains. Targets are computed once per block in
        // updateParameters() (they depend only on the mix param); the live gains glide
        // toward them per sample (~10 ms one-pole) so a moving Mix knob doesn't zipper.
        // NB the zipper predates the v0.6.4 hoist - the gains always stepped at block
        // rate; the old per-sample cos/sin recomputed the same block-constant values.
        float dryGain = 1.0f;
        float wetGain = 0.0f;
        float targetDryGain = 1.0f;
        float targetWetGain = 0.0f;
        float gainSmoothCoeff = 0.002f;   // set from sampleRate in updateParameters()

        // One-pole wet-tap smoothing - originally for wrap artifacts, kept for its
        // (approved) slight HF rounding of the wet tone now splices handle the wraps
        float leftPrevSample = 0.0f;
        float rightPrevSample = 0.0f;
        static constexpr float SMOOTHING_FACTOR = 0.95f; // Gentle smoothing

        void updateParameters(float amount, float mixAmount, double sampleRate);
        std::pair<float, float> process(float input);

    private:
        float processTap(float& readOffset, float playbackRate,
                         int& fadeRemaining, float& fadeOldOffset, SpliceSearch& search);
        void beginSearch(SpliceSearch& s, int basePos, bool jumpBack, int maxJump) const;
        void stepSearch(SpliceSearch& s) const;   // bounded: scores ONE candidate
        void finishSearch(SpliceSearch& s) const; // automation guard, rare
        float scoreJump(const SpliceSearch& s, int jump, int step) const;
        float interpolatedRead(float readPosition);
        float centsToRatio(float cents);
    };
 
    //==========================================================================
     // WARMTH EFFECT (15ips Tape Character)
     //==========================================================================
    // 0-50%: low-shelf bass boost only (unchanged - existing patches below half
    // are untouched). 50-100%: tape-style saturation fades in CONTINUOUSLY (the
    // old design stepped -3dB of makeup gain at 51% - the audible "switch-on" -
    // and its full-insert tanh at drive up to 4 with no post-filtering was the
    // buzzy, high-harmonic character Gerard disliked; reworked 4 Jul 2026).
    // Signal path: low shelf -> parallel tanh saturation -> high-shelf tone cut.
    struct WarmthProcessor {
        // ---- Saturation voicing constants (TUNE BY EAR) ----
        // t = (warmthAmount - 0.5) * 2, clamped to 0..1 (the upper half of the knob).
        static constexpr float kMaxExtraDrive = 2.0f;   // drive = 1 + this*t (was 3: drive 4 = buzz)
        static constexpr float kBias          = 0.12f;  // asymmetric bias * t -> even harmonics (tape)
        static constexpr float kMakeupDB      = -8.0f;  // * t^2: counters the shaper's small-signal boost
        static constexpr float kToneCutDB     = -4.0f;  // * t: post-sat high-shelf cut (tape darkening)
        static constexpr float kToneFreqHz    = 4500.0f;

        BiquadFilter lowShelfFilter;   // pre-saturation bass boost (the 0-50% behaviour)
        BiquadFilter toneFilter;       // post-saturation darkening (0dB at t=0 = transparent)

        float warmthAmount = 0.0f;
        double sampleRate = 44100.0;

        // Block-rate transfer-curve constants (functions of warmthAmount only)
        float driveAmount = 1.0f;        // 1 + kMaxExtraDrive*t
        float biasTerm = 0.0f;           // kBias*t
        float dcCorrection = 0.0f;       // tanh(biasTerm) - removes the DC the bias introduces
        float invTanhDrive = 1.0f;       // 1/tanh(drive) - peak normalisation (input 1 -> ~1)
        // Saturation wet-mix and makeup: block-rate targets + per-sample smoothed values
        // (same 10 ms de-zipper as the detune Mix control).
        float targetSatDepth = 0.0f;     // t^2: eases in from EXACTLY 0 at 50% - no switch-on step
        float satDepth = 0.0f;
        float targetMakeup = 1.0f;       // dB(kMakeupDB * t^2), continuous (old code stepped -3dB)
        float makeupGain = 1.0f;
        float gainSmoothCoeff = 0.002f;

        void prepareToPlay(double sr);
        void updateParameters(float amount, double sr);
        float process(float input);
    };

    //==========================================================================
    // PUNCH EFFECT (1176 FET Compressor + Presence)
    //==========================================================================
    struct PunchProcessor {
        BiquadFilter presenceFilter;   // shared BiquadFilter (see top of private section)

        // Compressor state
        float envelope = 0.0f;
        float punchAmount = 0.0f;
        float punchFreq = 1800.0f;
        double sampleRate = 44100.0;

        // 1176 FET characteristics
        static constexpr float RATIO = 4.0f;
        static constexpr float THRESHOLD = 0.5f;  // -6dB
        static constexpr float ATTACK_MS = 4.0f;
        static constexpr float RELEASE_MS = 50.0f;

        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;

        void prepareToPlay(double sr);
        void updateParameters(float amount, float frequency, double sr);
        float process(float input);

    private:
        float compress(float level, float threshold, float ratio);
    };

    //==========================================================================
    // EFFECT INSTANCES
    //==========================================================================
    DetuneProcessor detuneProcessor;
    WarmthProcessor warmthProcessor;
    PunchProcessor punchProcessor;


    // Audio properties
    double currentSampleRate = 44100.0;


};

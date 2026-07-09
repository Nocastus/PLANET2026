/*
  ==============================================================================
    PLANETVoice.cpp - Individual Voice Implementation
  ==============================================================================
*/

#include "PLANETVoice.h"
#include <random>
#include <functional>

//==============================================================================
// CONSTRUCTOR
//==============================================================================

PLANETVoice::PLANETVoice()
{
    // Initialize modulation states
    for (auto& modState : coeffModStates) {
        modState.envStage = EnvelopeStage::Idle;
        modState.envTime = 0.0;
        modState.envLevel = 0.0f;
        modState.lfoPhase = 0.0;
    }

    // Seed the envelope-curve cache from the default exponentialControl
    refreshEnvCurveCache();

    // Routing defaults until the first cycle-boundary snapshot: all bars feed the phase
    // path (classic PM), none go additive - matches the "first cycle is a pure sine"
    // start behaviour (coefficient grid is zero until the first promotion anyway).
    routePMGain.fill(1.0f);
    routeAddAmp.fill(0.0f);
    barDensity.fill(0.0f);
    barNoise.fill(false);
    noiseAmp.fill(1.0f);
    noiseEps.fill(0.0f);
    noiseAmpPrev.fill(1.0f);
    noiseEpsPrev.fill(0.0f);
}

// ======================== NOISE-BAR WALK VOICING (v4, tune by ear) ========================
// Per-cycle random-walk steps for the band-pass-noise bars, all scaled by the bar's
// Density knob w (repurposed as width). Bandwidth ~ lam * f0, so these are Q settings,
// pitch-independent. Symptom map:
//   pitch wobble too seasick  -> lower kNoiseSigF1 (or kNoiseSigF0 for the narrow end)
//   breath pumps too much     -> lower kNoiseSigA1
//   texture too static/tonal  -> raise kNoiseLam0 / kNoiseSigA0
//   want wider extremes at w=1 -> raise the *1 constants
static constexpr float kNoiseSigA0 = 0.06f, kNoiseSigA1 = 0.35f;   // amplitude step size
static constexpr float kNoiseSigF0 = 0.0015f, kNoiseSigF1 = 0.02f; // detune step size (fraction of f)
static constexpr float kNoiseLam0  = 0.08f, kNoiseLam1  = 0.25f;   // mean-reversion rate (per cycle)
static constexpr float kNoiseEpsMax = 0.08f;                       // hard detune ceiling (+-8%)

//==============================================================================
// VOICE CONTROL METHODS
//==============================================================================

void PLANETVoice::startNote(int noteNumber, float velocity, double sampleRate, float vintageAmount,
    float velToAmplitude, float brilliance, float lifeAmount, int lifeSeed,
    const std::array<bool, NUM_COEFFICIENTS>* drawbarFire,
    float glideFromHz, float portamentoTime, int portamentoMode,
    float detuneCents)
{
    // [Existing initialization code...]
    currentNoteNumber = noteNumber;
    noteVelocity = velocity;
    noteIsActive = true;

basePitchFrequency = 440.0 * std::pow(2.0, (noteNumber - 69) / 12.0);

// Apply vintage pitch randomization (simulate analog oscillator imperfections)
if (vintageAmount > 0.0f) {
    // Create a stable random seed based on voice address (so each voice has consistent variation)
    std::hash<PLANETVoice*> hasher;
    size_t seed = hasher(this) + noteNumber;  // Include note number for slight variation per note
    std::mt19937 generator(seed);
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    
    // Generate pitch variation in cents (}2 cents max)
    float pitchVariationCents = distribution(generator) * (vintageAmount / 100.0f) * 20.0f;
    
    // Apply to base frequency
    basePitchFrequency *= std::pow(2.0f, pitchVariationCents / 1200.0f);
}

// Unison detune (F6): a permanent, static cents offset for this stacked voice (0 for a single
// voice). Applied to the base pitch so it anchors the whole note; the glide origin below targets
// this detuned pitch, and vibrato/wheel/pitch-env still modulate symmetrically around it.
if (detuneCents != 0.0f)
    basePitchFrequency *= std::pow(2.0, (double)detuneCents / 1200.0);

currentFrequency = basePitchFrequency;
angleDelta = currentFrequency * 2.0 * juce::MathConstants<double>::pi / sampleRate;

    // ---- Portamento glide setup (F2a) ----
    // Default to "arrived" (no glide); engage only with a valid origin, a non-zero time, and a
    // real interval to cover. glideFromHz already carries the origin voice's in-progress glide,
    // so a voice reused mid-glide continues smoothly rather than jumping to a fresh origin.
    glideStartOffset = 0.0f;
    glideLevel       = 1.0f;
    glideTime        = 0.0;
    glideTimeSec     = 0.0;
    if (glideFromHz > 0.0f && portamentoTime > 0.0f && basePitchFrequency > 0.0)
    {
        float offs = 12.0f * std::log2(glideFromHz / (float)basePitchFrequency);  // signed semitones
        if (std::abs(offs) > 1.0e-4f)
        {
            glideStartOffset = offs;
            // Time mode: fixed duration for every glide. Rate mode: portamentoTime is
            // seconds-per-octave, so the duration scales with the interval (wide leaps glide longer).
            glideTimeSec = (portamentoMode == 1)
                         ? (double)portamentoTime * (std::abs(offs) / 12.0)
                         : (double)portamentoTime;
            if (glideTimeSec > 1.0e-4)
                glideLevel = 0.0f;   // begin the glide (level runs 0 -> 1 across glideTimeSec)
        }
    }
    hasEverSounded = true;

    totalPitchOffset = 0.0f;
    currentAngle = 0.0;

    // ======================== LIFE: per-strike setup ========================
    // Stage 3 doublets: each drawbar's partial is two components split by a small ratio
    // (the two vibration polarizations of a struck string), whose interference makes the
    // partial beat. The instrument's character (per-drawbar split scale and energy share)
    // is drawn deterministically from the patch seed - same seed, same instrument - while
    // each strike lands at a random point in every partial's beat cycle, so repeated
    // notes differ like repeated plucks of a real string.
    modPhases.fill(0.0);
    modPhasesB.fill(0.0);
    doubletRatioA.fill(1.0);
    doubletRatioB.fill(1.0);
    doubletEngaged.fill(false);

    // Reset routing to defaults for the first cycle (recomputed at the first cycle boundary);
    // keeps a reused voice's opening cycle clean rather than carrying stale routing.
    routePMGain.fill(1.0f);
    routeAddAmp.fill(0.0f);
    barDensity.fill(0.0f);
    barNoise.fill(false);   // noiseRng deliberately NOT reset - strikes must differ, like lifeNoiseState

    {
        std::mt19937 lifeGen((uint32_t)lifeSeed);
        std::uniform_real_distribution<float> u01(0.0f, 1.0f);

        // Per-strike randomness: a cheap LCG advanced across note-ons. Deliberately NOT
        // the Vintage hash (stable per voice+note) - strikes must differ, not repeat.
        lifeNoiseState += (uint32_t)noteNumber * 2654435761u;
        auto strikeRand = [this]() {
            lifeNoiseState = lifeNoiseState * 1664525u + 1013904223u;
            return (float)(lifeNoiseState >> 8) * (1.0f / 16777216.0f);   // [0,1)
        };

        // Voicing constants (dev panel when wired, defaults otherwise):
        // beatDepth pulls every partial's energy split toward 0.5 (deeper beat nulls);
        // strikeSpread scales how much each strike perturbs that split.
        const float beatDepth    = lifeVoicing ? lifeVoicing->beatDepth.load()    : LifeVoicingParams::kDefaultBeatDepth;
        const float strikeSpread = lifeVoicing ? lifeVoicing->strikeSpread.load() : LifeVoicingParams::kDefaultStrikeSpread;
        const float sSpread   = (1.0f - beatDepth) * 0.45f;
        const float jitterAmp = strikeSpread * 0.25f;

        // Character is drawn even when LIFE is 0 so a mid-note knob-up can engage the
        // doublets live; until engagement it changes nothing (single path, ratios 1.0).
        for (int i = 0; i < NUM_COEFFICIENTS; ++i)
        {
            doubletUnit[i] = 0.3f + 0.7f * u01(lifeGen);                       // seeded split scale
            const float sBase = 0.5f + (2.0f * u01(lifeGen) - 1.0f) * sSpread; // seeded energy share
            doubletMixB[i] = juce::jlimit(0.05f, 0.95f,
                                          sBase + 2.0f * jitterAmp * (strikeRand() - 0.5f));
        }

        if (lifeAmount > 0.0f)
        {
            for (int i = 1; i < NUM_COEFFICIENTS; ++i)   // i=0: fundamental stays locked
            {
                doubletEngaged[i] = true;
                // Random relative phase = random point in this partial's beat cycle:
                // some strikes start at the null (partial blooms in late), some at the peak.
                modPhasesB[i] = (double)strikeRand() * 2.0 * juce::MathConstants<double>::pi;
            }
        }
    }

    updateLifeRatios(lifeAmount);

    // Reset vibrato state for new note
    vibratoState.reset();



    // [Existing envelope initialization...]
    ampEnvStage = EnvelopeStage::Attack;
    ampEnvTime = 0.0;
    ampEnvLevel = 0.0f;

    // Reset pitch envelope for new note
    pitchEnvStage = EnvelopeStage::Attack;
    pitchEnvTime = 0.0;
    pitchEnvLevel = 0.0f;

    // Per-drawbar envelope trigger (F10). Firing drawbars reset to Attack (the normal behaviour);
    // a Single-trig drawbar that must not fire on this note is forced to Idle/silent so a reused
    // voice can't leak a stale envelope. Only the envelope is gated - the drawbar's base level
    // (globalParams[i].coefficient) is untouched, matching "the envelope is single-triggered".
    for (int i = 0; i < NUM_COEFFICIENTS; ++i) {
        const bool fire = (drawbarFire == nullptr) || (*drawbarFire)[i];
        coeffModStates[i].envStage = fire ? EnvelopeStage::Attack : EnvelopeStage::Idle;
        coeffModStates[i].envTime = 0.0;
        coeffModStates[i].envLevel = 0.0f;
        // Firing bars restart the LFO fade-in; a non-firing Single-trig bar sits in Idle
        // and must keep the legacy full-depth LFO there, so its held level is 1.
        coeffModStates[i].lfoFadeLevel = fire ? 0.0f : 1.0f;
    }

    cachedVelocityAmplitude = std::pow(velocity, velToAmplitude / 100.0f);

}

// Recompute the per-drawbar doublet rate ratios from the current LIFE depth.
// Called at note-on and then once per carrier cycle, so the LIFE knob (and, later,
// Brilliance-dependent weighting) acts on held notes in real time. Phases are
// continuous and rates only change at the cycle boundary, so retuning is click-free.
void PLANETVoice::updateLifeRatios(float lifeAmount)
{
    // Voicing constants come from the dev panel when present (atomics on the
    // processor); fall back to the struct defaults when not wired up.
    const float cMax = lifeVoicing ? lifeVoicing->splitCeiling.load() : LifeVoicingParams::kDefaultSplitCeiling;
    const float resp = lifeVoicing ? lifeVoicing->response.load()     : LifeVoicingParams::kDefaultResponse;
    const float tilt = lifeVoicing ? lifeVoicing->tilt.load()         : LifeVoicingParams::kDefaultTilt;

    const float depth01 = juce::jlimit(0.0f, 1.0f, lifeAmount * 0.01f);
    const float depth = depth01 > 0.0f ? std::pow(depth01, resp) : 0.0f;

    for (int i = 1; i < NUM_COEFFICIENTS; ++i)   // i=0: fundamental stays locked
    {
        // weight(i): bi-directional tilt across the 9 movable drawbars (i = 1..9; i=0 is the
        // locked fundamental). tilt = 0 -> flat (every partial weighted equally); tilt > 0 ->
        // top-weighted (shimmer up top); tilt < 0 -> bottom-weighted (counters the fact that
        // high partials beat faster for a fixed cents offset - beat rate ~ frequency). Normalised
        // so the favoured end is always 1.0, which keeps the Split-ceiling (cMax) calibration
        // meaningful in either direction. NB: overall LIFE energy rises as |tilt| -> 0 (flat puts
        // every partial at full cMax), so expect to re-trim Split ceiling when changing tilt.
        const float x = (i - 1) / 8.0f;   // 0..1 across drawbars 2..10
        const float w = std::exp(tilt * (x - 0.5f) - std::abs(tilt) * 0.5f);
        const float c = depth * w * doubletUnit[i] * cMax;

        if (c <= 0.0f)
        {
            // Engaged drawbars keep both components running at the carrier rate so a knob
            // sweep through zero stays continuous; never-engaged drawbars remain on the
            // single-accumulator path (bit-identical to LIFE off).
            doubletRatioA[i] = 1.0;
            doubletRatioB[i] = 1.0;
            continue;
        }

        if (!doubletEngaged[i])
        {
            // LIFE turned up mid-note: start the pair in phase (beat begins at its peak)
            // so the modulator value doesn't step.
            doubletEngaged[i] = true;
            modPhasesB[i] = modPhases[i];
        }

        // Half the split each way as a ratio: the pair stays centred on f (geometric
        // mean), so the partial's perceived pitch doesn't shift - it only beats.
        const double r = std::pow(2.0, (double)c / 2400.0);
        doubletRatioA[i] = r;
        doubletRatioB[i] = 1.0 / r;
    }
}

// Modulate phase angle to create pitch offsets
void PLANETVoice::updatePitchFromOffset(double sampleRate)
{
    // Convert semitone offset to frequency multiplier
    double pitchMultiplier = std::pow(2.0, totalPitchOffset / 12.0);

    // Apply to base frequency
    currentFrequency = basePitchFrequency * pitchMultiplier;

    // Update angle delta for new frequency
    angleDelta = currentFrequency * 2.0 * juce::MathConstants<double>::pi / sampleRate;
}

// Portamento (F2a): the pitch a reused voice should glide FROM - base pitch plus any glide still
// in progress, but WITHOUT vibrato/wheel (those are momentary displacements, not a glide origin).
float PLANETVoice::getCurrentGlidePitchHz() const
{
    if (!hasEverSounded)
        return 0.0f;   // never played - nothing to glide from (no glide-from-silence)

    // (1 - glideLevel) * glideStartOffset is the still-decaying part of the last glide; it is 0
    // once a note has arrived, so this returns the plain base pitch for a settled voice.
    float glideSemis = (1.0f - glideLevel) * glideStartOffset;
    return (float)(basePitchFrequency * std::pow(2.0, glideSemis / 12.0));
}


void PLANETVoice::stopNote(bool sustainPedalDown)
{
    if (sustainPedalDown) {
        // If sustain pedal is down, mark this voice as sustained instead of releasing
        sustainPedalHeld = true;
        // Don't trigger envelope release yet - keep playing
    }
    else {
        // Normal release behavior
        triggerRelease();
    }

    // Always clear the note number so note-lookup (retrigger/stop scans) won't find this voice
    currentNoteNumber = -1;
}

// Add a new method to PLANETVoice.cpp:
void PLANETVoice::triggerRelease()
{
    sustainPedalHeld = false;

    // Trigger amplitude envelope release
    if (ampEnvStage != EnvelopeStage::Release && ampEnvStage != EnvelopeStage::Idle) {
        ampReleaseStartLevel = ampEnvLevel;
        ampEnvStage = EnvelopeStage::Release;
        ampEnvTime = 0.0;
    }

    // Trigger coefficient envelope releases
    for (auto& modState : coeffModStates) {
        if (modState.envStage != EnvelopeStage::Release && modState.envStage != EnvelopeStage::Idle) {
            modState.releaseStartLevel = modState.envLevel;
            modState.envStage = EnvelopeStage::Release;
            modState.envTime = 0.0;
        }
    }
}

//==============================================================================
// MAIN AUDIO PROCESSING
//==============================================================================

float PLANETVoice::processNextSample(const CoefficientArray& globalParams,
    float ampAttack, float ampDecay, float ampSustain, float ampRelease,
    float brilliance, float carrierMorph, double sampleRate,
    float pitchWheelOffset,
    float vibratoRate, float vibratoDepth, float vibratoFadeIn, float vibratoVelThreshold,
    float velToAmplitude, float velToAttackTime, float vintageAmount,
    float pitchEnvDistance, float pitchAttackTime,
    float lifeAmount,
    double bpm, double beatPosition)
{
    auto twoPi = 2.0 * juce::MathConstants<double>::pi;
    cycleStartFlag = false;

    // Calculate timing
    double sampleDeltaTime = 1.0 / sampleRate;           // For amplitude envelope (per-sample)

    // Process amplitude envelope (per sample)
    float ampEnvValue = processAmplitudeEnvelope(sampleDeltaTime,
        ampAttack * (1.0f - (noteVelocity * velToAttackTime / 50.0f)),
        ampDecay, ampSustain, ampRelease);

    // If amplitude envelope has reached Idle, voice is finished
    if (ampEnvStage == EnvelopeStage::Idle) {
        noteIsActive = false;
        currentNoteNumber = -1;
        return 0.0f;
    }

    // Normalize current phase
    auto normalizedPhase = currentAngle / twoPi;

    // Advance phase
    currentAngle += angleDelta;

    // Detect cycle wrap (zero-crossing)
    if (currentAngle >= twoPi) {
        currentAngle -= twoPi;
        cycleStartFlag = true;

        // Timing for the coefficient envelopes / LFOs (per-cycle; only needed in this branch)
        double cycleDeltaTime = 1.0 / currentFrequency;

        // ======================== VIBRATO PROCESSING (EXISTING) ========================
        double vibratoPhaseAdvance = 2.0 * juce::MathConstants<double>::pi * vibratoRate * cycleDeltaTime;
        vibratoState.lfoPhase += vibratoPhaseAdvance;
        if (vibratoState.lfoPhase >= twoPi)
            vibratoState.lfoPhase -= twoPi;

        // Update vibrato fade-in envelope
        if (vibratoFadeIn > 0.0f) {
            vibratoState.fadeInTime += cycleDeltaTime;
            if (vibratoState.fadeInTime >= vibratoFadeIn) {
                vibratoState.fadeInLevel = 1.0f;  // Full vibrato
            }
            else {
                vibratoState.fadeInLevel = (float)(vibratoState.fadeInTime / vibratoFadeIn);
            }
        }
        else {
            vibratoState.fadeInLevel = 1.0f;  // Immediate full vibrato
        }

        // Calculate vibrato contribution (use LUT for performance)
        const auto& sineLUT = SineLUT::getInstance();
        float vibratoLFOValue = sineLUT.lookup(vibratoState.lfoPhase);
        float effectiveVibratoDepth = vibratoDepth * vibratoState.fadeInLevel;
        // VEL gate: when armed (threshold > 0), a note quieter than the threshold gets no vibrato.
        // noteVelocity is normalised 0-1; threshold arrives normalised too. Fade-in still runs above.
        if (vibratoVelThreshold > 0.0f && noteVelocity + 1.0e-6f < vibratoVelThreshold)
            effectiveVibratoDepth = 0.0f;
        float vibratoOffset = vibratoLFOValue * effectiveVibratoDepth;

        // ======================== PITCH ATTACK ENVELOPE ========================
        // Normalised finite-duration exponential: keeps the musical exponential sweep
        // (fast initial movement, graceful slow-in) but is scaled to reach EXACTLY 1.0
        // at t = pitchAttackTime, so the pitch lands perfectly in tune with no residual
        // detune on held notes (the flaw of a plain asymptotic exponential).
        //   level(t01) = (1 - e^(-k*t01)) / (1 - e^(-k)),  t01 = elapsed / duration in [0,1]
        // k controls curvature: higher = more "exponential" feel. Tune by ear.
        constexpr float kPitchEnvCurve = 5.0f;
        if (pitchAttackTime <= 0.001f) {
            pitchEnvLevel = 1.0f;
        }
        else if (pitchEnvLevel < 1.0f) {
            pitchEnvTime += cycleDeltaTime;
            if (pitchEnvTime >= pitchAttackTime) {
                pitchEnvLevel = 1.0f;
            }
            else {
                float t01 = (float)(pitchEnvTime / pitchAttackTime);
                pitchEnvLevel = (1.0f - std::exp(-kPitchEnvCurve * t01))
                              / (1.0f - std::exp(-kPitchEnvCurve));
            }
        }
        float pitchEnvValue = pitchEnvLevel;

        // ======================== PITCH OFFSET ACCUMULATOR ========================
        // Reset and accumulate all pitch modulation sources
        resetPitchOffset();
        addPitchOffset(pitchWheelOffset);  // Add pitch wheel (passed as parameter)
        addPitchOffset(vibratoOffset);     // Add vibrato
        float pitchEnvOffset = (1.0f - pitchEnvValue) * pitchEnvDistance;
        addPitchOffset(pitchEnvOffset);    // Add pitch envelope

        // ======================== PORTAMENTO GLIDE (F2a) ========================
        // Same normalised-exp curve as the pitch envelope above (fast start, graceful slow-in,
        // lands EXACTLY in tune at glideTimeSec), but the distance is the per-voice, signed
        // glideStartOffset set at note-on. Advanced per carrier cycle, like the pitch envelope.
        if (glideTimeSec <= 1.0e-4) {
            glideLevel = 1.0f;
        }
        else if (glideLevel < 1.0f) {
            glideTime += cycleDeltaTime;
            if (glideTime >= glideTimeSec) {
                glideLevel = 1.0f;
            }
            else {
                float g01 = (float)(glideTime / glideTimeSec);
                glideLevel = (1.0f - std::exp(-kPitchEnvCurve * g01))
                           / (1.0f - std::exp(-kPitchEnvCurve));
            }
        }
        float glideOffset = (1.0f - glideLevel) * glideStartOffset;
        addPitchOffset(glideOffset);       // Add portamento glide

        updatePitchFromOffset(sampleRate); // Apply total pitch offset

        // Recalculate cycleDeltaTime with new modulated frequency
        cycleDeltaTime = 1.0 / currentFrequency;

        // Advance K coefficient LFO phases
        for (int i = 0; i < NUM_COEFFICIENTS; ++i) {
            if (globalParams[i].lfoAmount != 0.0f) {

                if (globalParams[i].lfoSync > 0.5f && bpm > 0.0) {
                    // SYNC MODE: Calculate phase directly from DAW beat position
                    // This locks the LFO to barlines regardless of time signature
                    int divIdx = juce::jlimit(0, NUM_SYNC_DIVISIONS - 1, (int)globalParams[i].lfoSyncDiv);
                    float beatsPerCycle = SYNC_DIVISION_BEATS[divIdx];
                    double cyclePosition = std::fmod(beatPosition / beatsPerCycle, 1.0);
                    if (cyclePosition < 0.0) cyclePosition += 1.0;  // Handle negative beat positions
                    double newPhase = cyclePosition * twoPi;

                    // Detect phase wrap for random LFO sample-and-hold
                    if (newPhase < coeffModStates[i].lfoPhase - juce::MathConstants<double>::pi) {
                        static uint32_t seed = 12345;
                        seed = seed * 1664525u + 1013904223u;
                        coeffModStates[i].randomLfoValue = ((seed & 0xFFFF) / 32767.5f) - 1.0f;
                    }

                    coeffModStates[i].lfoPhase = newPhase;
                }
                else {
                    // FREE MODE: Accumulate phase at Hz rate (original behaviour)
                    double lfoPhaseAdvance = twoPi * globalParams[i].lfoRate * cycleDeltaTime;
                    coeffModStates[i].lfoPhase += lfoPhaseAdvance;
                    if (coeffModStates[i].lfoPhase >= twoPi) {
                        coeffModStates[i].lfoPhase -= twoPi;
                        // Generate new random value on phase wrap (for sample-and-hold)
                        static uint32_t seed = 12345;
                        seed = seed * 1664525u + 1013904223u;
                        coeffModStates[i].randomLfoValue = ((seed & 0xFFFF) / 32767.5f) - 1.0f;
                    }
                }
            }
        }

        // Calculate final modulated coefficients (envelope processing merged into single loop)
        auto& stagedCoeffs = voiceCoeffGrid.getStagedCoefficients();
        for (int i = 0; i < NUM_COEFFICIENTS; ++i) {
            float finalCoeff = globalParams[i].coefficient;

            // Always process envelope for timing (needed for LFO fade-in even if envelopeAmount is zero)
            bool needsEnvelope = (globalParams[i].envelopeAmount != 0.0f) || (globalParams[i].lfoAmount != 0.0f);

            if (needsEnvelope) {
                float envValue = processEnvelope(coeffModStates[i].envStage, coeffModStates[i].envTime,
                    coeffModStates[i].envLevel, cycleDeltaTime,
                    globalParams[i].attackTime * (1.0f - (noteVelocity * velToAttackTime / 50.0f)), globalParams[i].decayTime,
                    globalParams[i].sustainLevel, globalParams[i].releaseTime,
                    ampEnvStage != EnvelopeStage::Release && ampEnvStage != EnvelopeStage::Idle,
                    coeffModStates[i].releaseStartLevel);

                // Only apply envelope modulation if envelope amount is non-zero
                if (globalParams[i].envelopeAmount != 0.0f) {
                    finalCoeff += envValue * globalParams[i].envelopeAmount;
                }
            }

            // Calculate LFO modulation with envelope-based fade-in
            if (globalParams[i].lfoAmount != 0.0f) {
                float lfoValue;
                if ((int)globalParams[i].lfoShape == 4) {
                    // Random: use stored sample-and-hold value
                    lfoValue = coeffModStates[i].randomLfoValue;
                }
                else {
                    lfoValue = generateLFOWaveform(coeffModStates[i].lfoPhase, globalParams[i].lfoShape);
                }

                // Scale LFO amount based on envelope stage for fade-in effect
                // Attack = delay (LFO silent), Decay = fade-in, Sustain = full LFO.
                // Release/Idle HOLD the level the fade had reached at note-off: a note
                // released mid-fade must not jump its LFO to full depth for the release.
                float lfoScale = 1.0f;
                if (coeffModStates[i].envStage == EnvelopeStage::Attack) {
                    // During attack: LFO is silent (delay period)
                    lfoScale = 0.0f;
                    coeffModStates[i].lfoFadeLevel = lfoScale;
                }
                else if (coeffModStates[i].envStage == EnvelopeStage::Decay) {
                    // During decay: fade in LFO from 0 to 1 with concave exponential curve (slower at start)
                    // envTime tracks progress through decay stage
                    float linearProgress = (float)(coeffModStates[i].envTime / globalParams[i].decayTime);
                    linearProgress = juce::jlimit(0.0f, 1.0f, linearProgress);
                    // Apply concave exponential curve: y = x^2 (slower start, faster end)
                    lfoScale = linearProgress * linearProgress;
                    coeffModStates[i].lfoFadeLevel = lfoScale;
                }
                else if (coeffModStates[i].envStage == EnvelopeStage::Sustain) {
                    coeffModStates[i].lfoFadeLevel = lfoScale;
                }
                else {
                    // Release or Idle: the fade no longer advances; use the held level
                    lfoScale = coeffModStates[i].lfoFadeLevel;
                }

                finalCoeff += lfoValue * globalParams[i].lfoAmount * lfoScale;
            }
        

            // Apply per-drawbar velocity to harmonic scaling
            float velScale = 1.0f + (noteVelocity - 0.5f) * 2.0f * (globalParams[i].velToHarmonic / 100.0f);

            // Seed mechanism: at VelToHarmonic > 75%, add harmonic content at high velocities
            // This allows velocity to "create" harmonics even when drawbar is at zero
            float seed = 0.0f;
            if (globalParams[i].velToHarmonic > 75.0f)
            {
                float seedStrength = (globalParams[i].velToHarmonic - 75.0f) / 25.0f;  // 0 at 75%, 1 at 100%
                float velContribution = juce::jmax(0.0f, (noteVelocity - 0.5f) * 2.0f);  // 0 at vel 64, 1 at vel 127
                seed = seedStrength * velContribution * 0.5f;  // Max seed coefficient = 0.5
            }

            float stagedCoeff = finalCoeff * velScale + seed;
            stagedCoeffs[i] = stagedCoeff;

            // Snapshot per-drawbar routing at the cycle boundary (click-free, same as the
            // coefficient promotion below). Additive amplitude = clamp(k_eff, -2..2) / 2 -
            // a hard ceiling only against envelope/LFO/velocity overdrive; the fitted
            // amplitude is otherwise linear (no soft-clip that would warp the partial).
            // Brilliance (morphAmount) multiplies later, in applyPhaseDistortion.
            routePMGain[i] = (globalParams[i].toPM >= 0.5f) ? 1.0f : 0.0f;
            routeAddAmp[i] = (globalParams[i].toOut >= 0.5f)
                ? juce::jlimit(-2.0f, 2.0f, stagedCoeff) * 0.5f
                : 0.0f;

            // Per-drawbar Density: snapshot here for the same click-free reason as
            // routing (both modulator waveforms are exactly 0 at the boundary).
            barDensity[i] = juce::jlimit(0.0f, 1.0f, globalParams[i].density);

            // Per-drawbar Noise (v4 band-pass): snapshot the hijack switch and step the
            // bar's two mean-reverting walks - amplitude (reverts to 1) and fractional
            // detune (reverts to 0). One step per carrier cycle = bandwidth scales with
            // f0 = constant musical Q. The bar's Density knob w sets the width.
            // The stepped values are TARGETS: the sample path applies them smoothstep-
            // interpolated from the previous targets across the coming cycle (see
            // PLANETVoice.h) - raw per-cycle steps click once the detune walk has moved
            // the modulator's boundary phase off sin = 0, and their staircases leak
            // sinc skirts far above the band (the "HF hash" heard on Frail Unison).
            barNoise[i] = (globalParams[i].noiseMode >= 0.5f);
            if (barNoise[i]) {
                const float w    = barDensity[i];   // Density = band WIDTH on a noise bar
                const float sigA = kNoiseSigA0 + kNoiseSigA1 * w;
                const float sigF = kNoiseSigF0 + kNoiseSigF1 * w;
                const float lam  = kNoiseLam0  + kNoiseLam1  * w;
                noiseRng = noiseRng * 1664525u + 1013904223u;
                const float u1 = (float)((noiseRng >> 8) & 0xFFFF) / 32767.5f - 1.0f;
                noiseRng = noiseRng * 1664525u + 1013904223u;
                const float u2 = (float)((noiseRng >> 8) & 0xFFFF) / 32767.5f - 1.0f;
                noiseAmpPrev[i] = noiseAmp[i];
                noiseEpsPrev[i] = noiseEps[i];
                noiseAmp[i] = juce::jlimit(0.0f, 2.0f,
                    noiseAmp[i] + lam * (1.0f - noiseAmp[i]) + sigA * u1);
                noiseEps[i] = juce::jlimit(-kNoiseEpsMax, kNoiseEpsMax,
                    (1.0f - lam) * noiseEps[i] + sigF * u2);
            }
        }

        // Promote staged coefficients to active
        voiceCoeffGrid.promoteStaged();

        // LIFE: refresh doublet ratios at the cycle boundary (live knob response)
        updateLifeRatios(lifeAmount);

        // Cache brilliance value at cycle boundaries
        cachedVelocityBrilliance = brilliance;

        // F5 Density: cache the carrier morph at the boundary too. Updating here is
        // click-free - the carrier output is exactly 0 at the boundary (both sine and
        // soft-saw LUTs are 0 at theta=0), so a step in the morph amount is inaudible.
        cachedCarrierMorph = carrierMorph;



    }

    // Use cached velocity-influenced brilliance (already calculated at cycle boundaries - line 292)
    // Note: cachedVelocityBrilliance already includes base brilliance + velocity offset
    float velocityBrilliance = cachedVelocityBrilliance;

    // Apply phase distortion using velocity-influenced brilliance and pre-calculated spectral multipliers.
    // additiveOut accumulates any drawbars routed direct-to-output (additive partials), summed with
    // the carrier below and sharing the same amp-envelope / velocity scaling.
    double additiveOut = 0.0;
    auto distortedPhase = applyPhaseDistortion(normalizedPhase, velocityBrilliance, globalParams, additiveOut);
    // Apply user-controllable velocity to amplitude scaling
  // Use cached value instead of recalculating every sample
    float velocityAmplitude = cachedVelocityAmplitude;

    // Carrier lookup, with the F5 Density morph sine -> soft-saw. morph=0 takes the
    // pure-sine fast path (bit-identical to the old engine, so patches are unchanged).
    // The morph touches ONLY the carrier - the additive partials (additiveOut) stay pure
    // sines by design (Gerard, 2 Jul 2026).
    const auto& sineLUT = SineLUT::getInstance();
    float m = cachedCarrierMorph;
    float carrierSample = (m <= 0.0f)
        ? sineLUT.lookup(distortedPhase)
        : (1.0f - m) * sineLUT.lookup(distortedPhase)
          + m * SoftSawLUT::getInstance().lookup(distortedPhase);
    auto sample = (carrierSample + (float)additiveOut) * ampEnvValue * velocityAmplitude;


    return sample;
}

//==============================================================================
// HELPER METHODS
//==============================================================================

bool PLANETVoice::shouldBeRemoved() const
{
    return ampEnvStage == EnvelopeStage::Idle;
}

// Simplified phase distortion function - direct spectral multiplier usage
double PLANETVoice::applyPhaseDistortion(double normalizedPhase, float morphAmount,
    const CoefficientArray& globalParams, double& additiveOut)
{
    constexpr double twoPi = 2.0 * juce::MathConstants<double>::pi;
    auto x = normalizedPhase * twoPi;
    auto activeCoeffs = voiceCoeffGrid.getActiveCoefficients();

    // Get sine LUT instance for fast lookup
    const auto& sineLUT = SineLUT::getInstance();

    auto distortedPhase = x;
    additiveOut = 0.0;
    const auto& softSawLUT = SoftSawLUT::getInstance();

    // Smoothstep position within the carrier cycle, shared by every noise bar: the
    // per-cycle walk targets are applied interpolated prev -> current across the cycle
    // (C1-continuous amp/rate - no boundary clicks, no staircase skirts; see header).
    const float ns = (float)(normalizedPhase * normalizedPhase * (3.0 - 2.0 * normalizedPhase));

    for (int i = 0; i < NUM_COEFFICIENTS; ++i) {
        auto k = activeCoeffs[i] * morphAmount;
        const double f = globalParams[i].inputSpectralMultiplier;

        // Per-drawbar Density (experimental): the bar's modulator morphs sine -> soft-saw,
        // so one bar contributes a whole f, 2f, 3f... family. d == 0 is the fast path and
        // is bit-identical to the pure-sine engine. Both LUTs are 0 at phase 0, so the
        // cycle-boundary snapshot keeps knob moves click-free.
        const float d = barDensity[i];
        auto modLookup = [&](double ph) -> float {
            float v = sineLUT.lookup(ph);
            if (d > 0.0f)
                v = (1.0f - d) * v + d * softSawLUT.lookup(ph);
            return v;
        };

        // modWave is the drawbar's modulator term for THIS sample (doublet-aware). It is
        // computed once, then fed to either destination per the routing snapshot - so a bar
        // can drive the carrier phase (PM), add straight to the output (additive partial at
        // harmonic f), both, or neither (muted). The phase accumulators advance regardless of
        // routing, so toggling a destination never jumps the partial's phase.
        float modWave;
        if (barNoise[i])
        {
            // Noise bar (v4 band-pass): the bar's ordinary sine, with its level and
            // rate driven by the per-cycle walks - narrowband noise centred on this
            // bar's harmonic f, width from the Density knob. A pure sine at every
            // instant, so it cannot crackle; cost = a sine bar plus two lerps.
            // PM route = phase jitter (analogue roughness), ADD route = additive
            // breath shaped by the bar's K envelope. The accumulator advances at the
            // wobbled rate; density morph and doublets don't apply on a noise bar.
            const float ampNow = noiseAmpPrev[i] + (noiseAmp[i] - noiseAmpPrev[i]) * ns;
            const float epsNow = noiseEpsPrev[i] + (noiseEps[i] - noiseEpsPrev[i]) * ns;
            modWave = ampNow * sineLUT.lookup(modPhases[i]);
            modPhases[i] += f * (1.0 + (double)epsNow) * angleDelta;
            while (modPhases[i] >= twoPi) modPhases[i] -= twoPi;
            if (doubletEngaged[i])
            {
                modPhasesB[i] += f * doubletRatioB[i] * angleDelta;
                while (modPhasesB[i] >= twoPi) modPhasesB[i] -= twoPi;
            }
        }
        else if (doubletEngaged[i])
        {
            // Stage 3 doublet: two components either side of f, energy-shared by s.
            // Their interference makes this partial beat - and because the split is a
            // fixed ratio, partial n beats n times faster than the fundamental.
            // s = 0.5 beats through a true null; away from 0.5 it beats shallower with
            // a slight pitch wobble. Both are wanted flavours.
            const float sB = doubletMixB[i];
            modWave = (1.0f - sB) * modLookup(modPhases[i])
                    + sB          * modLookup(modPhasesB[i]);
            modPhases[i]  += f * doubletRatioA[i] * angleDelta;
            modPhasesB[i] += f * doubletRatioB[i] * angleDelta;
            while (modPhases[i]  >= twoPi) modPhases[i]  -= twoPi;
            while (modPhasesB[i] >= twoPi) modPhasesB[i] -= twoPi;
        }
        else
        {
            // Single-accumulator path (Stage 1): use-then-advance, so with integer f
            // this equals sin(f*x) exactly and LIFE-off patches are unchanged.
            // angleDelta already tracks vibrato / pitch-wheel / pitch-envelope.
            modWave = modLookup(modPhases[i]);
            modPhases[i] += f * angleDelta;
            // Multipliers go up to 30, so f*angleDelta can exceed 2pi on high notes;
            // a while-wrap stays correct where a single subtraction would not.
            while (modPhases[i] >= twoPi) modPhases[i] -= twoPi;
        }

        // Phase-distortion path (routePMGain is 1 or 0). Default patches route every bar
        // here, so with routePMGain[i] == 1 this is byte-identical to the old sum.
        distortedPhase += routePMGain[i] * k * modWave;

        // Additive path: routeAddAmp[i] is the pre-clamped amplitude (0 when not routed).
        // Brilliance (morphAmount) multiplies here too, so the additive bound survives it.
        additiveOut += routeAddAmp[i] * morphAmount * modWave;
    }

    return distortedPhase;
}

float PLANETVoice::generateLFOWaveform(double phase, float shape)
{
    const auto& sineLUT = SineLUT::getInstance();
    int shapeInt = (int)shape;
    switch (shapeInt)
    {
    case 1: // Sine
        return sineLUT.lookup(phase);
    case 2: // Triangle
    {
        double normalizedPhase = phase / (2.0 * juce::MathConstants<double>::pi);
        normalizedPhase = normalizedPhase - std::floor(normalizedPhase);
        if (normalizedPhase < 0.5)
            return (float)(4.0 * normalizedPhase - 1.0);
        else
            return (float)(3.0 - 4.0 * normalizedPhase);
    }
    case 3: // Square
        return (sineLUT.lookup(phase) >= 0.0f) ? 1.0f : -1.0f;
    case 4: // Random - handled externally, this is fallback
        return 0.0f;
    default:
        return sineLUT.lookup(phase);
    }
}


// Refresh the cached envelope-curve constants. k, e^-k and (1 - e^-k) depend only on
// exponentialControl, so they're computed here (constructor / setExponentialControl) rather
// than per sample in processEnvelope - the amp envelope runs per sample and was paying a
// second std::exp for the normaliser on every sample. Values are identical to before.
void PLANETVoice::refreshEnvCurveCache()
{
    envCurveK = 1.0f + exponentialControl * 6.0f;
    envCurveEk = std::exp(-envCurveK);
    envCurveOneMinusEk = 1.0f - envCurveEk;
}

float PLANETVoice::processEnvelope(EnvelopeStage& stage, double& envTime, float& envLevel,
    double deltaTime, float attackTime, float decayTime,
    float sustainLevel, float releaseTime, bool noteOn,
    float releaseStartLevel)
{
    switch (stage)
    {
    case EnvelopeStage::Attack:
        envTime += deltaTime;
        if (envTime >= attackTime)
        {
            envLevel = 1.0f;
            envTime = 0.0;
            stage = EnvelopeStage::Decay;
        }
        else
        {
            float linearProgress = (float)(envTime / attackTime);
            // Apply exponential curve for attack (convex)
            if (exponentialControl > 0.001f) {
                // Normalised rising exponential: exactly 0.0 at progress 0, exactly 1.0 at
                // progress 1, so the attack lands on full level with no discontinuity.
                // (The old form ended at 1 - e^(-k) and snapped up to 1.0 - an audible
                // click at the attack/decay boundary, worst at low curve amounts.)
                envLevel = (1.0f - std::exp(-envCurveK * linearProgress)) / envCurveOneMinusEk;
            }
            else {
                envLevel = linearProgress;  // Linear fallback
            }
        }
        break;

    case EnvelopeStage::Decay:
        envTime += deltaTime;
        if (envTime >= decayTime)
        {
            envLevel = sustainLevel;
            envTime = 0.0;
            stage = EnvelopeStage::Sustain;
        }
        else
        {
            float linearProgress = (float)(envTime / decayTime);
            float curvedProgress;
            // Apply exponential curve for decay (concave)
            if (exponentialControl > 0.001f) {
                // Normalised decaying exponential: exactly 1.0 at progress 0, exactly 0.0 at
                // progress 1, so the curve lands cleanly on sustain with no discontinuity.
                curvedProgress = (std::exp(-envCurveK * linearProgress) - envCurveEk) / envCurveOneMinusEk;
            }
            else {
                curvedProgress = 1.0f - linearProgress;  // Linear fallback
            }
            envLevel = sustainLevel + curvedProgress * (1.0f - sustainLevel);
        }
        break;

    case EnvelopeStage::Sustain:
        envLevel = sustainLevel;
        if (!noteOn)
        {
            envTime = 0.0;
            stage = EnvelopeStage::Release;
        }
        break;

    case EnvelopeStage::Release:
        envTime += deltaTime;
        if (envTime >= releaseTime)
        {
            envLevel = 0.0f;
            stage = EnvelopeStage::Idle;
        }
        else
        {
            float linearProgress = (float)(envTime / releaseTime);
            float curvedProgress;
            // Apply exponential curve for release (concave)
            if (exponentialControl > 0.001f) {
                // Normalised decaying exponential: exactly 1.0 at progress 0, exactly 0.0 at
                // progress 1. (An earlier fast-exp approximation ended at e^(-k) - well above
                // 0 - so the tail snapped to silence when the stage flipped to Idle.
                // Normalising kills the cliff.)
                curvedProgress = (std::exp(-envCurveK * linearProgress) - envCurveEk) / envCurveOneMinusEk;
            }
            else {
                curvedProgress = 1.0f - linearProgress;  // Linear fallback
            }
            envLevel = releaseStartLevel * curvedProgress;
        }
        break;

    case EnvelopeStage::Idle:
        envLevel = 0.0f;
        break;
    }

    return envLevel;
}


float PLANETVoice::processAmplitudeEnvelope(double deltaTime, float attackTime, float decayTime,
    float sustainLevel, float releaseTime)
{
    bool noteStillHeld = noteIsActive &&
        (ampEnvStage == EnvelopeStage::Attack ||
            ampEnvStage == EnvelopeStage::Decay ||
            ampEnvStage == EnvelopeStage::Sustain);

    return processEnvelope(ampEnvStage, ampEnvTime, ampEnvLevel,
        deltaTime, attackTime, decayTime, sustainLevel, releaseTime,
        noteStillHeld, ampReleaseStartLevel);
}

// Pitch modulation stuff:
void PLANETVoice::setPitchOffset(float offsetInSemitones)
{
    totalPitchOffset = offsetInSemitones;
}

void PLANETVoice::addPitchOffset(float offsetInSemitones)
{
    totalPitchOffset += offsetInSemitones;
}

void PLANETVoice::resetPitchOffset()
{
    totalPitchOffset = 0.0f;
}

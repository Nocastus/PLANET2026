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
    // Initialize LFO phases
    for (auto& phase : lfoPhases) {
        phase = 0.0;
    }

    // Initialize modulation states
    for (auto& modState : coeffModStates) {
        modState.envStage = EnvelopeStage::Idle;
        modState.envTime = 0.0;
        modState.envLevel = 0.0f;
        modState.lfoPhase = 0.0;
        modState.lfoPhaseDelta = 0.0;
    }

    // Routing defaults until the first cycle-boundary snapshot: all bars feed the phase
    // path (classic PM), none go additive - matches the "first cycle is a pure sine"
    // start behaviour (coefficient grid is zero until the first promotion anyway).
    routePMGain.fill(1.0f);
    routeAddAmp.fill(0.0f);
}

//==============================================================================
// VOICE CONTROL METHODS
//==============================================================================

void PLANETVoice::startNote(int noteNumber, float velocity, double sampleRate, float vintageAmount,
    float velToAmplitude, float brilliance, float lifeAmount, int lifeSeed)
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

currentFrequency = basePitchFrequency;
angleDelta = currentFrequency * 2.0 * juce::MathConstants<double>::pi / sampleRate;

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

    for (auto& modState : coeffModStates) {
        modState.envStage = EnvelopeStage::Attack;
        modState.envTime = 0.0;
        modState.envLevel = 0.0f;
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

    // Always clear the note number so findVoiceForNote() won't find this voice
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
    float vibratoRate, float vibratoDepth, float vibratoFadeIn,
    float velToAmplitude, float velToAttackTime, float vintageAmount,
    float pitchEnvDistance, float pitchAttackTime,
    float lifeAmount,
    double bpm, double beatPosition)
{
    auto twoPi = 2.0 * juce::MathConstants<double>::pi;
    cycleStartFlag = false;

    // Calculate timing
    double sampleDeltaTime = 1.0 / sampleRate;           // For amplitude envelope (per-sample)
    double cycleDeltaTime = 1.0 / currentFrequency;      // For coefficient envelopes (per-cycle)



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
                    coeffModStates[i].releaseStartLevel, exponentialControl);

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
                // Attack = delay (LFO silent), Decay = fade-in, Sustain+ = full LFO
                float lfoScale = 1.0f;
                if (coeffModStates[i].envStage == EnvelopeStage::Attack) {
                    // During attack: LFO is silent (delay period)
                    lfoScale = 0.0f;
                }
                else if (coeffModStates[i].envStage == EnvelopeStage::Decay) {
                    // During decay: fade in LFO from 0 to 1 with concave exponential curve (slower at start)
                    // envTime tracks progress through decay stage
                    float linearProgress = (float)(coeffModStates[i].envTime / globalParams[i].decayTime);
                    linearProgress = juce::jlimit(0.0f, 1.0f, linearProgress);
                    // Apply concave exponential curve: y = x^2 (slower start, faster end)
                    lfoScale = linearProgress * linearProgress;
                }
                // During Sustain, Release, and Idle: full LFO (lfoScale remains 1.0f)

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
    for (int i = 0; i < NUM_COEFFICIENTS; ++i) {
        auto k = activeCoeffs[i] * morphAmount;
        const double f = globalParams[i].inputSpectralMultiplier;

        // modWave is the drawbar's sin term for THIS sample (doublet-aware). It is computed
        // once, then fed to either destination per the routing snapshot - so a bar can drive
        // the carrier phase (PM), add straight to the output (additive partial at harmonic f),
        // both, or neither (muted). The phase accumulators advance regardless of routing, so
        // toggling a destination never jumps the partial's phase.
        float modWave;
        if (doubletEngaged[i])
        {
            // Stage 3 doublet: two components either side of f, energy-shared by s.
            // Their interference makes this partial beat - and because the split is a
            // fixed ratio, partial n beats n times faster than the fundamental.
            // s = 0.5 beats through a true null; away from 0.5 it beats shallower with
            // a slight pitch wobble. Both are wanted flavours.
            const float sB = doubletMixB[i];
            modWave = (1.0f - sB) * sineLUT.lookup(modPhases[i])
                    + sB          * sineLUT.lookup(modPhasesB[i]);
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
            modWave = sineLUT.lookup(modPhases[i]);
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


float PLANETVoice::processEnvelope(EnvelopeStage& stage, double& envTime, float& envLevel,
    double deltaTime, float attackTime, float decayTime,
    float sustainLevel, float releaseTime, bool noteOn,
    float releaseStartLevel, float curveAmount)
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
            if (curveAmount > 0.001f) {
                float k = 1.0f + curveAmount * 6.0f;
                // Normalised rising exponential: exactly 0.0 at progress 0, exactly 1.0 at
                // progress 1, so the attack lands on full level with no discontinuity.
                // (The old form ended at 1 - e^(-k) and snapped up to 1.0 - an audible
                // click at the attack/decay boundary, worst at low curve amounts.)
                float ek = std::exp(-k);
                envLevel = (1.0f - std::exp(-k * linearProgress)) / (1.0f - ek);
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
            if (curveAmount > 0.001f) {
                float k = 1.0f + curveAmount * 6.0f;
                // Normalised decaying exponential: exactly 1.0 at progress 0, exactly 0.0 at
                // progress 1, so the curve lands cleanly on sustain with no discontinuity.
                float ek = std::exp(-k);
                curvedProgress = (std::exp(-k * linearProgress) - ek) / (1.0f - ek);
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
            if (curveAmount > 0.001f) {
                float k = 1.0f + curveAmount * 6.0f;
                // Normalised decaying exponential: exactly 1.0 at progress 0, exactly 0.0 at
                // progress 1. The old fastExpDecay(k*progress) ended at e^(-k) (well above 0,
                // and the Padé approximation actually rises again for k > ~4.4), so the tail
                // snapped to silence when the stage flipped to Idle. Normalising kills the cliff.
                float ek = std::exp(-k);
                curvedProgress = (std::exp(-k * linearProgress) - ek) / (1.0f - ek);
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
        noteStillHeld, ampReleaseStartLevel, exponentialControl);  // Pass hardcoded value
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

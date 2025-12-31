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
}

//==============================================================================
// VOICE CONTROL METHODS
//==============================================================================

void PLANETVoice::startNote(int noteNumber, float velocity, double sampleRate, float vintageAmount,
    float velToAmplitude, float velToBrilliance, float brilliance)
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

    // NEW: Add these lines just before the closing brace }
    cachedVelocityAmplitude = std::pow(velocity, velToAmplitude / 100.0f);
    // Cache only the velocity scaling component (once per note)
    // cachedVelocityBrillianceOffset = (velocity - 0.63f) * (velToBrilliance / 100.0f);

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
    float brilliance, double sampleRate,
    float pitchWheelOffset,
    float vibratoRate, float vibratoDepth, float vibratoFadeIn,
    float velToAmplitude, float velToBrilliance, float velToAttackTime, float vintageAmount,
    float pitchEnvDistance, float pitchAttackTime)
{
    auto twoPi = 2.0 * juce::MathConstants<double>::pi;

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

        // Calculate vibrato contribution
        float vibratoLFOValue = std::sin(vibratoState.lfoPhase);
        float effectiveVibratoDepth = vibratoDepth * vibratoState.fadeInLevel * modWheelScale;
        float vibratoOffset = vibratoLFOValue * effectiveVibratoDepth;

        // ======================== PITCH ATTACK ENVELOPE ========================
        float pitchEnvValue = processEnvelope(pitchEnvStage, pitchEnvTime, pitchEnvLevel, cycleDeltaTime,
            pitchAttackTime, 0.1f, 1.0f, 0.1f,
            noteIsActive, 0.0f, 0.5f);  // Attack to correct pitch, then stay there

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

        // Advance K coefficient LFO phases (using same logic as vibrato)
        for (int i = 0; i < NUM_COEFFICIENTS; ++i) {
            if (globalParams[i].lfoAmount != 0.0f) {
                double lfoPhaseAdvance = 2.0 * juce::MathConstants<double>::pi * globalParams[i].lfoRate * cycleDeltaTime;
                coeffModStates[i].lfoPhase += lfoPhaseAdvance;
                if (coeffModStates[i].lfoPhase >= twoPi)
                    coeffModStates[i].lfoPhase -= twoPi;
            }
        }



        // Process coefficient envelope generators
        float envValues[NUM_COEFFICIENTS];
        for (int i = 0; i < NUM_COEFFICIENTS; ++i) {
            envValues[i] = processEnvelope(coeffModStates[i].envStage, coeffModStates[i].envTime,
                coeffModStates[i].envLevel, cycleDeltaTime,
                globalParams[i].attackTime, globalParams[i].decayTime,
                globalParams[i].sustainLevel, globalParams[i].releaseTime,
                ampEnvStage != EnvelopeStage::Release && ampEnvStage != EnvelopeStage::Idle,
                coeffModStates[i].releaseStartLevel, exponentialControl);  // Add hardcoded value
        }

        // Apply modulation to coefficients
        float lfoValues[NUM_COEFFICIENTS];
        for (int i = 0; i < NUM_COEFFICIENTS; ++i) {
            lfoValues[i] = generateLFOWaveform(coeffModStates[i].lfoPhase, globalParams[i].lfoShape);
        }

        // Calculate final modulated coefficients
        auto& stagedCoeffs = voiceCoeffGrid.getStagedCoefficients();
        for (int i = 0; i < NUM_COEFFICIENTS; ++i) {
            float finalCoeff = globalParams[i].coefficient;

            // Calculate envelope modulation if envelope amount is non-zero
            if (globalParams[i].envelopeAmount != 0.0f) {
                float envValue = processEnvelope(coeffModStates[i].envStage, coeffModStates[i].envTime,
                    coeffModStates[i].envLevel, cycleDeltaTime,
                    globalParams[i].attackTime * (1.0f - (noteVelocity * velToAttackTime / 50.0f)), globalParams[i].decayTime,
                    globalParams[i].sustainLevel, globalParams[i].releaseTime,
                    ampEnvStage != EnvelopeStage::Release && ampEnvStage != EnvelopeStage::Idle,
                    coeffModStates[i].releaseStartLevel, exponentialControl);

                finalCoeff += envValue * globalParams[i].envelopeAmount;
            }

            // Calculate LFO modulation with envelope-based fade-in
            if (globalParams[i].lfoAmount != 0.0f) {
                float lfoValue = generateLFOWaveform(coeffModStates[i].lfoPhase, globalParams[i].lfoShape);

                // Scale LFO amount based on envelope stage for fade-in effect
                float lfoScale = 1.0f;
                if (coeffModStates[i].envStage == EnvelopeStage::Attack) {
                    // During attack: scale from 0 to 1
                    lfoScale = coeffModStates[i].envLevel;
                }
                else if (coeffModStates[i].envStage == EnvelopeStage::Decay) {
                    // During decay: scale from 1 down to sustainLevel
                    // We want LFO to keep fading in, so use the envelope level directly
                    lfoScale = coeffModStates[i].envLevel;
                }
                // During Sustain, Release, and Idle: full LFO (lfoScale remains 1.0f)

                finalCoeff += lfoValue * globalParams[i].lfoAmount * lfoScale;
            }

            stagedCoeffs[i] = finalCoeff;
        }

        // Promote staged coefficients to active
        voiceCoeffGrid.promoteStaged();

        // Update velocity-influenced brilliance (per-cycle for responsive control)
        cachedVelocityBrilliance = juce::jlimit(0.0f, 1.0f,
            brilliance + (noteVelocity - 0.63f) * (velToBrilliance / 100.0f));



    }

    // Use cached velocity-influenced brilliance (calculated once per note)
    float velocityBrilliance = juce::jlimit(0.0f, 1.0f, brilliance + cachedVelocityBrilliance);

    // Apply phase distortion using velocity-influenced brilliance and pre-calculated spectral multipliers
    auto distortedPhase = applyPhaseDistortion(normalizedPhase, velocityBrilliance, globalParams);
    // Apply user-controllable velocity to amplitude scaling
  // Use cached value instead of recalculating every sample  
    float velocityAmplitude = cachedVelocityAmplitude;
    auto sample = (float)std::sin(distortedPhase) * ampEnvValue * velocityAmplitude;

    // Debug
    debugLastVelocity = noteVelocity;
    debugLastBrilliance = brilliance;
    debugLastVelocityBrilliance = velocityBrilliance;

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
    const CoefficientArray& globalParams)
{
    auto x = normalizedPhase * 2.0 * juce::MathConstants<double>::pi;
    auto activeCoeffs = voiceCoeffGrid.getActiveCoefficients();

    auto distortedPhase = x;
    for (int i = 0; i < NUM_COEFFICIENTS; ++i) {
        auto k = activeCoeffs[i] * morphAmount;
        auto f = globalParams[i].inputSpectralMultiplier;  // Direct usage - no LFO needed
        distortedPhase += k * std::sin(f * x);
    }

    return distortedPhase;
}

float PLANETVoice::generateLFOWaveform(double phase, float shape)
{
    int shapeInt = (int)shape;
    switch (shapeInt)
    {
    case 1: // Sine
        return std::sin(phase);
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
        return (std::sin(phase) >= 0.0f) ? 1.0f : -1.0f;
    default:
        return std::sin(phase);
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
                float curveFactor = 1.0f + curveAmount * 6.0f;
                envLevel = 1.0f - std::exp(-curveFactor * linearProgress);
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
                float curveFactor = 1.0f + curveAmount * 6.0f;
                curvedProgress = std::exp(-curveFactor * linearProgress);
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
                float curveFactor = 1.0f + curveAmount * 6.0f;
                curvedProgress = std::exp(-curveFactor * linearProgress);
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

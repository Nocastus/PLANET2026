/*
  ==============================================================================
    PLANETEffects.cpp - Modular Effects Processing Implementation
  ==============================================================================
*/

#include "PLANETEffects.h"

//==============================================================================
// CONSTRUCTOR
//==============================================================================

PLANETEffects::PLANETEffects()
{
    // Initialize detune buffer and read positions (mid splice corridor behind write head)
    detuneProcessor.buffer.fill(0.0f);
    detuneProcessor.leftReadOffset = (float)DetuneProcessor::BUFFER_SIZE
                                   - DetuneProcessor::BASE_DELAY_SAMPLES;
    detuneProcessor.rightReadOffset = detuneProcessor.leftReadOffset;
}

//==============================================================================
// SETUP
//==============================================================================

void PLANETEffects::prepareToPlay(double sampleRate)
{
    currentSampleRate = sampleRate;
    warmthProcessor.prepareToPlay(sampleRate);
    punchProcessor.prepareToPlay(sampleRate);
}


//==============================================================================
// PARAMETER UPDATES
//==============================================================================

void PLANETEffects::updateDetuneParams(float amount, float mix)
{
    detuneProcessor.updateParameters(amount, mix, currentSampleRate);
}

void PLANETEffects::updateWarmthParams(float amount)
{
    warmthProcessor.updateParameters(amount, currentSampleRate);
}

void PLANETEffects::updatePunchParams(float amount, float frequency)
{
    punchProcessor.updateParameters(amount, frequency, currentSampleRate);
}


//==============================================================================
// MAIN PROCESSING
//==============================================================================

std::pair<float, float> PLANETEffects::processStereoSample(float monoInput)
{
    // Process through warmth (mono)
    float warmedSignal = warmthProcessor.process(monoInput);

    // Process through punch (mono)
    float punchedSignal = punchProcessor.process(warmedSignal);

    // Process through detune effect (mono to stereo)
    auto detuneOutput = detuneProcessor.process(punchedSignal);

    return detuneOutput;
}

//==============================================================================
// DUAL DETUNE PITCH SHIFTING IMPLEMENTATION
//==============================================================================

void PLANETEffects::DetuneProcessor::updateParameters(float amount, float mixAmount, double sampleRate)
{
    float mix = juce::jlimit(0.0f, 1.0f, mixAmount);

    // Equal-power crossfade gain TARGETS (mix is block-rate); process() glides the live
    // gains toward these per sample, de-zippering Mix moves. ~10 ms time constant: long
    // enough to bury block-rate steps, short enough to feel immediate under the hand.
    targetDryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
    targetWetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
    gainSmoothCoeff = 1.0f - std::exp(-1.0f / (0.010f * (float)sampleRate));

    // Convert amount (0-1) to cents (0-20)
    float detuneCents = juce::jlimit(0.0f, 1.0f, amount) * MAX_DETUNE_CENTS;

    // Calculate playback rates for pitch shifting
    leftPlaybackRate = centsToRatio(detuneCents);    // Sharp (higher pitch)
    rightPlaybackRate = centsToRatio(-detuneCents);  // Flat (lower pitch)
}

std::pair<float, float> PLANETEffects::DetuneProcessor::process(float input)
{
    // Write input to buffer
    buffer[writeIndex] = input;

    // Read at different playback rates for pitch shifting (splicing near the write head)
    float leftWet = processTap(leftReadOffset, leftPlaybackRate,
                               leftFadeRemaining, leftFadeOldOffset, leftSearch);
    float rightWet = processTap(rightReadOffset, rightPlaybackRate,
                                rightFadeRemaining, rightFadeOldOffset, rightSearch);

    // One-pole smoothing on the wet taps. Wraps are handled by the splice crossfade
    // now, but this stays: it is part of the approved wet tone (slight HF rounding).
    leftWet = leftPrevSample * (1.0f - SMOOTHING_FACTOR) + leftWet * SMOOTHING_FACTOR;
    rightWet = rightPrevSample * (1.0f - SMOOTHING_FACTOR) + rightWet * SMOOTHING_FACTOR;

    leftPrevSample = leftWet;
    rightPrevSample = rightWet;

    // Equal power crossfade. Glide the gains toward their block-rate targets (one-pole,
    // ~10 ms) so Mix moves are click-free; at rest this settles to exactly the old values.
    dryGain += gainSmoothCoeff * (targetDryGain - dryGain);
    wetGain += gainSmoothCoeff * (targetWetGain - wetGain);

    float leftOutput = input * dryGain + leftWet * wetGain;
    float rightOutput = input * dryGain + rightWet * wetGain;

    // Advance write index normally
    writeIndex = (writeIndex + 1) % BUFFER_SIZE;

    return { leftOutput, rightOutput };
}

float PLANETEffects::DetuneProcessor::processTap(float& readOffset, float playbackRate,
                                                 int& fadeRemaining, float& fadeOldOffset,
                                                 SpliceSearch& search)
{
    // How far this tap trails the write head (0..BUFFER_SIZE)
    float delay = (float)writeIndex - readOffset;
    while (delay < 0.0f) delay += (float)BUFFER_SIZE;

    if (fadeRemaining == 0 && playbackRate != 1.0f)
    {
        // Splice when the tap reaches a corridor edge, on our terms. The fast (sharp)
        // tap closes on the write head, so it jumps BACK; the slow (flat) tap falls
        // behind, so it jumps FORWARD. At rate exactly 1.0 (Spread 0) there is no
        // drift, so no searches and no splices - this whole block is skipped.
        const bool fast = playbackRate > 1.0f;
        const float drift = fast ? playbackRate - 1.0f : 1.0f - playbackRate;

        // v4 trigger geometry: the fast tap splices at the corridor low edge; the
        // slow tap splices as soon as its delay exceeds landing-floor + planned jump
        // (unknowable before its search finishes, so the pre-Done trigger is the
        // corridor-top backstop). Both taps therefore hug the low-lag end of the
        // corridor and the average wet lag matches the pre-splice-fix feel.
        // An early slow splice must clear the correlation gate; otherwise the tap
        // keeps drifting (re-searching as longer jumps become measurable) until
        // either a long jump aligns the whole mix or the corridor-top backstop.
        const bool earlyOK = search.stage == SpliceSearch::Done
                          && search.bestCorr >= SPLICE_EARLY_CORR;
        const float earliestTrigger = fast
            ? (float)SPLICE_MARGIN
            : (float)(SPLICE_MARGIN + SPLICE_TRIGGER_SLACK + SPLICE_SEARCH_MIN);
        const float triggerDelay = fast
            ? (float)SPLICE_MARGIN
            : (earlyOK ? juce::jmin((float)SPLICE_CORRIDOR_HIGH,
                                    (float)(SPLICE_MARGIN + SPLICE_TRIGGER_SLACK + search.bestJump))
                       : (float)SPLICE_CORRIDOR_HIGH);
        const float toEarliest = fast ? delay - earliestTrigger : earliestTrigger - delay;
        const float toTrigger  = fast ? delay - triggerDelay   : triggerDelay - delay;

        // Longest candidate whose comparison window is measurable from this anchor:
        // unlimited for the fast tap (it jumps into the past), delay-limited for the
        // slow tap (it jumps toward the write head).
        const int maxValid = fast ? SPLICE_SEARCH_MAX
                                  : (int)delay - SPLICE_VALID_FLOOR;

        // Sliced search: one candidate per sample, started (SEARCH_SAMPLES + SLACK)
        // samples of drift ahead of the EARLIEST possible trigger. Drift cancels out
        // of the lead, so the search finishes ~SLACK samples early at any Spread.
        if (search.stage == SpliceSearch::Idle)
        {
            if (toEarliest < drift * (float)(SPLICE_SEARCH_SAMPLES + SPLICE_SEARCH_SLACK))
                beginSearch(search, (int)readOffset, fast, maxValid);
        }
        else if (search.stage != SpliceSearch::Done)
        {
            stepSearch(search);
        }
        else if (++search.doneAge > 4 * SPLICE_SEARCH_SAMPLES && toTrigger > 0.0f)
        {
            // Approach stalled or gated: re-anchor and search again - the tap has
            // drifted, so longer (possibly whole-mix-aligning) jumps are now
            // measurable and the audio itself has moved on.
            beginSearch(search, (int)readOffset, fast, maxValid);
        }

        if (toTrigger <= 0.0f)
        {
            // Past the corridor edge. Normally the search finished samples ago; if a
            // parameter jump outran it, defer the splice (the corridor has slack) and
            // only force-finish once the tap is at the hard floor.
            if (search.stage != SpliceSearch::Done)
            {
                const bool atFloor = fast
                    ? (delay < (float)SPLICE_HARD_FLOOR)
                    : (delay > (float)(SPLICE_CORRIDOR_HIGH + SPLICE_MARGIN - SPLICE_HARD_FLOOR));
                if (atFloor)
                    finishSearch(search);
            }
            if (search.stage == SpliceSearch::Done)
            {
                fadeOldOffset = readOffset;
                readOffset += fast ? -(float)search.bestJump : (float)search.bestJump;
                while (readOffset < 0.0f) readOffset += (float)BUFFER_SIZE;
                while (readOffset >= (float)BUFFER_SIZE) readOffset -= (float)BUFFER_SIZE;
                fadeRemaining = SPLICE_FADE_SAMPLES;
                search.stage = SpliceSearch::Idle;
            }
        }
    }

    float wet;
    if (fadeRemaining > 0)
    {
        // Linear crossfade retiring -> landed trajectory. The correlation-picked jump
        // means the two are near-identical in phase, so a linear fade (not equal-power)
        // sums without a level bump. Outside fades this branch never runs and the tap
        // is exactly the pre-fix single interpolated read.
        float fadeIn = 1.0f - (float)fadeRemaining / (float)SPLICE_FADE_SAMPLES;
        wet = interpolatedRead(fadeOldOffset) * (1.0f - fadeIn)
            + interpolatedRead(readOffset) * fadeIn;
        fadeOldOffset += playbackRate;
        --fadeRemaining;
    }
    else
    {
        wet = interpolatedRead(readOffset);
    }

    readOffset += playbackRate;
    while (readOffset >= (float)BUFFER_SIZE) readOffset -= (float)BUFFER_SIZE;
    while (readOffset < 0.0f) readOffset += (float)BUFFER_SIZE;

    return wet;
}

// ---- Sliced splice-jump search ----
// Picks the jump distance whose landing point best continues the waveform at the
// anchor: normalised cross-correlation over the SPLICE_CORR_WINDOW samples BEHIND
// each candidate (always valid history for the corridor ranges). For periodic
// material the winner is a whole number of periods, so even a bass note splices in
// phase. Coarse pass at 1/4 resolution in both jump and window (phase lives in the
// fundamental, and the correlation lobe of any period >= 16 samples is far wider
// than the step), then a full-resolution refine around the coarse winner. The work
// arrives one stepSearch() call - ONE candidate - per sample, so nothing here ever
// spikes a block; see the corridor comment block in the header.

void PLANETEffects::DetuneProcessor::beginSearch(SpliceSearch& s, int basePos, bool jumpBack,
                                                 int maxJump) const
{
    for (int i = 0; i < SPLICE_CORR_WINDOW; ++i)
        s.x[i] = buffer[(basePos - i) & (BUFFER_SIZE - 1)];
    s.anchor = basePos;
    s.jumpBack = jumpBack;
    s.maxJump = juce::jlimit(SPLICE_SEARCH_MIN, SPLICE_SEARCH_MAX, maxJump);
    s.nextJump = SPLICE_SEARCH_MIN;
    s.bestJump = SPLICE_SEARCH_MIN;
    s.bestScore = -1.0e9f;
    s.bestCorr = -1.0f;
    s.doneAge = 0;
    s.stage = SpliceSearch::Coarse;
}

void PLANETEffects::DetuneProcessor::stepSearch(SpliceSearch& s) const
{
    // Small preference for the SHORTEST jump: among a periodic note's near-equal
    // period multiples this picks one period, keeping the taps at low lag (v4 -
    // longer average lag read as a stronger, wobblier Spread). On noise/silence
    // (flat scores) it stops the jump being arbitrary.
    auto shortBias = [](int jump) -> float
    {
        return SPLICE_SHORT_BIAS
             * (1.0f - (float)(jump - SPLICE_SEARCH_MIN)
                     / (float)(SPLICE_SEARCH_MAX - SPLICE_SEARCH_MIN));
    };

    if (s.stage == SpliceSearch::Coarse)
    {
        const float corr = scoreJump(s, s.nextJump, 4);
        const float sc = corr + shortBias(s.nextJump);
        if (sc > s.bestScore) { s.bestScore = sc; s.bestJump = s.nextJump; s.bestCorr = corr; }
        s.nextJump += 4;
        if (s.nextJump > s.maxJump)
        {
            // Refine +-4 around the coarse winner at full resolution. The range
            // includes the winner itself, so resetting bestScore is safe.
            s.refineHi = juce::jmin(s.maxJump, s.bestJump + 4);
            s.nextJump = juce::jmax(SPLICE_SEARCH_MIN, s.bestJump - 4);
            s.bestScore = -1.0e9f;
            s.stage = SpliceSearch::Refine;
        }
    }
    else if (s.stage == SpliceSearch::Refine)
    {
        const float corr = scoreJump(s, s.nextJump, 1);
        const float sc = corr + shortBias(s.nextJump);
        if (sc > s.bestScore) { s.bestScore = sc; s.bestJump = s.nextJump; s.bestCorr = corr; }
        if (++s.nextJump > s.refineHi)
        {
            s.stage = SpliceSearch::Done;
            s.doneAge = 0;
        }
    }
}

void PLANETEffects::DetuneProcessor::finishSearch(SpliceSearch& s) const
{
    // Automation guard only: a Spread jump outran the sliced schedule. Bounded by
    // the candidates still unscored.
    while (s.stage == SpliceSearch::Coarse || s.stage == SpliceSearch::Refine)
        stepSearch(s);
}

float PLANETEffects::DetuneProcessor::scoreJump(const SpliceSearch& s, int jump, int step) const
{
    const int candPos = s.anchor + (s.jumpBack ? -jump : jump);
    float dot = 0.0f, xEnergy = 0.0f, yEnergy = 0.0f;
    for (int i = 0; i < SPLICE_CORR_WINDOW; i += step)
    {
        const float xs = s.x[i];
        const float y = buffer[(candPos - i) & (BUFFER_SIZE - 1)];
        dot += xs * y;
        xEnergy += xs * xs;
        yEnergy += y * y;
    }
    // Raw normalised correlation; the short-jump bias is added in stepSearch so the
    // winner's raw value survives for the early-splice gate.
    return dot / std::sqrt(xEnergy * yEnergy + 1.0e-12f);
}

float PLANETEffects::DetuneProcessor::centsToRatio(float cents)
{
    // Convert cents to frequency ratio: 2^(cents/1200)
    return std::pow(2.0f, cents / 1200.0f);
}

float PLANETEffects::DetuneProcessor::interpolatedRead(float readPosition)
{
    // Ensure readPosition is in valid range
    while (readPosition < 0) readPosition += BUFFER_SIZE;
    while (readPosition >= BUFFER_SIZE) readPosition -= BUFFER_SIZE;

    // Linear interpolation between samples
    int index1 = static_cast<int>(readPosition) % BUFFER_SIZE;
    int index2 = (index1 + 1) % BUFFER_SIZE;
    float fraction = readPosition - std::floor(readPosition);

    return buffer[index1] * (1.0f - fraction) + buffer[index2] * fraction;
}

//==============================================================================
// WARMTH EFFECT IMPLEMENTATION
//==============================================================================

void PLANETEffects::WarmthProcessor::prepareToPlay(double sr)
{
    sampleRate = sr;
    lowShelfFilter.reset();
    toneFilter.reset();
}

void PLANETEffects::WarmthProcessor::updateParameters(float amount, double sr)
{
    warmthAmount = juce::jlimit(0.0f, 1.0f, amount);
    sampleRate = sr;

    // Low shelf at 250Hz with boost from 0dB to +10dB (the whole-range EQ behaviour, unchanged)
    float shelfGainDB = warmthAmount * 10.0f;
    lowShelfFilter.setLowShelf(sampleRate, 250.0f, 0.7f, shelfGainDB);

    // ---- Tape saturation (upper half of the knob), reworked 4 Jul 2026 ----
    // t runs 0-1 over 50-100%. EVERYTHING below is a continuous function of t that is
    // identity/zero at t=0, so there is no switch-on step anywhere (the old design jumped
    // -3dB of makeup gain at 51%). satDepth eases in as t^2 (imperceptible just past half,
    // gentle by ~70%, full at 100%); drive is capped lower (3, was 4) and a post-saturation
    // high-shelf cut darkens the top as the drive rises - the tape character that was
    // missing (the old full-band tanh with no post-filter read as buzzy).
    float t = juce::jmax(0.0f, (warmthAmount - 0.5f) * 2.0f);
    driveAmount = 1.0f + kMaxExtraDrive * t;
    biasTerm = kBias * t;                       // asymmetric bias -> even harmonics
    dcCorrection = std::tanh(biasTerm);         // remove the DC the bias introduces
    invTanhDrive = 1.0f / std::tanh(driveAmount);   // peak-normalise: input 1 -> ~1
    targetSatDepth = t * t;                     // parallel wet-mix, quadratic ease-in
    targetMakeup = juce::Decibels::decibelsToGain(kMakeupDB * t * t);
    toneFilter.setHighShelf(sampleRate, kToneFreqHz, 0.7f, kToneCutDB * t);
    gainSmoothCoeff = 1.0f - std::exp(-1.0f / (0.010f * (float)sampleRate));
}

float PLANETEffects::WarmthProcessor::process(float input)
{
    // Pre-saturation bass boost (boosting lows INTO the shaper is part of the tape flavour)
    float output = lowShelfFilter.process(input);

    // De-zipper the block-rate wet-mix / makeup moves (same 10 ms one-pole as detune Mix)
    satDepth += gainSmoothCoeff * (targetSatDepth - satDepth);
    makeupGain += gainSmoothCoeff * (targetMakeup - makeupGain);

    // Parallel tape saturation. Runs unconditionally (constant CPU at every setting -
    // fixed-budget principle); at satDepth 0 the blend passes the dry signal exactly.
    float shaped = (std::tanh(output * driveAmount + biasTerm) - dcCorrection) * invTanhDrive;
    output = (output + satDepth * (shaped - output)) * makeupGain;

    // Post-saturation tone: high-shelf cut scaling with drive (0dB = transparent at t=0)
    return toneFilter.process(output);
}

//==============================================================================
// PUNCH EFFECT IMPLEMENTATION
//==============================================================================

void PLANETEffects::PunchProcessor::prepareToPlay(double sr)
{
    sampleRate = sr;
    presenceFilter.reset();
    envelope = 0.0f;

    // Calculate time constants for 1176-style envelope follower
    attackCoeff = 1.0f - std::exp(-1.0f / (ATTACK_MS * 0.001f * sampleRate));
    releaseCoeff = 1.0f - std::exp(-1.0f / (RELEASE_MS * 0.001f * sampleRate));
}

void PLANETEffects::PunchProcessor::updateParameters(float amount, float frequency, double sr)
{
    punchAmount = juce::jlimit(0.0f, 1.0f, amount);
    punchFreq = juce::jlimit(500.0f, 5000.0f, frequency);
    sampleRate = sr;

    // High shelf for presence boost (active in second half 50-100%)
    if (punchAmount > 0.5f)
    {
        // Scale from 0 to +4dB in upper half
        float presenceAmount = (punchAmount - 0.5f) * 2.0f;
        float shelfGainDB = presenceAmount * 4.0f;
        presenceFilter.setHighShelf(sampleRate, punchFreq, 0.7f, shelfGainDB);
    }
}

float PLANETEffects::PunchProcessor::process(float input)
{
    float output = input;

    // Apply 1176 compression if punch > 0
    if (punchAmount > 0.0f)
    {
        // Scale compression intensity with punch amount (0-50% range)
        float compressionIntensity = juce::jmin(punchAmount * 2.0f, 1.0f);

        // FET-style envelope follower (peak detection)
        float inputLevel = std::abs(output);

        if (inputLevel > envelope)
            envelope += attackCoeff * (inputLevel - envelope);
        else
            envelope += releaseCoeff * (inputLevel - envelope);

        // Hard knee compression
        if (envelope > THRESHOLD)
        {
            float gainReduction = compress(envelope, THRESHOLD, RATIO);
            output *= gainReduction * (1.0f - compressionIntensity) + compressionIntensity;
        }

        // Makeup gain to compensate for compression
        output *= (1.0f + compressionIntensity * 0.5f);
    }

    // Apply presence boost in second half (50-100%)
    if (punchAmount > 0.5f)
    {
        output = presenceFilter.process(output);
    }

    return output;
}

float PLANETEffects::PunchProcessor::compress(float level, float threshold, float ratio)
{
    // Hard knee compression - classic 1176 style
    float overshoot = level - threshold;
    float compressed = threshold + overshoot / ratio;
    return compressed / level;  // Return gain reduction factor
}
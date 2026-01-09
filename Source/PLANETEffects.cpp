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
    // Initialize detune buffer and read positions
    detuneProcessor.buffer.fill(0.0f);
    detuneProcessor.leftReadOffset = 64.0f;   // Start with small delay
    detuneProcessor.rightReadOffset = 64.0f;
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
    mix = juce::jlimit(0.0f, 1.0f, mixAmount);

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

    // Read at different playback rates for pitch shifting
    float leftWet = interpolatedRead(leftReadOffset);
    float rightWet = interpolatedRead(rightReadOffset);

    // Simple smoothing to reduce buffer wrap artifacts
    leftWet = leftPrevSample * (1.0f - SMOOTHING_FACTOR) + leftWet * SMOOTHING_FACTOR;
    rightWet = rightPrevSample * (1.0f - SMOOTHING_FACTOR) + rightWet * SMOOTHING_FACTOR;

    leftPrevSample = leftWet;
    rightPrevSample = rightWet;

    // Advance read offsets at different rates
    leftReadOffset += leftPlaybackRate;
    rightReadOffset += rightPlaybackRate;

    // Wrap read positions within buffer
    while (leftReadOffset >= BUFFER_SIZE) leftReadOffset -= BUFFER_SIZE;
    while (rightReadOffset >= BUFFER_SIZE) rightReadOffset -= BUFFER_SIZE;
    while (leftReadOffset < 0) leftReadOffset += BUFFER_SIZE;
    while (rightReadOffset < 0) rightReadOffset += BUFFER_SIZE;

    // Equal power crossfade
    float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
    float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);

    float leftOutput = input * dryGain + leftWet * wetGain;
    float rightOutput = input * dryGain + rightWet * wetGain;

    // Advance write index normally
    writeIndex = (writeIndex + 1) % BUFFER_SIZE;

    return { leftOutput, rightOutput };
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
}

void PLANETEffects::WarmthProcessor::updateParameters(float amount, double sr)
{
    warmthAmount = juce::jlimit(0.0f, 1.0f, amount);
    sampleRate = sr;

    // Low shelf at 250Hz with boost from 0dB to +10dB
    float shelfGainDB = warmthAmount * 10.0f;
    lowShelfFilter.setLowShelf(sampleRate, 250.0f, 0.7f, shelfGainDB);
}

float PLANETEffects::WarmthProcessor::process(float input)
{
    // Always apply low shelf - this should now be audible!
    float output = lowShelfFilter.process(input);

    // Apply tape saturation starting from 50%
    if (warmthAmount > 0.5f)
    {
        float saturationAmount = (warmthAmount - 0.5f) * 2.0f;

        // Simple tape distortion with your 1.2 coefficient
        output = tapeDistortion(output, saturationAmount);

        // Gain compensation
        float compensationDB = -3.0f - (saturationAmount * 7.0f);
        float compensationGain = juce::Decibels::decibelsToGain(compensationDB);
        output *= compensationGain;
    }

    return output;
}

// float PLANETEffects::WarmthProcessor::process(float input)
//{
    // Always apply low shelf
 //   float output = lowShelfFilter.processSample(input);

    // Apply tape saturation starting from 50% (gradual introduction)
//    if (warmthAmount > 0.5f)
//    {
//        // Saturation amount scales from 0-1 in upper half (50-100%)
//        float saturationAmount = (warmthAmount - 0.5f) * 2.0f;

        // Pre-emphasis
        // output = preEmphasisFilter.processSample(output);

        // Tape distortion with your adjusted drive (coefficient 1.2)
  //      output = tapeDistortion(output, saturationAmount);

        // De-emphasis
        // output = deEmphasisFilter.processSample(output);

        // Gain compensation: -3dB at 51% scaling to -10dB at 100%
 //       float compensationDB = -3.0f - (saturationAmount * 7.0f);
  //      float compensationGain = juce::Decibels::decibelsToGain(compensationDB);
   //     output *= compensationGain;
  //  }

//    return output;
//}

float PLANETEffects::WarmthProcessor::tapeDistortion(float input, float drive)
{
    // Increased drive range: 1.0 - 5.0 (was 1.0 - 3.0)
    float driveAmount = 1.0f + drive * 3.0f;

    // Asymmetric bias for even harmonics (tape character)
    // Increased bias amount for more character
    float biased = input * driveAmount + 0.12f * drive;

    // Soft saturation using tanh
    float saturated = std::tanh(biased);

    // Remove DC offset from bias
    saturated -= std::tanh(0.12f * drive);

    // Compensate for gain
    return saturated / std::tanh(driveAmount);
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
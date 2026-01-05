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

 
//==============================================================================
// MAIN PROCESSING
//==============================================================================

std::pair<float, float> PLANETEffects::processStereoSample(float monoInput)
{
    // Process through warmth (mono)
    float warmedSignal = warmthProcessor.process(monoInput);

    // Process through detune effect (mono to stereo)
    auto detuneOutput = detuneProcessor.process(warmedSignal);

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

    // Initialize filters
    lowShelfFilter.reset();
    preEmphasisFilter.reset();
    deEmphasisFilter.reset();
}

void PLANETEffects::WarmthProcessor::updateParameters(float amount, double sr)
{
    warmthAmount = juce::jlimit(0.0f, 1.0f, amount);
    sampleRate = sr;

    // Low shelf coefficients at 150Hz
    // Increased gain: 0dB to +10dB across full range (was +6dB)
    float shelfGainDB = warmthAmount * 10.0f;
    auto shelfCoeffs = juce::IIRCoefficients::makeLowShelf(sampleRate, 250.0, 0.7,
        juce::Decibels::decibelsToGain(shelfGainDB));
    lowShelfFilter.setCoefficients(shelfCoeffs);

    // Pre-emphasis at 3kHz for tape character (boost before saturation)
    auto preCoeffs = juce::IIRCoefficients::makeHighShelf(sampleRate, 3000.0, 0.7,
        juce::Decibels::decibelsToGain(3.0f));
    preEmphasisFilter.setCoefficients(preCoeffs);

    // De-emphasis (inverse of pre-emphasis)
    auto deCoeffs = juce::IIRCoefficients::makeHighShelf(sampleRate, 3000.0, 0.7,
        juce::Decibels::decibelsToGain(-3.0f));
    deEmphasisFilter.setCoefficients(deCoeffs);
}

float PLANETEffects::WarmthProcessor::process(float input)
{
    // Always apply low shelf
    float output = lowShelfFilter.processSingleSampleRaw(input);

    // Apply tape saturation starting from 50% (gradual introduction)
    if (warmthAmount > 0.5f)
    {
        // Saturation amount scales from 0-1 in upper half (50-100%)
        float saturationAmount = (warmthAmount - 0.5f) * 2.0f;

        // Pre-emphasis
        output = preEmphasisFilter.processSingleSampleRaw(output);

        // Tape distortion with increased drive range (was 1.0-3.0, now 1.0-5.0)
        output = tapeDistortion(output, saturationAmount);

        // De-emphasis
        output = deEmphasisFilter.processSingleSampleRaw(output);

        // Gain compensation: -3dB at 51% scaling to -10dB at 100%
        // At saturationAmount = 0.02 (51%): compensationDB = -3dB
        // At saturationAmount = 1.0 (100%): compensationDB = -10dB
        float compensationDB = -3.0f - (saturationAmount * 7.0f);
        float compensationGain = juce::Decibels::decibelsToGain(compensationDB);
        output *= compensationGain;
    }

    return output;
}

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
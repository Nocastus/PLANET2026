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

void PLANETEffects::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
 
}

//==============================================================================
// PARAMETER UPDATES
//==============================================================================

void PLANETEffects::updateDetuneParams(float amount, float mix)
{
    detuneProcessor.updateParameters(amount, mix, currentSampleRate);
}

 
//==============================================================================
// MAIN PROCESSING
//==============================================================================

std::pair<float, float> PLANETEffects::processStereoSample(float monoInput)
{
    // Process through detune effect
    auto detuneOutput = detuneProcessor.process(monoInput);

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

 
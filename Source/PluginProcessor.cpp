/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
    // Skew for the bipolar drawbar (k1..k10) and LFO-amount controls. Symmetric about 0 so small
    // ± values get fine control; skew < 1 compresses knob/fader travel toward the centre. Tune by eye.
    constexpr float kBipolarLowSkew = 0.5f;

    // Env-depth low-value exponent (equivalent feel to the 0.5 skew above: 1/0.5 = 2). Higher = finer near 0.
    constexpr float kEnvLowExponent = 2.0f;

    // Custom NormalisableRange for the ASYMMETRIC bipolar env-depth range (min < 0 < max). Value 0 keeps
    // its natural linear position (q), and each side uses a power law so both small +ve and small -ve
    // depths get fine resolution while the extremes compress. Same min/max as before (range unchanged).
    juce::NormalisableRange<float> makeBipolarLowRange(float minV, float maxV, float exponent)
    {
        const float q = -minV / (maxV - minV);   // proportion at which value 0 sits (unchanged from linear)
        return juce::NormalisableRange<float>(
            minV, maxV,
            [q, exponent](float s, float e, float p) -> float          // normalised 0..1 -> value
            {
                if (p <= q) { float f = (q > 0.0f) ? (q - p) / q : 0.0f; return s * std::pow(f, exponent); }
                float f = (p - q) / (1.0f - q);                          return e * std::pow(f, exponent);
            },
            [q, exponent](float s, float e, float v) -> float          // value -> normalised 0..1
            {
                if (v <= 0.0f) { float f = std::pow(v / s, 1.0f / exponent); return q * (1.0f - f); }
                float f = std::pow(v / e, 1.0f / exponent);             return q + (1.0f - q) * f;
            },
            [](float s, float e, float v) -> float                     // snap to 0.01 (as the old range did)
            {
                return juce::jlimit(s, e, 0.01f * std::round(v / 0.01f));
            });
    }
}

//==============================================================================CONSTRUCTOR STARTS HERE
// Here is the Constructor (THE BIG ONE - 101 parameters!)
PLANETtest4AudioProcessor::PLANETtest4AudioProcessor()

#ifndef JucePlugin_PreferredChannelConfigurations
    : juce::AudioProcessor(juce::AudioProcessor::BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
    parameters(*this, nullptr, juce::Identifier("PLANETtest4"),
        {
            // ======================== BASIC PARAMETERS (1) ========================
            std::make_unique<juce::AudioParameterFloat>("brilliance", "Brilliance", 0.0f, 1.0f, 0.5f),

            // ======================== K1-K10 COEFFICIENT PARAMETERS (10) ========================
            std::make_unique<juce::AudioParameterFloat>("k1", "K1 Coefficient", juce::NormalisableRange<float>(-2.0f, 2.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k2", "K2 Coefficient", juce::NormalisableRange<float>(-2.0f, 2.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k3", "K3 Coefficient", juce::NormalisableRange<float>(-2.0f, 2.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k4", "K4 Coefficient", juce::NormalisableRange<float>(-2.0f, 2.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k5", "K5 Coefficient", juce::NormalisableRange<float>(-2.0f, 2.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k6", "K6 Coefficient", juce::NormalisableRange<float>(-2.0f, 2.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k7", "K7 Coefficient", juce::NormalisableRange<float>(-2.0f, 2.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k8", "K8 Coefficient", juce::NormalisableRange<float>(-2.0f, 2.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k9", "K9 Coefficient", juce::NormalisableRange<float>(-2.0f, 2.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k10", "K10 Coefficient", juce::NormalisableRange<float>(-2.0f, 2.0f, 0.0f, kBipolarLowSkew, true), 0.0f),

            // ======================== ATTACK TIME PARAMETERS (10) ========================
            std::make_unique<juce::AudioParameterFloat>("k1AttackTime", "K1 Attack Time", 0.001f, 10.0f, 0.1f),
            std::make_unique<juce::AudioParameterFloat>("k2AttackTime", "K2 Attack Time", 0.001f, 10.0f, 0.1f),
            std::make_unique<juce::AudioParameterFloat>("k3AttackTime", "K3 Attack Time", 0.001f, 10.0f, 0.1f),
            std::make_unique<juce::AudioParameterFloat>("k4AttackTime", "K4 Attack Time", 0.001f, 10.0f, 0.1f),
            std::make_unique<juce::AudioParameterFloat>("k5AttackTime", "K5 Attack Time", 0.001f, 10.0f, 0.1f),
            std::make_unique<juce::AudioParameterFloat>("k6AttackTime", "K6 Attack Time", 0.001f, 10.0f, 0.1f),
            std::make_unique<juce::AudioParameterFloat>("k7AttackTime", "K7 Attack Time", 0.001f, 10.0f, 0.1f),
            std::make_unique<juce::AudioParameterFloat>("k8AttackTime", "K8 Attack Time", 0.001f, 10.0f, 0.1f),
            std::make_unique<juce::AudioParameterFloat>("k9AttackTime", "K9 Attack Time", 0.001f, 10.0f, 0.1f),
            std::make_unique<juce::AudioParameterFloat>("k10AttackTime", "K10 Attack Time", 0.001f, 10.0f, 0.1f),

            // ======================== DECAY TIME PARAMETERS (10) ========================
            std::make_unique<juce::AudioParameterFloat>("k1DecayTime", "K1 Decay Time", 0.001f, 10.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k2DecayTime", "K2 Decay Time", 0.001f, 10.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k3DecayTime", "K3 Decay Time", 0.001f, 10.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k4DecayTime", "K4 Decay Time", 0.001f, 10.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k5DecayTime", "K5 Decay Time", 0.001f, 10.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k6DecayTime", "K6 Decay Time", 0.001f, 10.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k7DecayTime", "K7 Decay Time", 0.001f, 10.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k8DecayTime", "K8 Decay Time", 0.001f, 10.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k9DecayTime", "K9 Decay Time", 0.001f, 10.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k10DecayTime", "K10 Decay Time", 0.001f, 10.0f, 0.5f),

            // ======================== SUSTAIN LEVEL PARAMETERS (10) ========================
            std::make_unique<juce::AudioParameterFloat>("k1SustainLevel", "K1 Sustain Level", 0.0f, 2.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k2SustainLevel", "K2 Sustain Level", 0.0f, 2.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k3SustainLevel", "K3 Sustain Level", 0.0f, 2.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k4SustainLevel", "K4 Sustain Level", 0.0f, 2.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k5SustainLevel", "K5 Sustain Level", 0.0f, 2.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k6SustainLevel", "K6 Sustain Level", 0.0f, 2.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k7SustainLevel", "K7 Sustain Level", 0.0f, 2.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k8SustainLevel", "K8 Sustain Level", 0.0f, 2.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k9SustainLevel", "K9 Sustain Level", 0.0f, 2.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("k10SustainLevel", "K10 Sustain Level", 0.0f, 2.0f, 0.5f),

            // ======================== RELEASE TIME PARAMETERS (10) ========================
            std::make_unique<juce::AudioParameterFloat>("k1ReleaseTime", "K1 Release Time", 0.001f, 10.0f, 2.0f),
            std::make_unique<juce::AudioParameterFloat>("k2ReleaseTime", "K2 Release Time", 0.001f, 10.0f, 2.0f),
            std::make_unique<juce::AudioParameterFloat>("k3ReleaseTime", "K3 Release Time", 0.001f, 10.0f, 2.0f),
            std::make_unique<juce::AudioParameterFloat>("k4ReleaseTime", "K4 Release Time", 0.001f, 10.0f, 2.0f),
            std::make_unique<juce::AudioParameterFloat>("k5ReleaseTime", "K5 Release Time", 0.001f, 10.0f, 2.0f),
            std::make_unique<juce::AudioParameterFloat>("k6ReleaseTime", "K6 Release Time", 0.001f, 10.0f, 2.0f),
            std::make_unique<juce::AudioParameterFloat>("k7ReleaseTime", "K7 Release Time", 0.001f, 10.0f, 2.0f),
            std::make_unique<juce::AudioParameterFloat>("k8ReleaseTime", "K8 Release Time", 0.001f, 10.0f, 2.0f),
            std::make_unique<juce::AudioParameterFloat>("k9ReleaseTime", "K9 Release Time", 0.001f, 10.0f, 2.0f),
            std::make_unique<juce::AudioParameterFloat>("k10ReleaseTime", "K10 Release Time", 0.001f, 10.0f, 2.0f),

            // ======================== ENVELOPE AMOUNT PARAMETERS (10) ========================
            std::make_unique<juce::AudioParameterFloat>("k1EnvelopeAmount", "K1 Envelope Amount",
                makeBipolarLowRange(-5.0f, 20.0f, kEnvLowExponent), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k2EnvelopeAmount", "K2 Envelope Amount",
                makeBipolarLowRange(-5.0f, 20.0f, kEnvLowExponent), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k3EnvelopeAmount", "K3 Envelope Amount",
                makeBipolarLowRange(-5.0f, 20.0f, kEnvLowExponent), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k4EnvelopeAmount", "K4 Envelope Amount",
                makeBipolarLowRange(-5.0f, 20.0f, kEnvLowExponent), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k5EnvelopeAmount", "K5 Envelope Amount",
                makeBipolarLowRange(-5.0f, 20.0f, kEnvLowExponent), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k6EnvelopeAmount", "K6 Envelope Amount",
                makeBipolarLowRange(-5.0f, 20.0f, kEnvLowExponent), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k7EnvelopeAmount", "K7 Envelope Amount",
                makeBipolarLowRange(-5.0f, 20.0f, kEnvLowExponent), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k8EnvelopeAmount", "K8 Envelope Amount",
                makeBipolarLowRange(-5.0f, 20.0f, kEnvLowExponent), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k9EnvelopeAmount", "K9 Envelope Amount",
                makeBipolarLowRange(-5.0f, 20.0f, kEnvLowExponent), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k10EnvelopeAmount", "K10 Envelope Amount",
                makeBipolarLowRange(-5.0f, 20.0f, kEnvLowExponent), 0.0f),

            // ======================== LFO SHAPE PARAMETERS (10) ========================
            std::make_unique<juce::AudioParameterFloat>("k1LFOShape", "K1 LFO Shape", 1.0f, 4.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k2LFOShape", "K2 LFO Shape", 1.0f, 4.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k3LFOShape", "K3 LFO Shape", 1.0f, 4.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k4LFOShape", "K4 LFO Shape", 1.0f, 4.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k5LFOShape", "K5 LFO Shape", 1.0f, 4.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k6LFOShape", "K6 LFO Shape", 1.0f, 4.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k7LFOShape", "K7 LFO Shape", 1.0f, 4.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k8LFOShape", "K8 LFO Shape", 1.0f, 4.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k9LFOShape", "K9 LFO Shape", 1.0f, 4.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k10LFOShape", "K10 LFO Shape", 1.0f, 4.0f, 1.0f),

            // ======================== LFO RATE PARAMETERS (10 - EXISTING) ========================
            // Using skew factor 0.2 to give more precision in the useful 0.05-10 Hz range
            std::make_unique<juce::AudioParameterFloat>("k1LFORate", "K1 LFO Rate",
                juce::NormalisableRange<float>(0.05f, 1000.0f, 0.01f, 0.2f), 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k2LFORate", "K2 LFO Rate",
                juce::NormalisableRange<float>(0.05f, 1000.0f, 0.01f, 0.2f), 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k3LFORate", "K3 LFO Rate",
                juce::NormalisableRange<float>(0.05f, 1000.0f, 0.01f, 0.2f), 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k4LFORate", "K4 LFO Rate",
                juce::NormalisableRange<float>(0.05f, 1000.0f, 0.01f, 0.2f), 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k5LFORate", "K5 LFO Rate",
                juce::NormalisableRange<float>(0.05f, 1000.0f, 0.01f, 0.2f), 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k6LFORate", "K6 LFO Rate",
                juce::NormalisableRange<float>(0.05f, 1000.0f, 0.01f, 0.2f), 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k7LFORate", "K7 LFO Rate",
                juce::NormalisableRange<float>(0.05f, 1000.0f, 0.01f, 0.2f), 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k8LFORate", "K8 LFO Rate",
                juce::NormalisableRange<float>(0.05f, 1000.0f, 0.01f, 0.2f), 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k9LFORate", "K9 LFO Rate",
                juce::NormalisableRange<float>(0.05f, 1000.0f, 0.01f, 0.2f), 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k10LFORate", "K10 LFO Rate",
                juce::NormalisableRange<float>(0.05f, 1000.0f, 0.01f, 0.2f), 4.0f),

            // ======================== LFO AMOUNT PARAMETERS (10 - EXISTING) ========================
            std::make_unique<juce::AudioParameterFloat>("k1LFOAmount", "K1 LFO Amount", juce::NormalisableRange<float>(-5.0f, 5.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k2LFOAmount", "K2 LFO Amount", juce::NormalisableRange<float>(-5.0f, 5.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k3LFOAmount", "K3 LFO Amount", juce::NormalisableRange<float>(-5.0f, 5.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k4LFOAmount", "K4 LFO Amount", juce::NormalisableRange<float>(-5.0f, 5.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k5LFOAmount", "K5 LFO Amount", juce::NormalisableRange<float>(-5.0f, 5.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k6LFOAmount", "K6 LFO Amount", juce::NormalisableRange<float>(-5.0f, 5.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k7LFOAmount", "K7 LFO Amount", juce::NormalisableRange<float>(-5.0f, 5.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k8LFOAmount", "K8 LFO Amount", juce::NormalisableRange<float>(-5.0f, 5.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k9LFOAmount", "K9 LFO Amount", juce::NormalisableRange<float>(-5.0f, 5.0f, 0.0f, kBipolarLowSkew, true), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k10LFOAmount", "K10 LFO Amount", juce::NormalisableRange<float>(-5.0f, 5.0f, 0.0f, kBipolarLowSkew, true), 0.0f),

            // ======================== VELOCITY TO HARMONIC PARAMETERS (10 - NEW) ========================
                std::make_unique<juce::AudioParameterFloat>("k1VelToHarmonic", "K1 Vel To Harmonic", -100.0f, 100.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("k2VelToHarmonic", "K2 Vel To Harmonic", -100.0f, 100.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("k3VelToHarmonic", "K3 Vel To Harmonic", -100.0f, 100.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("k4VelToHarmonic", "K4 Vel To Harmonic", -100.0f, 100.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("k5VelToHarmonic", "K5 Vel To Harmonic", -100.0f, 100.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("k6VelToHarmonic", "K6 Vel To Harmonic", -100.0f, 100.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("k7VelToHarmonic", "K7 Vel To Harmonic", -100.0f, 100.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("k8VelToHarmonic", "K8 Vel To Harmonic", -100.0f, 100.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("k9VelToHarmonic", "K9 Vel To Harmonic", -100.0f, 100.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("k10VelToHarmonic", "K10 Vel To Harmonic", -100.0f, 100.0f, 0.0f),

            // ======================== LFO TEMPO SYNC ON/OFF (10) ========================
            std::make_unique<juce::AudioParameterFloat>("k1LFOSync", "K1 LFO Sync", 0.0f, 1.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k2LFOSync", "K2 LFO Sync", 0.0f, 1.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k3LFOSync", "K3 LFO Sync", 0.0f, 1.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k4LFOSync", "K4 LFO Sync", 0.0f, 1.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k5LFOSync", "K5 LFO Sync", 0.0f, 1.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k6LFOSync", "K6 LFO Sync", 0.0f, 1.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k7LFOSync", "K7 LFO Sync", 0.0f, 1.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k8LFOSync", "K8 LFO Sync", 0.0f, 1.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k9LFOSync", "K9 LFO Sync", 0.0f, 1.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k10LFOSync", "K10 LFO Sync", 0.0f, 1.0f, 0.0f),

            // ======================== LFO SYNC DIVISION (10) ========================
            // Index 0-12: 4/1, 2/1, 1/1, 1/2., 1/2, 1/2T, 1/4., 1/4, 1/4T, 1/8., 1/8, 1/8T, 1/16
            std::make_unique<juce::AudioParameterFloat>("k1LFOSyncDiv", "K1 LFO Sync Div",
                juce::NormalisableRange<float>(0.0f, 12.0f, 1.0f), 7.0f),
            std::make_unique<juce::AudioParameterFloat>("k2LFOSyncDiv", "K2 LFO Sync Div",
                juce::NormalisableRange<float>(0.0f, 12.0f, 1.0f), 7.0f),
            std::make_unique<juce::AudioParameterFloat>("k3LFOSyncDiv", "K3 LFO Sync Div",
                juce::NormalisableRange<float>(0.0f, 12.0f, 1.0f), 7.0f),
            std::make_unique<juce::AudioParameterFloat>("k4LFOSyncDiv", "K4 LFO Sync Div",
                juce::NormalisableRange<float>(0.0f, 12.0f, 1.0f), 7.0f),
            std::make_unique<juce::AudioParameterFloat>("k5LFOSyncDiv", "K5 LFO Sync Div",
                juce::NormalisableRange<float>(0.0f, 12.0f, 1.0f), 7.0f),
            std::make_unique<juce::AudioParameterFloat>("k6LFOSyncDiv", "K6 LFO Sync Div",
                juce::NormalisableRange<float>(0.0f, 12.0f, 1.0f), 7.0f),
            std::make_unique<juce::AudioParameterFloat>("k7LFOSyncDiv", "K7 LFO Sync Div",
                juce::NormalisableRange<float>(0.0f, 12.0f, 1.0f), 7.0f),
            std::make_unique<juce::AudioParameterFloat>("k8LFOSyncDiv", "K8 LFO Sync Div",
                juce::NormalisableRange<float>(0.0f, 12.0f, 1.0f), 7.0f),
            std::make_unique<juce::AudioParameterFloat>("k9LFOSyncDiv", "K9 LFO Sync Div",
                juce::NormalisableRange<float>(0.0f, 12.0f, 1.0f), 7.0f),
            std::make_unique<juce::AudioParameterFloat>("k10LFOSyncDiv", "K10 LFO Sync Div",
                juce::NormalisableRange<float>(0.0f, 12.0f, 1.0f), 7.0f),

            // ======================== PER-DRAWBAR ROUTING SWITCHES (F7/F8) ========================
            // Two independent destinations per drawbar. ToPM = feed the phase-distortion path
            // (classic PM operator); ToOut = add straight to the output (additive partial at
            // harmonic f). Default ToPM=1 / ToOut=0 reproduces the old engine exactly, so existing
            // patches are bit-identical. Both off = muted; both on = the bar feeds both paths.
            std::make_unique<juce::AudioParameterFloat>("k1ToPM",  "K1 -> Phase Dist",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k2ToPM",  "K2 -> Phase Dist",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k3ToPM",  "K3 -> Phase Dist",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k4ToPM",  "K4 -> Phase Dist",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k5ToPM",  "K5 -> Phase Dist",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k6ToPM",  "K6 -> Phase Dist",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k7ToPM",  "K7 -> Phase Dist",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k8ToPM",  "K8 -> Phase Dist",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k9ToPM",  "K9 -> Phase Dist",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k10ToPM", "K10 -> Phase Dist", juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f),

            std::make_unique<juce::AudioParameterFloat>("k1ToOut",  "K1 -> Direct Out",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k2ToOut",  "K2 -> Direct Out",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k3ToOut",  "K3 -> Direct Out",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k4ToOut",  "K4 -> Direct Out",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k5ToOut",  "K5 -> Direct Out",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k6ToOut",  "K6 -> Direct Out",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k7ToOut",  "K7 -> Direct Out",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k8ToOut",  "K8 -> Direct Out",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k9ToOut",  "K9 -> Direct Out",  juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k10ToOut", "K10 -> Direct Out", juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f),

            // ======================== SPECTRAL MULTIPLIER INPUT PARAMETERS (10) ========================
            std::make_unique<juce::AudioParameterFloat>("input_f1", "Input F1 Spectral Multiplier", 0.5f, 30.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("input_f2", "Input F2 Spectral Multiplier", 0.5f, 30.0f, 2.0f),
            std::make_unique<juce::AudioParameterFloat>("input_f3", "Input F3 Spectral Multiplier", 0.5f, 30.0f, 3.0f),
            std::make_unique<juce::AudioParameterFloat>("input_f4", "Input F4 Spectral Multiplier", 0.5f, 30.0f, 4.0f),
            std::make_unique<juce::AudioParameterFloat>("input_f5", "Input F5 Spectral Multiplier", 0.5f, 30.0f, 5.0f),
            std::make_unique<juce::AudioParameterFloat>("input_f6", "Input F6 Spectral Multiplier", 0.5f, 30.0f, 6.0f),
            std::make_unique<juce::AudioParameterFloat>("input_f7", "Input F7 Spectral Multiplier", 0.5f, 30.0f, 7.0f),
            std::make_unique<juce::AudioParameterFloat>("input_f8", "Input F8 Spectral Multiplier", 0.5f, 30.0f, 8.0f),
            std::make_unique<juce::AudioParameterFloat>("input_f9", "Input F9 Spectral Multiplier", 0.5f, 30.0f, 9.0f),
            std::make_unique<juce::AudioParameterFloat>("input_f10", "Input F10 Spectral Multiplier", 0.5f, 30.0f, 10.0f),

            // =========================VELOCITY SCALING PARAMETERS=========================================
            std::make_unique<juce::AudioParameterFloat>("velToAmplitude", "Vel->Amplitude", 0.0f, 200.0f, 100.0f),
        
            std::make_unique<juce::AudioParameterFloat>("velToAttackTime", "Vel->Attack Time", 0.0f, 100.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("vintageAmount", "Vintage Amount", 0.0f, 100.0f, 0.0f),

            // ======================== LIFE PARAMETERS ========================
            // LIFE: master depth for per-partial micro-motion (doublet beating / drift).
            // LIFE SEED: the "luthier seed" - deterministic per-drawbar character, saved with patch.
            std::make_unique<juce::AudioParameterFloat>("lifeAmount", "Life", 0.0f, 100.0f, 0.0f),
            std::make_unique<juce::AudioParameterInt>("lifeSeed", "Life Seed", 0, 9999, 4271),

            // ======================== VIBRATO PARAMETERS ============================================
            std::make_unique<juce::AudioParameterFloat>("vibratoRate", "Vibrato Rate", 0.5f, 12.0f, 5.0f),
            std::make_unique<juce::AudioParameterFloat>("vibratoDepth", "Vibrato Depth", 0.0f, 2.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("vibratoFadeIn", "Vibrato Fade In", 0.0f, 10.0f, 2.0f),

            // ======================== PITCH ATTACK ENVELOPE PARAMETERS ========================
            std::make_unique<juce::AudioParameterFloat>("pitchEnvDistance", "Pitch Env Distance", -12.0f, 12.0f, 0.0f),
            // Skewed (skew < 1) so the low end gets most of the knob travel - the musically
            // useful fast pitch sweeps live below ~0.5 s and need fine control. skew = 0.35
            // puts the knob centre at ~0.70 s; the bottom quarter spans 0.01-0.10 s (skew 0.25
            // was too aggressive - the first quarter wasted on an imperceptible 0.01-0.03 s).
            // Range (0.01-5.0 s) and default (0.5 s) unchanged, so existing patches are
            // unaffected (they store real seconds and reload through this range). Tune by feel.
            std::make_unique<juce::AudioParameterFloat>("pitchEnvTime", "Pitch Env Time",
                juce::NormalisableRange<float>(0.01f, 5.0f, 0.0f, 0.35f), 0.5f),

            // ======================== AMPLITUDE ENVELOPE PARAMETERS (4) ========================
            std::make_unique<juce::AudioParameterFloat>("ampEnvAttackTime", "Amp Env Attack Time", 0.001f, 10.0f, 0.1f),
            std::make_unique<juce::AudioParameterFloat>("ampEnvDecayTime", "Amp Env Decay Time", 0.001f, 10.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("ampEnvSustainLevel", "Amp Env Sustain Level", 0.0f, 1.0f, 0.7f),
            std::make_unique<juce::AudioParameterFloat>("ampEnvReleaseTime", "Amp Env Release Time", 0.001f, 10.0f, 2.0f),

            // ========================EXPONENTIAL ENVELOPE PARAMETER==========================
            std::make_unique<juce::AudioParameterFloat>("exponentialControl", "Exponential Control", 0.0f, 1.0f, 0.5f),

            // ======================== EFFECTS PARAMETERS ========================
                std::make_unique<juce::AudioParameterFloat>("detuneAmount", "Detune Amount", 0.0f, 1.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("detuneMix", "Detune Mix", 0.0f, 1.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("warmth", "Warmth", 0.0f, 1.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("punch", "Punch", 0.0f, 1.0f, 0.0f),
                std::make_unique<juce::AudioParameterFloat>("punchFrequency", "Punch Frequency", 500.0f, 5000.0f, 1800.0f),
              
            // ======================== MASTER CONTROLS ========================
                std::make_unique<juce::AudioParameterFloat>("masterVolume", "Master Volume", 0.0f, 1.0f, 0.64f),
                std::make_unique<juce::AudioParameterFloat>("transpose", "Transpose", -24.0f, 24.0f, 0.0f),

        })
#endif
{
    // ======================== INITIALIZE BASIC PARAMETER POINTERS ========================
    brillianceParameter = parameters.getRawParameterValue("brilliance");

    // ======================== INITIALIZE AMPLITUDE ENVELOPE PARAMETER POINTERS ========================
    ampEnvAttackTimeParameter = parameters.getRawParameterValue("ampEnvAttackTime");
    ampEnvDecayTimeParameter = parameters.getRawParameterValue("ampEnvDecayTime");
    ampEnvSustainLevelParameter = parameters.getRawParameterValue("ampEnvSustainLevel");
    ampEnvReleaseTimeParameter = parameters.getRawParameterValue("ampEnvReleaseTime");

    // ======================== INITIALIZE VIBRATO POINTERS ========================
    vibratoRateParameter = parameters.getRawParameterValue("vibratoRate");
    vibratoDepthParameter = parameters.getRawParameterValue("vibratoDepth");
    vibratoFadeInParameter = parameters.getRawParameterValue("vibratoFadeIn");

    // ======================== INITIALIZE PITCH ENVELOPE PARAMETER POINTERS ========================
    pitchEnvDistanceParameter = parameters.getRawParameterValue("pitchEnvDistance");
    pitchEnvTimeParameter = parameters.getRawParameterValue("pitchEnvTime");

    // ======================== INITIALIZE VELOCITY SCALING PARAMETER POINTERS ========================
    velToAmplitudeParameter = parameters.getRawParameterValue("velToAmplitude");
  
    velToAttackTimeParameter = parameters.getRawParameterValue("velToAttackTime");

    //======================== INITIALIZE VINTAGE PARAMETER POINTER ========================
    vintageAmountParameter = parameters.getRawParameterValue("vintageAmount");
    lifeAmountParameter = parameters.getRawParameterValue("lifeAmount");
    lifeSeedParameter = parameters.getRawParameterValue("lifeSeed");

    // Give voices access to the LIFE voicing dev constants
    voiceManager.setLifeVoicingParams(&lifeVoicing);

    // ======================== INITIALIZE EFFECTS PARAMETER POINTERS ========================
    detuneAmountParameter = parameters.getRawParameterValue("detuneAmount");
    detuneMixParameter = parameters.getRawParameterValue("detuneMix");
    warmthParameter = parameters.getRawParameterValue("warmth");
    punchParameter = parameters.getRawParameterValue("punch");
    punchFrequencyParameter = parameters.getRawParameterValue("punchFrequency");

    // ======================== INITIALIZE MASTER CONTROL POINTERS ========================
    masterVolumeParameter = parameters.getRawParameterValue("masterVolume");
    transposeParameter = parameters.getRawParameterValue("transpose");




    // Initialize clean array
    coefficients.initializeFromAPVTS(parameters);

    // Initialize exponential control parameter pointer
 // Initialize exponential control parameter pointer
    exponentialControlParameter = parameters.getRawParameterValue("exponentialControl");




}

PLANETtest4AudioProcessor::~PLANETtest4AudioProcessor()
{
}

//==============================================================================
const juce::String PLANETtest4AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PLANETtest4AudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool PLANETtest4AudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool PLANETtest4AudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double PLANETtest4AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PLANETtest4AudioProcessor::getNumPrograms()
{
    return 1;
}

int PLANETtest4AudioProcessor::getCurrentProgram()
{
    return 0;
}

void PLANETtest4AudioProcessor::setCurrentProgram(int index)
{
}

const juce::String PLANETtest4AudioProcessor::getProgramName(int index)
{
    return {};
}

void PLANETtest4AudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

//==============================================================================
void PLANETtest4AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    // Initialize effects
    effects.prepareToPlay(sampleRate);
    // LFO phase deltas are now calculated per-voice in PLANETVoice
}



void PLANETtest4AudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PLANETtest4AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif



//==============================================Process Block ===============================

void PLANETtest4AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Read DAW tempo and beat position for LFO sync
    bool playing = false;
    if (auto* playHead = getPlayHead()) {
        auto posInfo = playHead->getPosition();
        if (posInfo.hasValue()) {
            if (posInfo->getBpm().hasValue())
                currentBPM = *posInfo->getBpm();
            if (posInfo->getPpqPosition().hasValue())
                currentBeatPosition = *posInfo->getPpqPosition();
            playing = posInfo->getIsPlaying();
        }
    }
    // Publish transport state for the GUI's LFO-rate pulse indicators (see PLANETMainGui).
    displayBPM.store(currentBPM);
    transportPlaying.store(playing);

    // Update global parameters (shared by all voices)
    coefficients.updateAllActiveValues();
    float sliderBrilliance = brillianceParameter->load();
    float effectiveBrilliance = sliderBrilliance;  // Default to slider value

    if (modWheelEngaged.load())
    {
        float M = rawModWheelValue.load();  // 0-1, center at 0.5
        float S = sliderBrilliance;

        if (M < 0.5f)
        {
            // Below center: interpolate from 0 (at MW=0) to S (at MW=0.5)
            effectiveBrilliance = M * 2.0f * S;
        }
        else
        {
            // At/above center: interpolate from S (at MW=0.5) to 1 (at MW=1)
            effectiveBrilliance = S + (M - 0.5f) * 2.0f * (1.0f - S);
        }
    }

    // Get envelope exponential parameters
    float currentExponentialControl = exponentialControlParameter->load();
    voiceManager.setExponentialControl(currentExponentialControl);

    // Get amplitude envelope parameters
    float ampAttackTime = ampEnvAttackTimeParameter->load();
    float ampDecayTime = ampEnvDecayTimeParameter->load();
    float ampSustainLevel = ampEnvSustainLevelParameter->load();
    float ampReleaseTime = ampEnvReleaseTimeParameter->load();

    // Get velocity scaling parameters
    float velToAmplitude = velToAmplitudeParameter->load();
   
    float velToAttackTime = velToAttackTimeParameter->load();
    float vintageAmount = vintageAmountParameter->load();
    float lifeAmount = lifeAmountParameter->load();
    int lifeSeed = (int)lifeSeedParameter->load();
    float pitchEnvDistance = pitchEnvDistanceParameter->load();
    float pitchAttackTime = pitchEnvTimeParameter->load();


    // Apply a single MIDI message. Defined here, but invoked from the interleave loop
    // below at each event's true sample offset rather than all-at-once at block start.
    auto applyMidiMessage = [&](const juce::MidiMessage& message)
    {
        if (message.isNoteOn())
        {
            float velocity = message.getVelocity() / 127.0f;
            if (velocity > 0.0f)
            {
                // Apply transpose to incoming MIDI note
                int transposedNote = message.getNoteNumber() + (int)transposeParameter->load();
                transposedNote = juce::jlimit(0, 127, transposedNote);

                // Convert current pitch wheel to semitones
                float pitchWheelSemitones = currentPitchWheelValue * pitchWheelRange;

                voiceManager.startNote(transposedNote, velocity, getSampleRate(), pitchWheelSemitones, vintageAmount,
                    velToAmplitude, brillianceParameter->load(), lifeAmount, lifeSeed);
            }
            else
            {
                int transposedNote = message.getNoteNumber() + (int)transposeParameter->load();
                transposedNote = juce::jlimit(0, 127, transposedNote);
                voiceManager.stopNote(transposedNote, sustainPedalDown);
            }
        }
        else if (message.isNoteOff())
        {
            int transposedNote = message.getNoteNumber() + (int)transposeParameter->load();
            transposedNote = juce::jlimit(0, 127, transposedNote);
            voiceManager.stopNote(transposedNote, sustainPedalDown);
        }
        else if (message.isController())
        {



            // Handle sustain pedal (CC #64)
            // Handle sustain pedal (CC #64)
            if (message.getControllerNumber() == 64)
            {
                bool newSustainState = message.getControllerValue() >= 64;

                if (sustainPedalDown && !newSustainState)
                {
                    voiceManager.releaseSustainedNotes();
                }

                sustainPedalDown = newSustainState;
            }

            // Handle mod wheel (CC #1) - brilliance control with soft takeover
            else if (message.getControllerNumber() == 1)
            {
                float newRawValue = message.getControllerValue() / 127.0f;  // 0-1

                // Check for engagement (crossing through center point 0.5)
                if (!modWheelEngaged.load())
                {
                    bool crossedCenter = (lastRawModWheelValue < 0.5f && newRawValue >= 0.5f) ||
                        (lastRawModWheelValue > 0.5f && newRawValue <= 0.5f);
                    if (crossedCenter)
                    {
                        modWheelEngaged.store(true);
                    }
                }

                lastRawModWheelValue = newRawValue;
                rawModWheelValue.store(newRawValue);
            }

        }



        else if (message.isPitchWheel())
        {
            // Handle pitch wheel
            // MIDI pitch wheel range: 0-16383, center = 8192
            int wheelValue = message.getPitchWheelValue();

            // Convert to -1.0 to +1.0 range
            currentPitchWheelValue = (wheelValue - 8192) / 8192.0f;

            // Convert to semitones
            float pitchWheelSemitones = currentPitchWheelValue * pitchWheelRange;

            // Apply to all active voices
            voiceManager.setPitchWheelOffset(pitchWheelSemitones, getSampleRate());
        }
    };


    // ======================== LOAD ALL PARAMETERS ONCE PER BLOCK ========================
    float detuneAmount = detuneAmountParameter->load();
    float detuneMix = detuneMixParameter->load();
    float warmth = warmthParameter->load();
    float punch = punchParameter->load();
    float punchFrequency = punchFrequencyParameter->load();

    float vibratoRate = vibratoRateParameter->load();
    float vibratoDepth = vibratoDepthParameter->load();
    float vibratoFadeIn = vibratoFadeInParameter->load();

    // Update effects parameters once per block
    effects.updateDetuneParams(detuneAmount, detuneMix);
    effects.updateWarmthParams(warmth);
    effects.updatePunchParams(punch, punchFrequency);
 

    // ======================== GENERATE POLYPHONIC AUDIO ========================
    // Render one contiguous segment [startSample, endSample) of the block. The interleave
    // loop below calls this between MIDI events so each event lands at its true offset.
    auto renderSamples = [&](int startSample, int endSample)
    {
    for (int sample = startSample; sample < endSample; ++sample)
    {
        // Pitch wheel reflects the events applied so far this block (sub-block accurate)
        float pitchWheelSemitones = currentPitchWheelValue * pitchWheelRange;

        // Advance beat position per sample for sub-block accuracy
        double beatPositionForSample = currentBeatPosition + (sample * currentBPM / (60.0 * getSampleRate()));

        float mixedSample = voiceManager.processNextSample(coefficients,
            ampAttackTime, ampDecayTime, ampSustainLevel, ampReleaseTime,
            effectiveBrilliance, getSampleRate(),
            pitchWheelSemitones,
            vibratoRate, vibratoDepth, vibratoFadeIn,
            velToAmplitude, velToAttackTime, vintageAmount,
            pitchEnvDistance, pitchAttackTime,
            lifeAmount,
            currentBPM, beatPositionForSample);

        // Waveform snapshot capture
        if (snapshotCapturing)
        {
            waveformSnapshot[snapshotWritePos++] = voiceManager.getLastFirstVoiceSample();
            if (snapshotWritePos >= snapshotTargetLength)
            {
                snapshotCapturing = false;
                waveformSnapshotLength.store(snapshotTargetLength);
                waveformSnapshotReady.store(true);
            }
        }
        else if (waveformSnapshotRequest.load() && voiceManager.getFirstVoiceCycleStart())
        {
            // Start new capture at cycle boundary
            float freq = voiceManager.getFirstVoiceFrequency();
            int samplesPerCycle = (int)(getSampleRate() / freq);
            snapshotTargetLength = juce::jmin(samplesPerCycle * 2, WAVEFORM_SNAPSHOT_SIZE);
            snapshotWritePos = 0;
            snapshotCapturing = true;
            waveformSnapshotRequest.store(false);

            // Capture first sample
            waveformSnapshot[snapshotWritePos++] = voiceManager.getLastFirstVoiceSample();
        }

        

        // Process through effects chain
        auto stereoOutput = effects.processStereoSample(mixedSample);

        // Get master volume
        float masterVol = masterVolumeParameter->load();

        // Apply to output channels
        // Note: Voice manager already applies 0.25 scaling for polyphony headroom
        if (totalNumOutputChannels >= 1) {
            auto* leftData = buffer.getWritePointer(0);
            leftData[sample] = stereoOutput.first * masterVol;
        }
        if (totalNumOutputChannels >= 2) {
            auto* rightData = buffer.getWritePointer(1);
            rightData[sample] = stereoOutput.second * masterVol;
        }
    }
    };

    // ======================== INTERLEAVE MIDI + AUDIO (split-block timing) ========================
    // Walk MIDI events in sample order: render audio up to each event, apply the event, repeat;
    // then render whatever remains of the block. A live note stamped at sample 0 still renders
    // from sample 0 (identical to before — no added latency); a sequenced event stamped mid-block
    // now sounds at that offset instead of being pulled forward to the block start.
    const int numSamples = buffer.getNumSamples();
    int nextSampleToRender = 0;
    for (const auto metadata : midiMessages)
    {
        const int eventSample = juce::jlimit(0, numSamples, metadata.samplePosition);
        if (eventSample > nextSampleToRender)
        {
            renderSamples(nextSampleToRender, eventSample);
            nextSampleToRender = eventSample;
        }
        applyMidiMessage(metadata.getMessage());
    }
    renderSamples(nextSampleToRender, numSamples);

    // Update waveform display state
    waveformActive.store(voiceManager.getActiveVoiceCount() > 0);



}
//==============================================================================
bool PLANETtest4AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PLANETtest4AudioProcessor::createEditor()
{
    return new PLANETtest4AudioProcessorEditor(*this);
}

//==============================================================================
// PATCH MANAGEMENT
//==============================================================================

void PLANETtest4AudioProcessor::loadPatch(const juce::File& patchFile) {
    PLANETPatch patch;
    if (patchManager.loadPatchFromFile(patchFile, patch)) {
        // Suppress GUI updates during bulk parameter load for performance
        if (auto* editor = dynamic_cast<PLANETtest4AudioProcessorEditor*>(getActiveEditor())) {
            if (auto* gui = editor->getMainGui()) {
                gui->suppressParameterUpdates = true;
            }
        }

        // Reset parameters to defaults ONLY for old patches that don't have them
        for (int i = 1; i <= 10; ++i)
        {
            juce::String prefix = "k" + juce::String(i);

            // VelToHarmonic (default 0)
            juce::String velParamID = prefix + "VelToHarmonic";
            if (patch.parameters.count(velParamID) == 0)
            {
                if (auto* param = parameters.getParameter(velParamID))
                    param->setValueNotifyingHost(param->convertTo0to1(0.0f));
            }

            // LFOSync (default 0 = Free)
            juce::String syncParamID = prefix + "LFOSync";
            if (patch.parameters.count(syncParamID) == 0)
            {
                if (auto* param = parameters.getParameter(syncParamID))
                    param->setValueNotifyingHost(param->convertTo0to1(0.0f));
            }

            // LFOSyncDiv (default 7 = 1/4 note)
            juce::String divParamID = prefix + "LFOSyncDiv";
            if (patch.parameters.count(divParamID) == 0)
            {
                if (auto* param = parameters.getParameter(divParamID))
                    param->setValueNotifyingHost(param->convertTo0to1(7.0f));
            }
        }

        patchManager.applyPatchToProcessor(patch, parameters);

        // Reset mod wheel engagement for new patch
        modWheelEngaged.store(false);
        lastRawModWheelValue = 0.5f;

        // Store patch metadata in processor
        currentPatchName = patch.patchName;
        currentPatchDescription = patch.description;

        // Re-enable GUI updates and refresh all values once
        if (auto* editor = dynamic_cast<PLANETtest4AudioProcessorEditor*>(getActiveEditor())) {
            if (auto* gui = editor->getMainGui()) {
                gui->suppressParameterUpdates = false;
                gui->refreshAllGUIValues();
                gui->updatePatchNameDisplay(currentPatchName);
                gui->updatePatchCommentDisplay(currentPatchDescription);
            }
        }
    }
}

void PLANETtest4AudioProcessor::savePatch(const juce::File& patchFile,
                                          const juce::String& name,
                                          const juce::String& description,
                                          const juce::String& tags,
                                          const juce::String& category) {
    PLANETPatch patch = patchManager.createPatchFromProcessor(parameters, name, description, tags, category);
    patchManager.savePatchToFile(patch, patchFile);
}

//==============================================================================
//==============================================================================
//==============================================================================
void PLANETtest4AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();

    // Add patch metadata to state
    state.setProperty("patchName", currentPatchName, nullptr);
    state.setProperty("patchDescription", currentPatchDescription, nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PLANETtest4AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
    {
        auto newState = juce::ValueTree::fromXml(*xmlState);

        // Restore patch metadata
        currentPatchName = newState.getProperty("patchName", "Init").toString();
        currentPatchDescription = newState.getProperty("patchDescription", "").toString();

        parameters.replaceState(newState);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PLANETtest4AudioProcessor();
}

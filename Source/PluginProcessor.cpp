/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
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
            std::make_unique<juce::AudioParameterFloat>("k1", "K1 Coefficient", -2.0f, 2.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k2", "K2 Coefficient", -2.0f, 2.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k3", "K3 Coefficient", -2.0f, 2.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k4", "K4 Coefficient", -2.0f, 2.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k5", "K5 Coefficient", -2.0f, 2.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k6", "K6 Coefficient", -2.0f, 2.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k7", "K7 Coefficient", -2.0f, 2.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k8", "K8 Coefficient", -2.0f, 2.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k9", "K9 Coefficient", -2.0f, 2.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k10", "K10 Coefficient", -2.0f, 2.0f, 0.0f),

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
                juce::NormalisableRange<float>(-5.0f, 20.0f, 0.01f, 0.5f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k2EnvelopeAmount", "K2 Envelope Amount",
                juce::NormalisableRange<float>(-5.0f, 20.0f, 0.01f, 0.5f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k3EnvelopeAmount", "K3 Envelope Amount",
                juce::NormalisableRange<float>(-5.0f, 20.0f, 0.01f, 0.5f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k4EnvelopeAmount", "K4 Envelope Amount",
                juce::NormalisableRange<float>(-5.0f, 20.0f, 0.01f, 0.5f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k5EnvelopeAmount", "K5 Envelope Amount",
                juce::NormalisableRange<float>(-5.0f, 20.0f, 0.01f, 0.5f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k6EnvelopeAmount", "K6 Envelope Amount",
                juce::NormalisableRange<float>(-5.0f, 20.0f, 0.01f, 0.5f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k7EnvelopeAmount", "K7 Envelope Amount",
                juce::NormalisableRange<float>(-5.0f, 20.0f, 0.01f, 0.5f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k8EnvelopeAmount", "K8 Envelope Amount",
                juce::NormalisableRange<float>(-5.0f, 20.0f, 0.01f, 0.5f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k9EnvelopeAmount", "K9 Envelope Amount",
                juce::NormalisableRange<float>(-5.0f, 20.0f, 0.01f, 0.5f), 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k10EnvelopeAmount", "K10 Envelope Amount",
                juce::NormalisableRange<float>(-5.0f, 20.0f, 0.01f, 0.5f), 0.0f),

            // ======================== LFO SHAPE PARAMETERS (10) ========================
            std::make_unique<juce::AudioParameterFloat>("k1LFOShape", "K1 LFO Shape", 1.0f, 3.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k2LFOShape", "K2 LFO Shape", 1.0f, 3.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k3LFOShape", "K3 LFO Shape", 1.0f, 3.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k4LFOShape", "K4 LFO Shape", 1.0f, 3.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k5LFOShape", "K5 LFO Shape", 1.0f, 3.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k6LFOShape", "K6 LFO Shape", 1.0f, 3.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k7LFOShape", "K7 LFO Shape", 1.0f, 3.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k8LFOShape", "K8 LFO Shape", 1.0f, 3.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k9LFOShape", "K9 LFO Shape", 1.0f, 3.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>("k10LFOShape", "K10 LFO Shape", 1.0f, 3.0f, 1.0f),

            // ======================== LFO RATE PARAMETERS (10 - EXISTING) ========================
            std::make_unique<juce::AudioParameterFloat>("k1LFORate", "K1 LFO Rate", 0.05f, 1000.0f, 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k2LFORate", "K2 LFO Rate", 0.05f, 1000.0f, 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k3LFORate", "K3 LFO Rate", 0.05f, 1000.0f, 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k4LFORate", "K4 LFO Rate", 0.05f, 1000.0f, 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k5LFORate", "K5 LFO Rate", 0.05f, 1000.0f, 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k6LFORate", "K6 LFO Rate", 0.05f, 1000.0f, 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k7LFORate", "K7 LFO Rate", 0.05f, 1000.0f, 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k8LFORate", "K8 LFO Rate", 0.05f, 1000.0f, 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k9LFORate", "K9 LFO Rate", 0.05f, 1000.0f, 4.0f),
            std::make_unique<juce::AudioParameterFloat>("k10LFORate", "K10 LFO Rate", 0.05f, 1000.0f, 4.0f),

            // ======================== LFO AMOUNT PARAMETERS (10 - EXISTING) ========================
            std::make_unique<juce::AudioParameterFloat>("k1LFOAmount", "K1 LFO Amount", -5.0f, 5.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k2LFOAmount", "K2 LFO Amount", -5.0f, 5.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k3LFOAmount", "K3 LFO Amount", -5.0f, 5.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k4LFOAmount", "K4 LFO Amount", -5.0f, 5.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k5LFOAmount", "K5 LFO Amount", -5.0f, 5.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k6LFOAmount", "K6 LFO Amount", -5.0f, 5.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k7LFOAmount", "K7 LFO Amount", -5.0f, 5.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k8LFOAmount", "K8 LFO Amount", -5.0f, 5.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k9LFOAmount", "K9 LFO Amount", -5.0f, 5.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("k10LFOAmount", "K10 LFO Amount", -5.0f, 5.0f, 0.0f),

            // ======================== SPECTRAL MULTIPLIER INPUT PARAMETERS (10 - NEW) ========================
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
            std::make_unique<juce::AudioParameterFloat>("velToBrilliance", "Vel->Brilliance", -100.0f, 100.0f, 100.0f),
            std::make_unique<juce::AudioParameterFloat>("velToAttackTime", "Vel->Attack Time", 0.0f, 100.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("vintageAmount", "Vintage Amount", 0.0f, 100.0f, 0.0f),

            // ======================== VIBRATO PARAMETERS ============================================
            std::make_unique<juce::AudioParameterFloat>("vibratoRate", "Vibrato Rate", 0.5f, 12.0f, 5.0f),
            std::make_unique<juce::AudioParameterFloat>("vibratoDepth", "Vibrato Depth", 0.0f, 2.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("vibratoFadeIn", "Vibrato Fade In", 0.0f, 10.0f, 2.0f),

            // ======================== PITCH ATTACK ENVELOPE PARAMETERS ========================
            std::make_unique<juce::AudioParameterFloat>("pitchEnvDistance", "Pitch Env Distance", -12.0f, 12.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("pitchEnvTime", "Pitch Env Time", 0.01f, 5.0f, 0.5f),

            // ======================== AMPLITUDE ENVELOPE PARAMETERS (4) ========================
            std::make_unique<juce::AudioParameterFloat>("ampEnvAttackTime", "Amp Env Attack Time", 0.001f, 10.0f, 0.1f),
            std::make_unique<juce::AudioParameterFloat>("ampEnvDecayTime", "Amp Env Decay Time", 0.001f, 10.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>("ampEnvSustainLevel", "Amp Env Sustain Level", 0.0f, 1.0f, 0.7f),
            std::make_unique<juce::AudioParameterFloat>("ampEnvReleaseTime", "Amp Env Release Time", 0.001f, 10.0f, 2.0f),

            // ========================EXPONENTIAL ENVELOPE PARAMETER==========================
            std::make_unique<juce::AudioParameterFloat>("exponentialControl", "Exponential Control", 0.0f, 1.0f, 0.5f),

            // ======================== EFFECTS PARAMETERS (4) ========================
            std::make_unique<juce::AudioParameterFloat>("detuneAmount", "Detune Amount", 0.0f, 1.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>("detuneMix", "Detune Mix", 0.0f, 1.0f, 0.0f),

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
    velToBrillianceParameter = parameters.getRawParameterValue("velToBrilliance");
    velToAttackTimeParameter = parameters.getRawParameterValue("velToAttackTime");

    //======================== INITIALIZE VINTAGE PARAMETER POINTER ========================
    vintageAmountParameter = parameters.getRawParameterValue("vintageAmount");

    // ======================== INITIALIZE EFFECTS PARAMETER POINTERS ========================
    detuneAmountParameter = parameters.getRawParameterValue("detuneAmount");
    detuneMixParameter = parameters.getRawParameterValue("detuneMix");




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
    effects.prepareToPlay(sampleRate, samplesPerBlock);
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



// Helper function to generate LFO waveforms
float PLANETtest4AudioProcessor::generateLFOWaveform(double phase, float shape)
{
    int shapeInt = (int)shape;
    switch (shapeInt)
    {
    case 1: // Sine
        return std::sin(phase);
    case 2: // Triangle
    {
        double normalizedPhase = phase / (2.0 * juce::MathConstants<double>::pi);
        normalizedPhase = normalizedPhase - std::floor(normalizedPhase); // 0-1 range
        if (normalizedPhase < 0.5)
            return (float)(4.0 * normalizedPhase - 1.0); // -1 to +1
        else
            return (float)(3.0 - 4.0 * normalizedPhase); // +1 to -1
    }
    case 3: // Square
        return (std::sin(phase) >= 0.0f) ? 1.0f : -1.0f;
    default:
        return std::sin(phase); // Default to sine
    }
}

// Helper function to process individual envelope
float PLANETtest4AudioProcessor::processEnvelope(EnvelopeStage& stage, double& envTime, float& envLevel,
    double deltaTime, float attackTime, float decayTime, float sustainLevel, float releaseTime, bool noteOn)
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
            envLevel = (float)(envTime / attackTime);
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
            float progress = (float)(envTime / decayTime);
            envLevel = 1.0f + progress * (sustainLevel - 1.0f);
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
            float progress = (float)(envTime / releaseTime);
            envLevel = sustainLevel * (1.0f - progress);
        }
        break;

    case EnvelopeStage::Idle:
        envLevel = 0.0f;
        break;
    }

    return envLevel;
}

//==============================================Process Block ===============================

void PLANETtest4AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Update global parameters (shared by all voices)
    coefficients.updateAllActiveValues();
    float baseBrilliance = brillianceParameter->load();
    float modWheelBrilliance = currentModWheelValue.load();
    float effectiveBrilliance = juce::jlimit(0.0f, 1.0f, baseBrilliance + modWheelBrilliance);

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
    float velToBrilliance = velToBrillianceParameter->load();
    float velToAttackTime = velToAttackTimeParameter->load();
    float vintageAmount = vintageAmountParameter->load();
    float pitchEnvDistance = pitchEnvDistanceParameter->load();
    float pitchAttackTime = pitchEnvTimeParameter->load();


    // Process MIDI messages
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            float velocity = message.getVelocity() / 127.0f;
            if (velocity > 0.0f)
            {
                // Convert current pitch wheel to semitones
                float pitchWheelSemitones = currentPitchWheelValue * pitchWheelRange;

                voiceManager.startNote(message.getNoteNumber(), velocity, getSampleRate(), pitchWheelSemitones, vintageAmount,
                    velToAmplitude, velToBrilliance, baseBrilliance);
            }
            else
            {
                voiceManager.stopNote(message.getNoteNumber(), sustainPedalDown);
            }
        }
        else if (message.isNoteOff())
        {
            voiceManager.stopNote(message.getNoteNumber(), sustainPedalDown);
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

            // Handle mod wheel (CC #1)
            else if (message.getControllerNumber() == 1)
            {
                currentModWheelValue.store(message.getControllerValue() / 127.0f);
            }

        }
        // Handle mod wheel (CC #1) - bipolar, centered at 64
        else if (message.getControllerNumber() == 1)
        {
            float bipolarValue = (message.getControllerValue() - 64) / 64.0f;  // -1 to +1
            currentModWheelValue.store(bipolarValue * 0.5f);  // -0.5 to +0.5 brilliance offset
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
    }


    // ======================== LOAD ALL PARAMETERS ONCE PER BLOCK ========================
    float detuneAmount = detuneAmountParameter->load();
    float detuneMix = detuneMixParameter->load();
 
    float vibratoRate = vibratoRateParameter->load();
    float vibratoDepth = vibratoDepthParameter->load();
    float vibratoFadeIn = vibratoFadeInParameter->load();
    float pitchWheelSemitones = currentPitchWheelValue * pitchWheelRange;

    // Update effects parameters once per block
    effects.updateDetuneParams(detuneAmount, detuneMix);
 

    // ======================== GENERATE POLYPHONIC AUDIO ========================
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float mixedSample = voiceManager.processNextSample(coefficients,
            ampAttackTime, ampDecayTime, ampSustainLevel, ampReleaseTime,
            effectiveBrilliance, getSampleRate(),
            pitchWheelSemitones,
            vibratoRate, vibratoDepth, vibratoFadeIn,
            velToAmplitude, velToBrilliance, velToAttackTime, vintageAmount,
            pitchEnvDistance, pitchAttackTime);

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

        // Apply to output channels with volume scaling
        if (totalNumOutputChannels >= 1) {
            auto* leftData = buffer.getWritePointer(0);
            leftData[sample] = stereoOutput.first * 0.125f;
        }
        if (totalNumOutputChannels >= 2) {
            auto* rightData = buffer.getWritePointer(1);
            rightData[sample] = stereoOutput.second * 0.125f;
        }
    }

    // Update waveform display state
    waveformActive.store(voiceManager.getActiveVoiceCount() > 0);

    // Clean up finished voices
    voiceManager.clearFinishedVoices();



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
void PLANETtest4AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PLANETtest4AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}
//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PLANETtest4AudioProcessor();
}

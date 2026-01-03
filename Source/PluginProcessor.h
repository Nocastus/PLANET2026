/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PLANETDataStructures.h"
#include "PLANETVoiceManager.h"
#include "PLANETEffects.h"

//==============================================================================
/**
*/
class PLANETtest4AudioProcessor : public juce::AudioProcessor
{
public:

    // Debug values for GUI display=================================================
    std::atomic<float> debugCurrentAttackTime{ 0.0f };
    std::atomic<float> debugCurrentEnvLevel{ 0.0f };
    std::atomic<int> debugEnvStage{ 0 }; // 0=Idle, 1=Attack, 2=Decay, 3=Sustain, 4=Release
    std::atomic<float> debugDeltaTime{ 0.0f };
    std::atomic<float> debugCurrentFrequency{ 0.0f };
    std::atomic<int> debugEnvelopeUpdateCount{ 0 };
    std::atomic<float> debugEnvTime{ 0.0f };
    std::atomic<float> debugSampleRate{ 0.0f };

    std::atomic<int> debugLastNoteOn{ -1 };
    std::atomic<int> debugLastNoteOff{ -1 };
    std::atomic<float> debugLastVelocity{ 0.0f };
    std::atomic<int> debugActiveVoices{ 0 };

    std::atomic<int> debugAmpEnvStage{ 0 };
    std::atomic<float> debugAmpEnvTime{ 0.0f };
    std::atomic<float> debugReleaseTime{ 0.0f };
    std::atomic<float> debugDeltaTimeUsed{ 0.0f };

    std::atomic<float> debugVelocity{ 0.0f };
    std::atomic<float> debugBrilliance{ 0.0f };
    std::atomic<float> debugVelocityBrilliance{ 0.0f };

    std::atomic<int> debugLastCC{ -1 };
    std::atomic<int> debugLastCCValue{ 0 };

    // Waveform snapshot buffer (captured once per GUI frame)
    static constexpr int WAVEFORM_SNAPSHOT_SIZE = 2048;
    std::array<float, WAVEFORM_SNAPSHOT_SIZE> waveformSnapshot{};
    std::atomic<int> waveformSnapshotLength{ 0 };
    std::atomic<bool> waveformSnapshotReady{ false };
    std::atomic<bool> waveformSnapshotRequest{ false };
    std::atomic<bool> waveformActive{ false };

    // Audio-thread only (not atomic)
    int snapshotWritePos = 0;
    int snapshotTargetLength = 0;
    bool snapshotCapturing = false;

    //========Read MOD WHEEL to control BRILLIANCE

    std::atomic<float> currentModWheelValue{ 0.0f };

    //==============================================================================
    PLANETtest4AudioProcessor();
    ~PLANETtest4AudioProcessor() override;

    juce::AudioProcessorValueTreeState parameters;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:

    // NEW: Organized data structures
    CoefficientArray coefficients;          // Global coefficient parameters

    // NEW: Polyphonic voice system
    PLANETVoiceManager voiceManager;        // Handles all 16 voices

    // ======================== EFFECTS SYSTEM ========================
    PLANETEffects effects;

    // ======================== BASIC PARAMETERS ========================
    std::atomic<float>* brillianceParameter = nullptr;

    // ========================Envelope exponential curve parameter==================
    std::atomic<float>* exponentialControlParameter = nullptr;


    // ======================== AMPLITUDE ENVELOPE PARAMETERS ========================
    std::atomic<float>* ampEnvAttackTimeParameter = nullptr;
    std::atomic<float>* ampEnvDecayTimeParameter = nullptr;
    std::atomic<float>* ampEnvSustainLevelParameter = nullptr;
    std::atomic<float>* ampEnvReleaseTimeParameter = nullptr;

    // ======================== VELOCITY SCALING PARAMETERS ========================
    std::atomic<float>* velToAmplitudeParameter = nullptr;
    std::atomic<float>* velToBrillianceParameter = nullptr;
    std::atomic<float>* velToAttackTimeParameter = nullptr;
    std::atomic<float>* vintageAmountParameter = nullptr;


    // ======================== SYNTHESIS VARIABLES ========================
    double currentSampleRate = 44100.0;


    // Sustain pedal status tracking flag, false = off
    bool sustainPedalDown = false;

    // Pitch wheel gubbins
    float currentPitchWheelValue = 0.0f;  // -1.0 to +1.0 range
    float pitchWheelRange = 2.0f;         // }2 semitones (standard)

    // for Vibrato:
    std::atomic<float>* vibratoRateParameter = nullptr;        // Hz
    std::atomic<float>* vibratoDepthParameter = nullptr;       // Semitones
    std::atomic<float>* vibratoFadeInParameter = nullptr;      // Seconds

    // ======================== PITCH ATTACK ENVELOPE PARAMETERS ========================
    std::atomic<float>* pitchEnvDistanceParameter = nullptr;
    std::atomic<float>* pitchEnvTimeParameter = nullptr;

    // ======================== EFFECTS PARAMETER POINTERS ========================
    std::atomic<float>* detuneAmountParameter = nullptr;
    std::atomic<float>* detuneMixParameter = nullptr;
    std::atomic<float>* reverbLengthParameter = nullptr;
    std::atomic<float>* reverbMixParameter = nullptr;
    std::atomic<float>* reverbDampingParameter = nullptr;    // NEW
    std::atomic<float>* reverbWidthParameter = nullptr;      // NEW




    // ======================== ENVELOPE STATE VARIABLES (NEW) ===================================================================================================================================================



    // Helper functions for LFO and envelope processing
    float generateLFOWaveform(double phase, float shape);
    float processEnvelope(EnvelopeStage& stage, double& envTime, float& envLevel, double deltaTime,
        float attackTime, float decayTime, float sustainLevel, float releaseTime, bool noteOn);

    // Phase distortion function (uses active coefficients including envelope + LFO modulation)
    double applyPhaseDistortion(double normalizedPhase);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PLANETtest4AudioProcessor)
};

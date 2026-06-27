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
#include "PLANETPatchManager.h"

//==============================================================================
/**
*/
class PLANETtest4AudioProcessor : public juce::AudioProcessor
{
public:

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

    // Mod wheel state for brilliance control
    std::atomic<float> rawModWheelValue{ 0.5f };      // 0-1, MIDI 0-127 normalized, default center
    std::atomic<bool> modWheelEngaged{ false };       // Has mod wheel "picked up" since patch load?

    // LIFE voicing dev scaffold (see PLANETDataStructures.h) - written by the dev
    // panel in the GUI, read per-cycle by the voices. Not part of saved state.
    LifeVoicingParams lifeVoicing;

    //==============================================================================
    PLANETtest4AudioProcessor();
    ~PLANETtest4AudioProcessor() override;

    juce::AudioProcessorValueTreeState parameters;

    // ======================== PATCH MANAGEMENT ========================
    PLANETPatchManager patchManager;

    // Patch management methods
    void loadPatch(const juce::File& patchFile);
    void savePatch(const juce::File& patchFile, const juce::String& name,
                   const juce::String& description, const juce::String& tags,
                   const juce::String& category);
    const std::vector<PLANETPatch>& getAllPatches() const { return patchManager.getAllPatches(); }
    void scanPatchLibrary() { patchManager.scanPatchLibrary(PLANETPatchManager::getDefaultPatchDirectory()); }
    juce::String currentPatchName = "Init";
    juce::String currentPatchDescription = "";


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
  
    std::atomic<float>* velToAttackTimeParameter = nullptr;
    std::atomic<float>* vintageAmountParameter = nullptr;
    std::atomic<float>* lifeAmountParameter = nullptr;
    std::atomic<float>* lifeSeedParameter = nullptr;


    // ======================== SYNTHESIS VARIABLES ========================
    double currentSampleRate = 44100.0;


    // Sustain pedal status tracking flag, false = off
    bool sustainPedalDown = false;
    float lastRawModWheelValue = 0.5f;  // For detecting crossover through center (audio thread only)

    // Pitch wheel gubbins
    float currentPitchWheelValue = 0.0f;  // -1.0 to +1.0 range
    float pitchWheelRange = 2.0f;         // }2 semitones (standard)

    // DAW tempo for LFO sync
    double currentBPM = 120.0;
    double currentBeatPosition = 0.0;  // Current position in beats from DAW transport

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
    std::atomic<float>* warmthParameter = nullptr;
    std::atomic<float>* punchParameter = nullptr;
    std::atomic<float>* punchFrequencyParameter = nullptr;

    // ======================== MASTER CONTROLS ========================
    std::atomic<float>* masterVolumeParameter = nullptr;
    std::atomic<float>* transposeParameter = nullptr;




    // ======================== ENVELOPE STATE VARIABLES (NEW) ===================================================================================================================================================

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PLANETtest4AudioProcessor)
};

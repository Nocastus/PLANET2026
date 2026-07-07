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

    // DAW transport state, published each block for the GUI's LFO-rate pulse indicators.
    // transportPlaying=false OR displayBPM<=0 means "no tempo" -> a tempo-synced pulse shows solid on.
    std::atomic<double> displayBPM{ 120.0 };
    std::atomic<bool>   transportPlaying{ false };

    // Audio-thread only (not atomic)
    int snapshotWritePos = 0;
    int snapshotTargetLength = 0;
    bool snapshotCapturing = false;

    // Mod wheel state for brilliance control
    std::atomic<float> rawModWheelValue{ 0.5f };      // 0-1, MIDI 0-127 normalized, default center
    std::atomic<bool> modWheelEngaged{ false };       // Has mod wheel "picked up" since patch load?

    // Published effective (post mod-wheel / latch) Colour values, for the GUI diff indicators to
    // draw exactly what's heard - including Inverse direction and the Off-latch hold.
    std::atomic<float> effectiveBrillianceDisplay{ 0.5f };
    std::atomic<float> effectiveCarrierMorphDisplay{ 0.0f };

    // Output-saturation meter (F12): peak |sample| of the final output since the GUI last read it.
    // Audio thread keeps the running max; the GUI exchanges it to 0 each frame (single producer /
    // single consumer), so no short inter-frame peak is missed. Drives the Vol thumb traffic light.
    std::atomic<float> outputPeak{ 0.0f };

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
    void loadFactoryPatch(const PLANETPatch& patch);   // baked-in bank, no file
    void savePatch(const juce::File& patchFile, const juce::String& name,
                   const juce::String& description, const juce::String& tags,
                   const juce::String& category);
    const std::vector<PLANETPatch>& getAllPatches() const { return patchManager.getAllPatches(); }
    const std::vector<PLANETPatch>& getFactoryPatches() { return patchManager.getFactoryPatches(); }
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

    // Shared patch-apply path used by loadPatch (file) and loadFactoryPatch
    // (baked-in bank): legacy-default resets, apply, latch resets, GUI refresh.
    void applyLoadedPatch(const PLANETPatch& patch);

    // NEW: Organized data structures
    CoefficientArray coefficients;          // Global coefficient parameters

    // NEW: Polyphonic voice system
    PLANETVoiceManager voiceManager;        // Handles all 16 voices

    // ======================== EFFECTS SYSTEM ========================
    PLANETEffects effects;

    // ======================== BASIC PARAMETERS ========================
    std::atomic<float>* brillianceParameter = nullptr;
    std::atomic<float>* carrierMorphParameter = nullptr;   // F5 Density (carrier sine->soft-saw morph)
    std::atomic<float>* brillianceModWheelParameter = nullptr;    // 0 Off / 1 Normal / 2 Inverse
    std::atomic<float>* carrierMorphModWheelParameter = nullptr;  // 0 Off / 1 Normal / 2 Inverse

    // Colour-zone mod-wheel latch state (audio thread). When a slider's button is switched to Off
    // while the wheel has pushed the value off the thumb, the last effective value is latched and
    // held (heard) until the button is re-armed. held tracks the live effective while armed.
    float heldBrilliance = 0.5f;
    float heldCarrierMorph = 0.0f;
    bool  brillMWLatched = false;
    bool  densMWLatched = false;
    int   prevBrillMWMode = 1;   // Brilliance default = Normal
    int   prevDensMWMode = 0;    // Density default = Off
    bool  colourStateInitialised = false;   // sync held/prevMode to params on the first audio block

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
    std::atomic<float>* vibratoVelSwitchParameter = nullptr;   // VEL gate on/off
    std::atomic<float>* vibratoVelThresholdParameter = nullptr;// 1-127 MIDI velocity threshold

    // ======================== PITCH ATTACK ENVELOPE PARAMETERS ========================
    std::atomic<float>* pitchEnvDistanceParameter = nullptr;
    std::atomic<float>* pitchEnvTimeParameter = nullptr;

    // ======================== PORTAMENTO (F2a) ========================
    // Glide time (seconds in Time mode; seconds-per-octave in Rate mode). 0 = off.
    // Mode: false = constant-Time, true = constant-Rate.
    std::atomic<float>* portamentoTimeParameter = nullptr;
    std::atomic<float>* portamentoModeParameter = nullptr;

    // ======================== VOICE STACKING / UNISON (F6) ========================
    // Voices per note (1-4) and total detune spread in cents (symmetric, sums to zero).
    std::atomic<float>* unisonVoicesParameter = nullptr;
    std::atomic<float>* unisonDetuneParameter = nullptr;

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

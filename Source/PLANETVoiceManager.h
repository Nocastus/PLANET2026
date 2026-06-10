/*
  ==============================================================================
    PLANETVoiceManager.h - Polyphonic Voice Manager
    Manages pool of 16 voices, handles MIDI routing and voice stealing
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "PLANETVoice.h"
#include "PLANETDataStructures.h"
#include <array>

//==============================================================================
// POLYPHONIC VOICE MANAGER
//==============================================================================

class PLANETVoiceManager {
public:
    static constexpr int MAX_VOICES = 16;

    // Add this to PLANETVoiceManager.h public section:
    void setPitchWheelOffset(float offsetInSemitones, double sampleRate);
    void setExponentialControl(float value);

    PLANETVoice* getFirstActiveVoice();  // For debug info

    float getFirstVoiceFrequency() const;
    float getLastFirstVoiceSample() const { return lastFirstVoiceSample; }
    bool getFirstVoiceCycleStart() const;


    PLANETVoiceManager();

    // Voice control
    void startNote(int noteNumber, float velocity, double sampleRate, float pitchWheelOffset = 0.0f, float vintageAmount = 0.0f,
        float velToAmplitude = 100.0f, float brilliance = 0.5f, float lifeAmount = 0.0f, int lifeSeed = 0);
    void stopNote(int noteNumber, bool sustainPedalDown = false);  // Updated
    void releaseSustainedNotes();  // New method
    void stopAllNotes();

    // Audio processing
    float processNextSample(const CoefficientArray& globalParams,
        float ampAttack, float ampDecay, float ampSustain, float ampRelease,
        float brilliance, double sampleRate,
        float pitchWheelOffset,
        float vibratoRate, float vibratoDepth, float vibratoFadeIn,
        float velToAmplitude, float velToAttackTime, float vintageAmount,
        float pitchEnvDistance, float pitchAttackTime,
        double bpm, double beatPosition);

    // Voice management
    int getActiveVoiceCount() const;

private:
    // Voice pool
    std::array<PLANETVoice, MAX_VOICES> voices;

    // Voice allocation helpers
    PLANETVoice* findFreeVoice();
    PLANETVoice* findVoiceForNote(int noteNumber);
    PLANETVoice* findOldestVoice();  // For voice stealing

    // Voice stealing counter (simple increment for "oldest")
    int voiceAllocationCounter = 0;
    std::array<int, MAX_VOICES> voiceAllocationTimes;
    float lastFirstVoiceSample = 0.0f;
};

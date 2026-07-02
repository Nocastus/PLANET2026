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

    // LIFE voicing dev scaffold: hand every voice a pointer to the processor's
    // tunable constants (lifetimes match - processor owns both).
    void setLifeVoicingParams(const LifeVoicingParams* p) {
        for (auto& voice : voices) voice.setLifeVoicingParams(p);
    }

    // F10 single-trigger perc: point at the processor's per-drawbar params so startNote()
    // can read each drawbar's Single/Multi trigger mode. Processor owns it (stable lifetime).
    void setCoefficientParams(const CoefficientArray* p) { coeffParams = p; }

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

    // F10 single-trigger perc: advance the phrase grace-window countdown by one audio block.
    // Called once per processBlock (cheap - a single clamp), so window timing is measured in
    // real elapsed samples without touching the per-sample path.
    void advanceBlock(int numSamples);

    // Audio processing
    float processNextSample(const CoefficientArray& globalParams,
        float ampAttack, float ampDecay, float ampSustain, float ampRelease,
        float brilliance, float carrierMorph, double sampleRate,
        float pitchWheelOffset,
        float vibratoRate, float vibratoDepth, float vibratoFadeIn,
        float velToAmplitude, float velToAttackTime, float vintageAmount,
        float pitchEnvDistance, float pitchAttackTime,
        float lifeAmount,
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

    // ======================== F10 SINGLE-TRIGGER PERCUSSION ========================
    // Read-only view of the per-drawbar params (owned by the processor); used at note-on
    // to resolve each drawbar's Single/Multi trigger mode. Null until wired = all Multi.
    const CoefficientArray* coeffParams = nullptr;

    // Single-trigger grace window, counted in audio samples remaining and decremented once per
    // block by advanceBlock(). A note-on that finds NO keys held is a phrase start and reloads the
    // window; any note-on while samples remain fires the Single-trig drawbars - so a chord struck
    // together all gets the percussion, but a later legato note (window drained, keys still held)
    // does not. Re-arms the instant all keys lift. ~30 ms, sized at the phrase start.
    int percGraceSamplesRemaining = 0;
    static constexpr double kPercGraceSeconds = 0.030;

    // Physical key-down tracking for the phrase / re-arm edge. We must NOT use getActiveVoiceCount()
    // here: a voice stays active through its amp-release tail, so voice count wouldn't fall to 0
    // until the release finished - delaying re-arm and killing perc on detached playing. Instead
    // track keys directly: set on note-on, cleared on note-off (key-up), so heldKeyCount == 0 means
    // "all keys released" the moment the last key lifts, regardless of any sounding release tail.
    // A bitset + count keeps it O(1) and allocation-free, and idempotent under duplicate on/off.
    std::array<bool, 128> keyHeld {};
    int heldKeyCount = 0;
};

/*
  ==============================================================================
    PLANETVoice.h - Individual Voice for Polyphonic PLANET Synthesizer
    Each voice handles one note with independent modulation evolution
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "PLANETDataStructures.h"

//==============================================================================
// INDIVIDUAL VOICE CLASS
//==============================================================================

class PLANETVoice {
public:

    bool getIsActiveFlag() const { return noteIsActive; }
    PLANETVoice();

    // Voice control
    void startNote(int noteNumber, float velocity, double sampleRate, float vintageAmount = 0.0f,
        float velToAmplitude = 100.0f, float velToBrilliance = 100.0f, float brilliance = 0.5f);
    void stopNote(bool sustainPedalDown = false);  // KEEP ONLY THIS ONE
    void triggerRelease();  // New method for forced release
    bool isActive() const { return noteIsActive; }
    bool shouldBeRemoved() const;

    // Sustain pedal methods
    bool isSustainedByPedal() const { return sustainPedalHeld; }
    void setSustainPedalHeld(bool held) { sustainPedalHeld = held; }

    // Pitch modulation methods
    void setPitchOffset(float offsetInSemitones);   // Set total offset directly (for pitch wheel)
    void addPitchOffset(float offsetInSemitones);   // Add to accumulator (for multiple sources)
    void resetPitchOffset();                        // Clear accumulator
    void updatePitchFromOffset(double sampleRate);  // Apply accumulated offset

    // Audio processing
    float processNextSample(const CoefficientArray& globalParams,
        float ampAttack, float ampDecay, float ampSustain, float ampRelease,
        float brilliance, double sampleRate,
        float pitchWheelOffset,
        float vibratoRate, float vibratoDepth, float vibratoFadeIn,
        float velToAmplitude, float velToBrilliance, float velToAttackTime, float vintageAmount,
        float pitchEnvDistance, float pitchAttackTime);

    // Voice identification
    int getNoteNumber() const { return currentNoteNumber; }
    float getCurrentFrequency() const { return (float)currentFrequency; }
    bool getCycleStartFlag() const { return cycleStartFlag; }
    float getVelocity() const { return noteVelocity; }

    // Mod wheel hook (for future implementation)
    void setModWheelScale(float scale) { modWheelScale = juce::jlimit(0.0f, 1.0f, scale); }

    //Setter for Envelope exponential curve control
    void setExponentialControl(float value) { exponentialControl = value; }

    //Getter methods for Debug
    float getDebugVelocity() const { return debugLastVelocity; }
    float getDebugBrilliance() const { return debugLastBrilliance; }
    float getDebugVelocityBrilliance() const { return debugLastVelocityBrilliance; }
   


private:

    // Per-voice vibrato state
    struct VibratoState {
        double lfoPhase = 0.0;              // LFO phase per voice
        double fadeInTime = 0.0;            // Fade-in envelope time
        float fadeInLevel = 0.0f;           // Current fade-in level (0-1)

        void reset() {
            lfoPhase = 0.0;
            fadeInTime = 0.0;
            fadeInLevel = 0.0f;
        }
    };

    VibratoState vibratoState;






    //Debug variables

    float debugLastVelocity = 0.0f;
    float debugLastBrilliance = 0.0f;
    float debugLastVelocityBrilliance = 0.0f;

    // NEW: Cache expensive velocity calculations (add after debugLastVelocityBrilliance)
    float cachedVelocityAmplitude = 1.0f;
    float cachedVelocityBrilliance = 0.5f;

    //Exponential envelope amount
    float exponentialControl = 0.5f;  // Now a member variable instead of constant

    // Mod wheel hook (for future implementation)
    float modWheelScale = 1.0f;             // 0-1, controlled by mod wheel (future)

    // Voice identity & state
    int currentNoteNumber = -1;
    float noteVelocity = 0.0f;
    bool noteIsActive = false;

    // Sustain pedal status for an individual voice
    bool sustainPedalHeld = false;  // True if this voice is being sustained by pedal

    // Pitch modulation state
    double basePitchFrequency = 440.0;  // Store unmodulated frequency
    float totalPitchOffset = 0.0f;      // Accumulator in semitones

    // Synthesis state
    double currentAngle = 0.0;
    double angleDelta = 0.0;
    double currentFrequency = 440.0;

    // Per-voice modulation states (K1-K10 independent evolution)
    ModulationStateArray coeffModStates;

    // Per-voice LFO phases  
    std::array<double, NUM_COEFFICIENTS> lfoPhases;

    // Per-voice amplitude envelope
    EnvelopeStage ampEnvStage = EnvelopeStage::Idle;
    double ampEnvTime = 0.0;
    float ampEnvLevel = 0.0f;
    float storedAmpEnvValue = 0.0f;  // stores envelope value between cycles
    float ampReleaseStartLevel = 0.0f; // new mechanism for ensuring release starts at the right level

    bool cycleStartFlag = false;

    // Per-voice pitch attack envelope
    EnvelopeStage pitchEnvStage = EnvelopeStage::Idle;
    double pitchEnvTime = 0.0;
    float pitchEnvLevel = 0.0f;

    // Per-voice coefficient grid (each voice calculates its own modulated coefficients)
    CoefficientGrid voiceCoeffGrid;

    // Helper functions
    double applyPhaseDistortion(double normalizedPhase, float morphAmount, const CoefficientArray& globalParams);
    float generateLFOWaveform(double phase, float shape);
    float processEnvelope(EnvelopeStage& stage, double& envTime, float& envLevel,
        double deltaTime, float attackTime, float decayTime,
        float sustainLevel, float releaseTime, bool noteOn,
        float releaseStartLevel, float curveAmount);

    float processAmplitudeEnvelope(double deltaTime, float attackTime, float decayTime,
        float sustainLevel, float releaseTime);
};

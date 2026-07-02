/*
  ==============================================================================
    PLANETVoiceManager.cpp - Polyphonic Voice Manager Implementation
  ==============================================================================
*/

#include "PLANETVoiceManager.h"

//==============================================================================
// CONSTRUCTOR
//==============================================================================

PLANETVoiceManager::PLANETVoiceManager()
{
    // Initialize allocation times
    for (auto& time : voiceAllocationTimes) {
        time = 0;
    }
}

//==============================================================================
// VOICE CONTROL METHODS
//==============================================================================

void PLANETVoiceManager::startNote(int noteNumber, float velocity, double sampleRate, float currentPitchWheelOffset, float vintageAmount,
    float velToAmplitude, float brilliance, float lifeAmount, int lifeSeed)
{
    // [Existing voice finding/allocation code...]
    auto* existingVoice = findVoiceForNote(noteNumber);
    if (existingVoice) {
        existingVoice->stopNote(false);
    }

    auto* voice = findFreeVoice();
    if (!voice) {
        voice = findOldestVoice();
    }

    if (voice) {
        voice->startNote(noteNumber, velocity, sampleRate, vintageAmount, velToAmplitude, brilliance, lifeAmount, lifeSeed);

        // Apply current pitch wheel state to new voice
        voice->setPitchOffset(currentPitchWheelOffset);
        voice->updatePitchFromOffset(sampleRate);

        // Update allocation tracking
        int voiceIndex = voice - voices.data();
        voiceAllocationTimes[voiceIndex] = ++voiceAllocationCounter;
    }
}

// Now with added sustain pedal functionality!
void PLANETVoiceManager::stopNote(int noteNumber, bool sustainPedalDown)
{
    auto* voice = findVoiceForNote(noteNumber);
    if (voice) {
        voice->stopNote(sustainPedalDown);
    }
}

// When systain pedal is released, stop the held notes
void PLANETVoiceManager::releaseSustainedNotes()
{
    // Release all voices that are being sustained by pedal
    for (auto& voice : voices) {
        if (voice.isSustainedByPedal()) {
            voice.triggerRelease();
        }
    }
}

void PLANETVoiceManager::stopAllNotes()
{
    for (auto& voice : voices) {
        if (voice.isActive()) {
            voice.stopNote(false);
        }
    }
}

//==============================================================================
// AUDIO PROCESSING
//==============================================================================

float PLANETVoiceManager::processNextSample(const CoefficientArray& globalParams,
    float ampAttack, float ampDecay, float ampSustain, float ampRelease,
    float brilliance, float carrierMorph, double sampleRate,
    float pitchWheelOffset,
    float vibratoRate, float vibratoDepth, float vibratoFadeIn,
    float velToAmplitude, float velToAttackTime, float vintageAmount,
    float pitchEnvDistance, float pitchAttackTime,
    float lifeAmount,
    double bpm, double beatPosition)

{  // Opening brace
    float mixedSample = 0.0f;
    int activeVoices = 0;
    bool firstVoiceCaptured = false;

    for (auto& voice : voices) {
        if (voice.isActive()) {
            float voiceSample = voice.processNextSample(globalParams,
                ampAttack, ampDecay, ampSustain, ampRelease,
                brilliance, carrierMorph, sampleRate,
                pitchWheelOffset,
                vibratoRate, vibratoDepth, vibratoFadeIn,
                velToAmplitude, velToAttackTime, vintageAmount,
                pitchEnvDistance, pitchAttackTime,
                lifeAmount,
                bpm, beatPosition);

            // Capture first voice for waveform display
            if (!firstVoiceCaptured) {
                lastFirstVoiceSample = voiceSample;
                firstVoiceCaptured = true;
            }

            mixedSample += voiceSample;
            activeVoices++;
        }
    }

    if (activeVoices > 0) {
        mixedSample *= 0.25f;
    }

    return mixedSample;
}
//==============================================================================
// VOICE MANAGEMENT
//==============================================================================

int PLANETVoiceManager::getActiveVoiceCount() const
{
    int count = 0;
    for (const auto& voice : voices) {
        if (voice.isActive()) {
            count++;
        }
    }
    return count;
}

float PLANETVoiceManager::getFirstVoiceFrequency() const
{
    for (const auto& voice : voices) {
        if (voice.isActive()) {
            return voice.getCurrentFrequency();
        }
    }
    return 440.0f;
}

bool PLANETVoiceManager::getFirstVoiceCycleStart() const
{
    for (const auto& voice : voices) {
        if (voice.isActive()) {
            return voice.getCycleStartFlag();
        }
    }
    return false;
}

//==============================================================================
// PRIVATE HELPER METHODS
//==============================================================================

PLANETVoice* PLANETVoiceManager::findFreeVoice()
{
    // Look for an inactive voice
    for (auto& voice : voices) {
        if (!voice.isActive()) {
            return &voice;
        }
    }
    return nullptr;  // No free voices
}

PLANETVoice* PLANETVoiceManager::getFirstActiveVoice()
{
    for (auto& voice : voices) {
        if (voice.isActive()) {
            return &voice;
        }
    }
    return nullptr;  // No active voices
}

void PLANETVoiceManager::setExponentialControl(float value)
{
    for (auto& voice : voices) {
        voice.setExponentialControl(value);
    }
}

PLANETVoice* PLANETVoiceManager::findVoiceForNote(int noteNumber)
{
    // Look for a voice currently playing this note number
    for (auto& voice : voices) {
        if (voice.isActive() && voice.getNoteNumber() == noteNumber) {
            return &voice;
        }
    }
    return nullptr;  // Note not currently playing
}

PLANETVoice* PLANETVoiceManager::findOldestVoice()
{
    // Find the voice with the smallest allocation time (oldest)
    PLANETVoice* oldestVoice = nullptr;
    int oldestTime = INT_MAX;

    for (size_t i = 0; i < voices.size(); ++i) {
        if (voices[i].isActive() && voiceAllocationTimes[i] < oldestTime) {
            oldestTime = voiceAllocationTimes[i];
            oldestVoice = &voices[i];
        }
    }

    // If we can't find an active voice to steal, just use the first one
    if (!oldestVoice) {
        oldestVoice = &voices[0];
    }

    return oldestVoice;
}

// Pitch wheel implementation
void PLANETVoiceManager::setPitchWheelOffset(float offsetInSemitones, double sampleRate)
{
    // Apply pitch wheel offset to all active voices
    for (auto& voice : voices) {
        if (voice.isActive()) {
            voice.setPitchOffset(offsetInSemitones);
            voice.updatePitchFromOffset(sampleRate);
        }
    }
}

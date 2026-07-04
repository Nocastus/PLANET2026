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
    float velToAmplitude, float brilliance, float lifeAmount, int lifeSeed,
    float portamentoTime, int portamentoMode,
    int unisonVoices, float unisonDetune)
{
    // ======================== F10: resolve single-trigger firing for THIS note-on ========================
    // A note-on that finds polyphony at 0 starts a new phrase: reload the grace window. Any note-on
    // while the window still has samples fires the Single-trig drawbars (so a chord struck together
    // all gets the percussion); once it drains with keys still held, later notes don't fire - and it
    // only reloads after all keys release (polyphony back to 0). Multi drawbars always fire (unchanged).
    // Re-arm is driven by physical keys held, NOT active voices: a voice stays active through its
    // amp-release tail, so keying off getActiveVoiceCount() would delay re-arming until the release
    // finished - defeating perc on detached playing. heldKeyCount is checked BEFORE registering
    // this key, so the first key of a phrase (no keys currently down) is the phrase start.
    const bool phraseStart = (heldKeyCount == 0);
    if (phraseStart)
        percGraceSamplesRemaining = (int)(kPercGraceSeconds * sampleRate);
    const bool fireSingle = (percGraceSamplesRemaining > 0);

    // Register this key as held (idempotent - a retriggered still-held note isn't a new phrase).
    if (noteNumber >= 0 && noteNumber < 128 && !keyHeld[noteNumber]) {
        keyHeld[noteNumber] = true;
        ++heldKeyCount;
    }

    std::array<bool, NUM_COEFFICIENTS> drawbarFire;
    for (int i = 0; i < NUM_COEFFICIENTS; ++i) {
        const bool isSingle = coeffParams && (*coeffParams)[i].trigSingle >= 0.5f;
        drawbarFire[i] = isSingle ? fireSingle : true;   // Multi (or unwired) drawbars always fire
    }

    // Retrigger: release EVERY voice still playing this note. With unison a note owns a whole stack
    // that all share the note number, so we must stop them all, not just the first one found.
    for (auto& v : voices) {
        if (v.isActive() && v.getNoteNumber() == noteNumber)
            v.stopNote(false);
    }

    // Unison (F6): allocate `unisonVoices` voices for this note, each a permanent, symmetric detune
    // that sums to zero. detuneCents(i) = detune * (i/(S-1) - 0.5): S=1 -> {0}; S=2 -> {-d/2,+d/2};
    // S=3 -> {-d/2, 0, +d/2} (centre voice at pitch); S=4 -> {-d/2,-d/6,+d/6,+d/2}. Stealing stays
    // per-voice (findOldestVoice), so an over-full stack can split an old group - a standard stealing
    // artifact; whole-group stealing is a later refinement.
    const int S = juce::jlimit(1, MAX_VOICES, unisonVoices);
    for (int i = 0; i < S; ++i)
    {
        auto* voice = findFreeVoice();
        if (!voice) voice = findOldestVoice();
        if (!voice) break;

        const float detuneCents = (S > 1) ? unisonDetune * ((float)i / (float)(S - 1) - 0.5f) : 0.0f;

        // Portamento origin (F2a): a voice glides from the pitch IT last played, so read the chosen
        // voice's live glide pitch BEFORE startNote() overwrites it. --- MODEL-2 PIVOT POINT: to
        // switch to legato/nearest-held glide, replace this line with the nearest held note's pitch. ---
        const float glideFromHz = voice->getCurrentGlidePitchHz();

        voice->startNote(noteNumber, velocity, sampleRate, vintageAmount, velToAmplitude, brilliance, lifeAmount, lifeSeed, &drawbarFire,
            glideFromHz, portamentoTime, portamentoMode, detuneCents);

        // Apply current pitch wheel state to the new voice
        voice->setPitchOffset(currentPitchWheelOffset);
        voice->updatePitchFromOffset(sampleRate);

        // Update allocation tracking (newest counter, so this voice won't be stolen by the next
        // iteration's findOldestVoice)
        int voiceIndex = voice - voices.data();
        voiceAllocationTimes[voiceIndex] = ++voiceAllocationCounter;
    }
}

// Now with added sustain pedal functionality!
void PLANETVoiceManager::stopNote(int noteNumber, bool sustainPedalDown)
{
    // Key physically up: clear held state so the perc re-arms on the key-lift itself, regardless of
    // any amp-release tail still sounding (F10). We clear it even when the sustain pedal holds the
    // sound - the pedal holds the note, not the key, and "all keys released" is what re-arms perc.
    if (noteNumber >= 0 && noteNumber < 128 && keyHeld[noteNumber]) {
        keyHeld[noteNumber] = false;
        --heldKeyCount;
    }

    // Release EVERY voice playing this note (a unison stack shares the note number), not just the first.
    for (auto& voice : voices) {
        if (voice.isActive() && voice.getNoteNumber() == noteNumber)
            voice.stopNote(sustainPedalDown);
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
    // Clear key-held tracking too, so a panic / all-notes-off fully re-arms the perc (F10).
    keyHeld.fill(false);
    heldKeyCount = 0;

    for (auto& voice : voices) {
        if (voice.isActive()) {
            voice.stopNote(false);
        }
    }
}

// F10: drain the single-trigger grace window by one block's worth of samples. Once per block,
// so the ~30 ms window is measured in real elapsed audio time (see startNote / the header note).
void PLANETVoiceManager::advanceBlock(int numSamples)
{
    percGraceSamplesRemaining = juce::jmax(0, percGraceSamplesRemaining - numSamples);
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

void PLANETVoiceManager::setExponentialControl(float value)
{
    for (auto& voice : voices) {
        voice.setExponentialControl(value);
    }
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

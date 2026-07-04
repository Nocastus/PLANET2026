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
// SINE LOOKUP TABLE (Optimization)
//==============================================================================

class SineLUT {
public:
    static constexpr int TABLE_SIZE = 8192;
    static constexpr double TABLE_SIZE_OVER_2PI = TABLE_SIZE / (2.0 * juce::MathConstants<double>::pi);

    SineLUT() {
        for (int i = 0; i < TABLE_SIZE; ++i) {
            table[i] = std::sin(2.0 * juce::MathConstants<double>::pi * i / TABLE_SIZE);
        }
    }

    // Fast sine lookup with linear interpolation
    inline float lookup(double phase) const {
        // Wrap phase to [0, 2π]
        while (phase < 0.0) phase += 2.0 * juce::MathConstants<double>::pi;
        while (phase >= 2.0 * juce::MathConstants<double>::pi)
            phase -= 2.0 * juce::MathConstants<double>::pi;

        // Convert phase to table index. Phase is non-negative here, so the int cast IS the
        // floor, and TABLE_SIZE is a power of two, so wrap is a mask - no modulo, no floor().
        double indexDouble = phase * TABLE_SIZE_OVER_2PI;
        int floorIndex = static_cast<int>(indexDouble);
        int index1 = floorIndex & (TABLE_SIZE - 1);
        int index2 = (index1 + 1) & (TABLE_SIZE - 1);

        // Linear interpolation
        float fraction = static_cast<float>(indexDouble - floorIndex);
        return table[index1] * (1.0f - fraction) + table[index2] * fraction;
    }

    // Get singleton instance
    static const SineLUT& getInstance() {
        static SineLUT instance;
        return instance;
    }

private:
    std::array<float, TABLE_SIZE> table;
};

//==============================================================================
// SOFT-SAW LOOKUP TABLE (F5 "Density" carrier morph)
//==============================================================================
// The alternate carrier the Density knob morphs toward. Built as a pure SINE
// SERIES  sum_{n=1}^N (r^(n-1)/n) sin(n*theta)  so it is exactly 0 at theta=0 -
// the same zero-at-cycle-boundary property SineLUT has, which is what keeps the
// coefficient-staging click-free (see PLANETVoice.cpp). Shape (r=0.8, N=16) was
// chosen by offline analysis: soft enough to leave morph range, yet a few drawbars
// sharpen it to a real saw. Fundamental amplitude is 1 (matches sine), so morphing
// preserves the fundamental and fades in the upper harmonics; peak ~1.16.
class SoftSawLUT {
public:
    static constexpr int TABLE_SIZE = 8192;
    static constexpr double TABLE_SIZE_OVER_2PI = TABLE_SIZE / (2.0 * juce::MathConstants<double>::pi);
    static constexpr float  TAPER = 0.8f;   // r
    static constexpr int    HARMONICS = 16; // N

    SoftSawLUT() {
        for (int i = 0; i < TABLE_SIZE; ++i) {
            double theta = 2.0 * juce::MathConstants<double>::pi * i / TABLE_SIZE;
            double v = 0.0, rp = 1.0;               // rp = TAPER^(n-1)
            for (int n = 1; n <= HARMONICS; ++n) {
                v += (rp / n) * std::sin(n * theta);
                rp *= TAPER;
            }
            table[i] = (float)v;
        }
    }

    inline float lookup(double phase) const {
        while (phase < 0.0) phase += 2.0 * juce::MathConstants<double>::pi;
        while (phase >= 2.0 * juce::MathConstants<double>::pi)
            phase -= 2.0 * juce::MathConstants<double>::pi;
        // Same mask/cast indexing as SineLUT::lookup (phase non-negative, table power-of-two).
        double indexDouble = phase * TABLE_SIZE_OVER_2PI;
        int floorIndex = static_cast<int>(indexDouble);
        int index1 = floorIndex & (TABLE_SIZE - 1);
        int index2 = (index1 + 1) & (TABLE_SIZE - 1);
        float fraction = static_cast<float>(indexDouble - floorIndex);
        return table[index1] * (1.0f - fraction) + table[index2] * fraction;
    }

    static const SoftSawLUT& getInstance() {
        static SoftSawLUT instance;
        return instance;
    }

private:
    std::array<float, TABLE_SIZE> table;
};

//==============================================================================
// INDIVIDUAL VOICE CLASS
//==============================================================================

class PLANETVoice {
public:

    bool getIsActiveFlag() const { return noteIsActive; }
    PLANETVoice();

    // Voice control
    // drawbarFire (F10): per-drawbar gate for this note-on. drawbarFire[i] == true resets that
    // drawbar's envelope to Attack (fires it - the normal Multi behaviour); false forces it to
    // Idle/silent (a Single-trig drawbar that must not fire on this note). null = all fire.
    void startNote(int noteNumber, float velocity, double sampleRate, float vintageAmount = 0.0f,
        float velToAmplitude = 100.0f, float brilliance = 0.5f, float lifeAmount = 0.0f, int lifeSeed = 0,
        const std::array<bool, NUM_COEFFICIENTS>* drawbarFire = nullptr,
        float glideFromHz = 0.0f, float portamentoTime = 0.0f, int portamentoMode = 0,
        float detuneCents = 0.0f);

    // Portamento (F2a): the live pitch this voice would glide FROM if reused now - its base
    // pitch plus any still-decaying glide, EXCLUDING vibrato/wheel (those are momentary, not a
    // glide origin). Returns 0 if the voice has never sounded (no glide-from-silence). The voice
    // manager reads this before startNote() overwrites the pitch. It is deliberately the ONLY
    // thing the manager needs to know the origin from: to pivot from per-voice history to a
    // legato/nearest-held model, change what the manager passes as glideFromHz - not this voice.
    float getCurrentGlidePitchHz() const;
    void stopNote(bool sustainPedalDown = false);  // KEEP ONLY THIS ONE
    void setLifeVoicingParams(const LifeVoicingParams* p) { lifeVoicing = p; }
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
        float brilliance, float carrierMorph, double sampleRate,
        float pitchWheelOffset,
        float vibratoRate, float vibratoDepth, float vibratoFadeIn,
        float velToAmplitude, float velToAttackTime, float vintageAmount,
        float pitchEnvDistance, float pitchAttackTime,
        float lifeAmount,
        double bpm, double beatPosition);

    // Voice identification
    int getNoteNumber() const { return currentNoteNumber; }
    float getCurrentFrequency() const { return (float)currentFrequency; }
    bool getCycleStartFlag() const { return cycleStartFlag; }
    float getVelocity() const { return noteVelocity; }

    //Setter for Envelope exponential curve control. The curve constants k and (1 - e^-k) are
    //cached here so the per-sample amp envelope pays one std::exp, not two (values unchanged).
    void setExponentialControl(float value) {
        if (value != exponentialControl) {
            exponentialControl = value;
            refreshEnvCurveCache();
        }
    }

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


    // Cache expensive velocity calculations
    float cachedVelocityAmplitude = 1.0f;
    float cachedVelocityBrilliance = 0.5f;
    float cachedCarrierMorph = 0.0f;   // F5 Density: cached at cycle boundary (click-free, output=0 there)

    //Exponential envelope amount
    float exponentialControl = 0.5f;  // Now a member variable instead of constant

    // Cached envelope-curve constants, functions of exponentialControl only. Refreshed by
    // setExponentialControl / the constructor, so processEnvelope never recomputes e^-k.
    float envCurveK = 4.0f;                 // k = 1 + curve*6
    float envCurveOneMinusEk = 0.98168f;    // 1 - e^-k (real value set by refreshEnvCurveCache)
    float envCurveEk = 0.01832f;            // e^-k     (real value set by refreshEnvCurveCache)
    void refreshEnvCurveCache();

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

    // ======================== LIFE FEATURE STATE ========================
    // Stage 1: each modulator carries its own accumulated phase instead of deriving it
    // from the carrier (f*x). This is what allows non-integer / time-varying multipliers
    // without a per-cycle discontinuity. With integer multipliers and no doublet engaged,
    // modPhases[i] tracks f*x (mod 2pi) exactly, so existing patches are unchanged.
    std::array<double, NUM_COEFFICIENTS> modPhases;

    // Stage 3 doublets: each partial is two components (two "polarizations" of a struck
    // string) split by a small ratio about f; their interference is the per-partial beat.
    // Because the split is a ratio (not Hz), partial n beats n times faster than the
    // fundamental - the signature of a real instrument. Character (split scale, energy
    // share) is drawn from the LIFE seed; ratios are refreshed once per carrier cycle by
    // updateLifeRatios() so the LIFE knob acts on held notes in real time.
    std::array<double, NUM_COEFFICIENTS> modPhasesB;     // lower component's phase
    std::array<double, NUM_COEFFICIENTS> doubletRatioA;  // upper component, 2^(+c/2400)
    std::array<double, NUM_COEFFICIENTS> doubletRatioB;  // lower component, 2^(-c/2400)
    std::array<float,  NUM_COEFFICIENTS> doubletMixB;    // s: lower component's energy share
    std::array<float,  NUM_COEFFICIENTS> doubletUnit;    // seeded per-drawbar split scale [0.3,1]
    std::array<bool,   NUM_COEFFICIENTS> doubletEngaged; // per-note gate; engages, never disengages
    uint32_t lifeNoiseState = 0x12345678;                // per-strike RNG (LCG), advances across notes
    const LifeVoicingParams* lifeVoicing = nullptr;      // dev-tunable constants (null = defaults)
    void updateLifeRatios(float lifeAmount);

    // Per-voice amplitude envelope
    EnvelopeStage ampEnvStage = EnvelopeStage::Idle;
    double ampEnvTime = 0.0;
    float ampEnvLevel = 0.0f;
    float ampReleaseStartLevel = 0.0f; // new mechanism for ensuring release starts at the right level

    bool cycleStartFlag = false;

    // Per-voice pitch attack envelope
    EnvelopeStage pitchEnvStage = EnvelopeStage::Idle;
    double pitchEnvTime = 0.0;
    float pitchEnvLevel = 0.0f;

    // ======================== PORTAMENTO / GLIDE (F2a) ========================
    // Mechanically identical to the pitch attack envelope (a semitone offset that decays to
    // zero over a set time, same normalised-exp curve), but the start distance is per-voice and
    // signed: set at note-on to (originPitch - thisNote), so each voice slides FROM its own last
    // pitch. glideLevel runs 0->1 over glideTimeSec; level 1 (the default) means "arrived" = no
    // glide, so a note with portamento off costs nothing but a compare. hasEverSounded gates the
    // first note (nothing to glide from).
    float  glideStartOffset = 0.0f;   // signed semitones (origin - target), latched at note-on
    float  glideLevel       = 1.0f;   // 0..1; 1 = arrived
    double glideTime        = 0.0;    // elapsed seconds since note-on
    double glideTimeSec     = 0.0;    // resolved glide duration for THIS note (Time/Rate applied)
    bool   hasEverSounded   = false;  // false until the voice's first startNote()

    // Per-voice coefficient grid (each voice calculates its own modulated coefficients)
    CoefficientGrid voiceCoeffGrid;

    // ======================== PER-DRAWBAR ROUTING (F7/F8) ========================
    // Two independent destinations per drawbar, snapshotted at the carrier-cycle boundary
    // (alongside coefficient promotion) so toggling is click-free - at that boundary every
    // sin(modPhases[i]) is exactly 0, so both the phase and the additive contribution are 0.
    //   routePMGain[i] : 1 if the bar's k*sin term feeds the phase-distortion path, else 0.
    //   routeAddAmp[i] : additive amplitude (clamped k_eff / 2) if routed direct-to-output, else 0.
    // Both destinations off = muted (subsumes the old "mute" idea); both on = feeds both.
    std::array<float, NUM_COEFFICIENTS> routePMGain;
    std::array<float, NUM_COEFFICIENTS> routeAddAmp;

    // Helper functions
    double applyPhaseDistortion(double normalizedPhase, float morphAmount, const CoefficientArray& globalParams,
        double& additiveOut);
    float generateLFOWaveform(double phase, float shape);
    // Curve shape comes from the cached envCurve* members (exponentialControl), not a parameter.
    float processEnvelope(EnvelopeStage& stage, double& envTime, float& envLevel,
        double deltaTime, float attackTime, float decayTime,
        float sustainLevel, float releaseTime, bool noteOn,
        float releaseStartLevel);

    float processAmplitudeEnvelope(double deltaTime, float attackTime, float decayTime,
        float sustainLevel, float releaseTime);
};

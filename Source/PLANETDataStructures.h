/*
  ==============================================================================
    PLANET VST - Clean Coefficient Structure (Array of Structs Pattern)
    Replace the matrix approach with elegant per-coefficient structs
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <array>

constexpr int NUM_COEFFICIENTS = 10;  // K1 through K10
constexpr int NUM_SYNC_DIVISIONS = 13;

// LFO Tempo Sync division lookup tables
static constexpr float SYNC_DIVISION_BEATS[NUM_SYNC_DIVISIONS] = {
    16.0f, 8.0f, 4.0f, 3.0f, 2.0f, 1.3333f, 1.5f, 1.0f, 0.6667f, 0.75f, 0.5f, 0.3333f, 0.25f
};

static const char* const SYNC_DIVISION_NAMES[NUM_SYNC_DIVISIONS] = {
    "4/1", "2/1", "1/1", "1/2.", "1/2", "1/2T", "1/4.", "1/4", "1/4T", "1/8.", "1/8", "1/8T", "1/16"
};

// Convert sync division index + BPM to LFO rate in Hz
inline float syncDivisionToHz(float divIndex, double bpm) {
    int idx = juce::jlimit(0, NUM_SYNC_DIVISIONS - 1, (int)divIndex);
    float beatsPerCycle = SYNC_DIVISION_BEATS[idx];
    float secondsPerCycle = beatsPerCycle * (60.0f / (float)bpm);
    return 1.0f / secondsPerCycle;
}

//==============================================================================
// SINGLE COEFFICIENT PARAMETER SET
//==============================================================================

struct CoefficientParams {
    // Parameter pointers (connect to APVTS)
    std::atomic<float>* coefficientPtr = nullptr;
    std::atomic<float>* attackTimePtr = nullptr;
    std::atomic<float>* decayTimePtr = nullptr;
    std::atomic<float>* sustainLevelPtr = nullptr;
    std::atomic<float>* releaseTimePtr = nullptr;
    std::atomic<float>* envelopeAmountPtr = nullptr;
    std::atomic<float>* lfoShapePtr = nullptr;
    std::atomic<float>* lfoRatePtr = nullptr;
    std::atomic<float>* lfoAmountPtr = nullptr;
    std::atomic<float>* inputSpectralMultiplierPtr = nullptr;  
    std::atomic<float>* velToHarmonicPtr = nullptr;     // Per-drawbar velocity to harmonic
    std::atomic<float>* lfoSyncPtr = nullptr;            // LFO tempo sync on/off (0=free, 1=synced)
    std::atomic<float>* lfoSyncDivPtr = nullptr;         // LFO sync division index (0-12)
    std::atomic<float>* toPMPtr = nullptr;               // Route to phase-distortion path (0=off, 1=on)
    std::atomic<float>* toOutPtr = nullptr;              // Route direct to output / additive path (0=off, 1=on)
    std::atomic<float>* trigSinglePtr = nullptr;         // Envelope trigger mode (0=Multi/retrigger, 1=Single/phrase-start)
    std::atomic<float>* densityPtr = nullptr;            // Per-drawbar modulator morph sine->soft-saw (0..1, experimental)
    std::atomic<float>* noisePtr = nullptr;              // Per-drawbar Noise switch: hijack the bar into a windowed-noise source (0/1, experimental)

    // Active values (cached at zero-crossings)
    float coefficient = 0.0f;
    float attackTime = 0.01f;
    float decayTime = 0.3f;
    float sustainLevel = 1.0f;
    float releaseTime = 2.0f;
    float envelopeAmount = 0.0f;
    float lfoShape = 1.0f;
    float lfoRate = 1.0f;
    float lfoAmount = 0.0f;
    float inputSpectralMultiplier = 1.0f;    // User input value (snapped to 0.5 steps)
    float velToHarmonic = 0.0f;                         // -100 to +100
    float lfoSync = 0.0f;                               // 0=free, 1=synced to tempo
    float lfoSyncDiv = 7.0f;                            // Sync division index (default 7 = 1/4 note)
    float toPM = 1.0f;                                  // Route to phase distortion (default ON = classic PM operator)
    float toOut = 0.0f;                                 // Route direct to output / additive (default OFF)
    float trigSingle = 0.0f;                            // 0 = Multi (retrigger per note), 1 = Single (fire on phrase start only)
    float density = 0.0f;                               // Per-drawbar modulator density (default OFF = pure sine, patch-compatible)
    float noiseMode = 0.0f;                             // Per-drawbar Noise switch (default OFF = normal drawbar, patch-compatible)

    // Initialize parameter pointers for this coefficient
    void initializePointers(juce::AudioProcessorValueTreeState& apvts, int coeffIndex) {
        std::string prefix = "k" + std::to_string(coeffIndex + 1);

        coefficientPtr = apvts.getRawParameterValue(prefix);
        attackTimePtr = apvts.getRawParameterValue(prefix + "AttackTime");
        decayTimePtr = apvts.getRawParameterValue(prefix + "DecayTime");
        sustainLevelPtr = apvts.getRawParameterValue(prefix + "SustainLevel");
        releaseTimePtr = apvts.getRawParameterValue(prefix + "ReleaseTime");
        envelopeAmountPtr = apvts.getRawParameterValue(prefix + "EnvelopeAmount");
        lfoShapePtr = apvts.getRawParameterValue(prefix + "LFOShape");
        lfoRatePtr = apvts.getRawParameterValue(prefix + "LFORate");
        lfoAmountPtr = apvts.getRawParameterValue(prefix + "LFOAmount");
        inputSpectralMultiplierPtr = apvts.getRawParameterValue("input_f" + std::to_string(coeffIndex + 1));
        velToHarmonicPtr = apvts.getRawParameterValue(prefix + "VelToHarmonic");
        lfoSyncPtr = apvts.getRawParameterValue(prefix + "LFOSync");
        lfoSyncDivPtr = apvts.getRawParameterValue(prefix + "LFOSyncDiv");
        toPMPtr = apvts.getRawParameterValue(prefix + "ToPM");
        toOutPtr = apvts.getRawParameterValue(prefix + "ToOut");
        trigSinglePtr = apvts.getRawParameterValue(prefix + "TrigSingle");
        densityPtr = apvts.getRawParameterValue(prefix + "Density");
        noisePtr = apvts.getRawParameterValue(prefix + "Noise");
    }

    // Update active values from parameter pointers
    void updateActiveValues() {
        if (coefficientPtr) coefficient = coefficientPtr->load();
        if (attackTimePtr) attackTime = attackTimePtr->load();
        if (decayTimePtr) decayTime = decayTimePtr->load();
        if (sustainLevelPtr) sustainLevel = sustainLevelPtr->load();
        if (releaseTimePtr) releaseTime = releaseTimePtr->load();
        if (envelopeAmountPtr) envelopeAmount = envelopeAmountPtr->load();
        if (lfoShapePtr) lfoShape = lfoShapePtr->load();
        if (lfoRatePtr) lfoRate = lfoRatePtr->load();
        if (lfoAmountPtr) lfoAmount = lfoAmountPtr->load();
        if (inputSpectralMultiplierPtr) inputSpectralMultiplier = std::round(inputSpectralMultiplierPtr->load() * 2.0f) / 2.0f; // Round to nearest 0.5
        if (velToHarmonicPtr) velToHarmonic = velToHarmonicPtr->load();
        if (lfoSyncPtr) lfoSync = lfoSyncPtr->load();
        if (lfoSyncDivPtr) lfoSyncDiv = lfoSyncDivPtr->load();
        if (toPMPtr) toPM = toPMPtr->load();
        if (toOutPtr) toOut = toOutPtr->load();
        if (trigSinglePtr) trigSingle = trigSinglePtr->load();
        if (densityPtr) density = densityPtr->load();
        if (noisePtr) noiseMode = noisePtr->load();
    }
};

//==============================================================================
// LIFE VOICING (dev scaffold): tuning constants for the Life doublet engine,
// exposed to a temporary dev panel so the voicing can be found by ear.
// Deliberately NOT APVTS parameters - not saved in patches, not automatable,
// invisible to the host. Once the values are settled they get hard-coded and
// this struct (plus the panel) can be removed or left as a hidden tool.
//==============================================================================
struct LifeVoicingParams {
    // Intensity/Ratio reparameterisation (27 Jun): the dev panel now drives ceiling and
    // response together along the ear-found ridge response = ratio * ceiling. The engine
    // still reads splitCeiling + response directly; the GUI computes them from the ridge.
    static constexpr float kDefaultSplitCeiling = 16.0f;    // C_MAX cents at LIFE=100 ("Intensity") - ear-locked 27 Jun
    static constexpr float kDefaultRatio        = 0.15f;    // response/ceiling (ear-locked 27 Jun, just under the 0.1875 ridge for a touch more body)
    static constexpr float kDefaultResponse     = kDefaultRatio * kDefaultSplitCeiling;  // = 2.4
    static constexpr float kDefaultTilt         = -1.0f;   // bi-directional weight: 0 flat, >0 top-weighted, <0 bottom-weighted (ear-locked 27 Jun)
    static constexpr float kDefaultBeatDepth    = 0.5f;   // 1 = every partial beats through a full null
    static constexpr float kDefaultStrikeSpread = 0.4f;   // per-strike energy-split jitter scale

    std::atomic<float> splitCeiling { kDefaultSplitCeiling };
    std::atomic<float> response     { kDefaultResponse };
    std::atomic<float> tilt         { kDefaultTilt };
    std::atomic<float> beatDepth    { kDefaultBeatDepth };
    std::atomic<float> strikeSpread { kDefaultStrikeSpread };
};

//==============================================================================
// COEFFICIENT ARRAY MANAGER
//==============================================================================

class CoefficientArray {
public:
    using CoeffArray = std::array<CoefficientParams, NUM_COEFFICIENTS>;

    CoefficientArray() = default;

    // Initialize all coefficient parameter pointers
    void initializeFromAPVTS(juce::AudioProcessorValueTreeState& apvts) {
        for (int i = 0; i < NUM_COEFFICIENTS; ++i) {
            coefficients[i].initializePointers(apvts, i);
            coefficients[i].updateActiveValues();  // Initial load
        }
    }

    // Update all active values at zero-crossings
    void updateAllActiveValues() {
        for (auto& coeff : coefficients) {
            coeff.updateActiveValues();
        }
    }

    // Access individual coefficients
    CoefficientParams& operator[](int index) { return coefficients[index]; }
    const CoefficientParams& operator[](int index) const { return coefficients[index]; }

    // Iterator support for range-based loops
    auto begin() { return coefficients.begin(); }
    auto end() { return coefficients.end(); }
    auto begin() const { return coefficients.begin(); }
    auto end() const { return coefficients.end(); }

private:
    CoeffArray coefficients;
};

//==============================================================================
// COEFFICIENT GRID (Active/Staged Buffer) - Keep existing
//==============================================================================

class CoefficientGrid {
public:
    enum class GridRow {
        Active = 0,     // Currently used for audio generation
        Staged = 1      // Calculated but waiting for promotion
    };

    using CoefficientBuffer = std::array<float, NUM_COEFFICIENTS>;
    using GridMatrix = std::array<CoefficientBuffer, 2>;

    CoefficientGrid() {
        grid[static_cast<size_t>(GridRow::Active)].fill(0.0f);
        grid[static_cast<size_t>(GridRow::Staged)].fill(0.0f);
    }

    const CoefficientBuffer& getActiveCoefficients() const {
        return grid[static_cast<size_t>(GridRow::Active)];
    }

    CoefficientBuffer& getStagedCoefficients() {
        return grid[static_cast<size_t>(GridRow::Staged)];
    }

    void promoteStaged() {
        grid[static_cast<size_t>(GridRow::Active)] = grid[static_cast<size_t>(GridRow::Staged)];
    }

private:
    GridMatrix grid;
};

//==============================================================================
// MODULATION STATE (unchanged)
//==============================================================================

enum class EnvelopeStage { Idle, Attack, Decay, Sustain, Release };

struct ModulationState {
    double lfoPhase = 0.0;
    EnvelopeStage envStage = EnvelopeStage::Idle;
    double envTime = 0.0;
    float envLevel = 0.0f;
    float releaseStartLevel = 0.0f;
    float randomLfoValue = 0.0f;  // Stored value for sample-and-hold random LFO
    float lfoFadeLevel = 1.0f;    // LFO fade-in scale reached so far; held through Release/Idle
                                  // so an early note-off can't jump a fading-in LFO to full depth
    float squareLfoSmoothed = 0.0f; // Square-LFO edge softener state (one-pole; see PLANETVoice)
};

using ModulationStateArray = std::array<ModulationState, NUM_COEFFICIENTS>;

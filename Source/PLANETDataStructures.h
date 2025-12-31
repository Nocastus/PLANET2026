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
    std::atomic<float>* inputSpectralMultiplierPtr = nullptr;  // NEW: Input spectral multiplier

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
    float inputSpectralMultiplier = 1.0f;    // NEW: User input value (integer)
    float spectralMultiplier = 1.0f;        // NEW: Effective value (input + LFO)

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
    }

    // NEW: Update effective spectral multiplier with LFO modulation
    void updateSpectralMultiplier(float spectralLfoValue) {
        spectralMultiplier = inputSpectralMultiplier + spectralLfoValue;
    }
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

    void setStagedCoefficient(int index, float value) {
        if (index >= 0 && index < NUM_COEFFICIENTS) {
            grid[static_cast<size_t>(GridRow::Staged)][index] = value;
        }
    }

    void promoteStaged() {
        grid[static_cast<size_t>(GridRow::Active)] = grid[static_cast<size_t>(GridRow::Staged)];
    }

    float getActiveCoefficient(int index) const {
        if (index >= 0 && index < NUM_COEFFICIENTS) {
            return grid[static_cast<size_t>(GridRow::Active)][index];
        }
        return 0.0f;
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
    double lfoPhaseDelta = 0.0;
    EnvelopeStage envStage = EnvelopeStage::Idle;
    double envTime = 0.0;
    float envLevel = 0.0f;
    float releaseStartLevel = 0.0f;

    void reset() {
        lfoPhase = 0.0;
        envStage = EnvelopeStage::Attack;
        envTime = 0.0;
        envLevel = 0.0f;
    }
};

using ModulationStateArray = std::array<ModulationState, NUM_COEFFICIENTS>;

/*
  ==============================================================================
    PLANETEffects.h - Modular Effects Processing
    Template-based design for consistent two-parameter effect modules
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

//==============================================================================
// MODULAR EFFECTS PROCESSOR
//==============================================================================

class PLANETEffects {
public:
    PLANETEffects();

    // Setup
    void prepareToPlay(double sampleRate);

    // Main processing - converts mono input to stereo output with effects
    std::pair<float, float> processStereoSample(float monoInput);

    // Parameter updates (call once per audio block for efficiency)
    void updateDetuneParams(float amount, float mix);
    void updateWarmthParams(float amount);
   

private:
    //==========================================================================
    // DUAL DETUNE EFFECT
    //==========================================================================
    struct DetuneProcessor {
        static constexpr int BUFFER_SIZE = 1024;  // Much smaller buffer (~23ms at 44.1kHz)
        static constexpr float MAX_DETUNE_CENTS = 50.0f;  // More obvious effect
        static constexpr float BASE_DELAY_SAMPLES = 64.0f;  // Small fixed delay (~1.5ms)

        std::array<float, BUFFER_SIZE> buffer;
        int writeIndex = 0;

        // Read positions track write position with rate offsets
        float leftReadOffset = BASE_DELAY_SAMPLES;
        float rightReadOffset = BASE_DELAY_SAMPLES;

        float leftPlaybackRate = 1.0f;
        float rightPlaybackRate = 1.0f;
        float mix = 0.0f;

        // Simple smoothing for buffer wrap artifacts
        float leftPrevSample = 0.0f;
        float rightPrevSample = 0.0f;
        static constexpr float SMOOTHING_FACTOR = 0.95f; // Gentle smoothing

        void updateParameters(float amount, float mixAmount, double sampleRate);
        std::pair<float, float> process(float input);

    private:
        float interpolatedRead(float readPosition);
        float centsToRatio(float cents);
    };
 
    //==========================================================================
    // WARMTH EFFECT (15ips Tape Character)
    //==========================================================================
    struct WarmthProcessor {
        // Low shelf filter at 150Hz
        juce::IIRFilter lowShelfFilter;

        // Pre-emphasis / de-emphasis filters for tape simulation
        juce::IIRFilter preEmphasisFilter;
        juce::IIRFilter deEmphasisFilter;

        float warmthAmount = 0.0f;
        double sampleRate = 44100.0;

        void prepareToPlay(double sr);
        void updateParameters(float amount, double sr);
        float process(float input);

    private:
        float tapeDistortion(float input, float drive);
    };

    //==========================================================================
    // EFFECT INSTANCES
    //==========================================================================
    DetuneProcessor detuneProcessor;
    WarmthProcessor warmthProcessor;
 

    // Audio properties
    double currentSampleRate = 44100.0;


};

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
        // Simple biquad filter state
        struct BiquadFilter {
            float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
            float a1 = 0.0f, a2 = 0.0f;
            float x1 = 0.0f, x2 = 0.0f;
            float y1 = 0.0f, y2 = 0.0f;

            void setLowShelf(double sampleRate, float freq, float q, float gainDB) {
                float A = std::pow(10.0f, gainDB / 40.0f);
                float w0 = 2.0f * juce::MathConstants<float>::pi * freq / (float)sampleRate;
                float cosw0 = std::cos(w0);
                float sinw0 = std::sin(w0);
                float alpha = sinw0 / (2.0f * q);

                float sqrtA = std::sqrt(A);
                float beta = 2.0f * sqrtA * alpha;

                float a0 = (A + 1.0f) + (A - 1.0f) * cosw0 + beta;

                b0 = (A * ((A + 1.0f) - (A - 1.0f) * cosw0 + beta)) / a0;
                b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0)) / a0;
                b2 = (A * ((A + 1.0f) - (A - 1.0f) * cosw0 - beta)) / a0;
                a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0)) / a0;
                a2 = ((A + 1.0f) + (A - 1.0f) * cosw0 - beta) / a0;
            }

            float process(float input) {
                float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

                x2 = x1;
                x1 = input;
                y2 = y1;
                y1 = output;

                return output;
            }

            void reset() {
                x1 = x2 = y1 = y2 = 0.0f;
            }
        };

        BiquadFilter lowShelfFilter;

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

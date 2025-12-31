/*
  ==============================================================================
    PLANETMainGui.h - User-Friendly GUI for PLANET Synthesizer
    Mockup Phase - No parameter binding yet
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>

class PLANETMainGui : public juce::Component
{
public:
    PLANETMainGui();
    ~PLANETMainGui() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void updateAdsrDisplay();  // Update ADSR fields for selected drawbar

    // Envelope drag state
    enum class DragTarget { None, HarmonicAttack, HarmonicDecaySustain, HarmonicRelease,
                            AmpAttack, AmpDecaySustain, AmpRelease };
    DragTarget currentDragTarget = DragTarget::None;
    
    // Envelope graph bounds (calculated in resized, used in mouse handling)
    juce::Rectangle<int> harmonicEnvBounds;
    juce::Rectangle<int> ampEnvBounds;
    
    // Helper methods for envelope interaction
    juce::Point<float> getEnvelopePoint(int pointIndex, const juce::Rectangle<int>& bounds,
                                         float attack, float decay, float sustain, float release);
    void updateAdsrFromDrag(const juce::MouseEvent& event);

private:
    // Colour scheme
    juce::Colour backgroundLight { 0xff3a3a3a };   // Per-harmonic zone (lighter grey)
    juce::Colour backgroundGlobal { 0xff2a2a4a };  // Global controls zone (brighter blue-purple)
    juce::Colour accentColour { 0xff6ab0ff };      // Highlights (brighter blue)

    // Drawbar colours (resistor code inspired)
    std::array<juce::Colour, 10> drawbarColours {
        juce::Colour(0xff4a4a4a),   // 1 - dark grey
        juce::Colour(0xff8b4513),   // 2 - brown
        juce::Colour(0xffcc0000),   // 3 - red
        juce::Colour(0xffff6600),   // 4 - orange
        juce::Colour(0xffcccc00),   // 5 - yellow
        juce::Colour(0xff00aa00),   // 6 - green
        juce::Colour(0xff0066cc),   // 7 - blue
        juce::Colour(0xff6600cc),   // 8 - violet
        juce::Colour(0xff666666),   // 9 - grey
        juce::Colour(0xffeeeeee)    // 10 - white
    };

    // Layout proportions
    static constexpr float leftWidthRatio = 0.67f;   // Left 2/3
    static constexpr float rightWidthRatio = 0.33f;  // Right 1/3
    static constexpr int patchBarHeight = 40;
    static constexpr int drawbarSectionHeight = 200;

    // Drawbar components
    std::array<juce::Slider, 10> drawbarSliders;
    std::array<juce::Label, 10> fValueLabels;
    int selectedDrawbar = 0;  // Currently selected drawbar (0-9)

    // Harmonic ADSR display
    std::array<juce::Label, 4> adsrLabels;      // A, D, S, R text labels
    std::array<juce::Label, 4> adsrValueEditors; // Editable value fields
    float adsrValues[10][4] = {};  // [drawbar][A/D/S/R] - placeholder values

    // Harmonic LFO controls
    juce::ComboBox lfoShapeCombo;
    juce::Slider lfoSpeedKnob;
    juce::Slider lfoDepthKnob;
    juce::Label lfoShapeLabel, lfoSpeedLabel, lfoDepthLabel;
    juce::Label selectedFDisplay;  // Shows F value of selected drawbar

    // Envelope depth control
    juce::Slider envDepthKnob;
    juce::Label envDepthLabel;
    juce::Label envDepthValue;

    // Amplitude ADSR display
    std::array<juce::Label, 4> ampAdsrLabels;      // A, D, S, R text labels
    std::array<juce::Label, 4> ampAdsrValueEditors; // Editable value fields
    float ampAdsrValues[4] = { 0.1f, 0.3f, 0.7f, 0.5f };  // Global amp envelope values

    // Velocity to Amplitude control
    juce::Slider velAmpSlider;
    juce::Label velAmpLabel;
    juce::Label velAmpValue;

    // Amplitude zone knobs (2x2 grid)
    juce::Slider velBrillKnob, velAttackKnob, envCurveKnob, vintageKnob;
    juce::Label velBrillLabel, velAttackLabel, envCurveLabel, vintageLabel;
    juce::Label velBrillValue, velAttackValue, envCurveValue, vintageValue;

    // ======================== RIGHT COLUMN CONTROLS ========================
    
    // Waveform display (placeholder for now)
    // Will be implemented as custom painting + circular buffer later
    
    // Vibrato section
    juce::Slider vibratoRateKnob, vibratoDepthKnob, vibratoFadeKnob;
    juce::Label vibratoRateLabel, vibratoDepthLabel, vibratoFadeLabel;
    
    // Pitch section
    juce::Slider pitchDistKnob, pitchTimeKnob;
    juce::Label pitchDistLabel, pitchTimeLabel;
    
    // Brilliance section (horizontal slider)
    juce::Slider brillianceSlider;
    juce::Label brillianceMainLabel;
    
    // Effects section (vertical sliders)
    juce::Slider detuneAmountSlider, detuneMixSlider;
    juce::Slider reverbTimeSlider, reverbMixSlider;
    juce::Label detuneAmountLabel, detuneMixLabel;
    juce::Label reverbTimeLabel, reverbMixLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PLANETMainGui)
};

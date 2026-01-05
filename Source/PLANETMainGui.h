/*
  ==============================================================================
    PLANETMainGui.h - User-Friendly GUI for PLANET Synthesizer
    With parameter binding to audio engine
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "WaveformDisplay.h"
#include <array>

class PLANETMainGui : public juce::Component,
                       public juce::AudioProcessorValueTreeState::Listener,
                       public juce::Timer
{
public:
    PLANETMainGui(juce::AudioProcessorValueTreeState& apvts,
        std::atomic<float>* modWheelPtr = nullptr,
        std::array<float, 2048>* waveformSnapshotPtr = nullptr,
        std::atomic<int>* snapshotLengthPtr = nullptr,
        std::atomic<bool>* snapshotReadyPtr = nullptr,
        std::atomic<bool>* snapshotRequestPtr = nullptr,
        std::atomic<bool>* waveformActivePtr = nullptr);
    ~PLANETMainGui() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void updateAdsrDisplay();
    void timerCallback() override;
    void updateDrawbarColors();
    
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void bindToSelectedDrawbar();

    // Envelope drag state
    enum class DragTarget { None, HarmonicAttack, HarmonicDecaySustain, HarmonicRelease,
                            AmpAttack, AmpDecaySustain, AmpRelease };
    DragTarget currentDragTarget = DragTarget::None;
    
    // Envelope graph bounds
    juce::Rectangle<int> harmonicEnvBounds;
    juce::Rectangle<int> ampEnvBounds;
    
    // Helper methods for envelope interaction
    juce::Point<float> getEnvelopePoint(int pointIndex, const juce::Rectangle<int>& bounds,
                                         float attack, float decay, float sustain, float release);
    void updateAdsrFromDrag(const juce::MouseEvent& event);
    
    // Envelope drawing helper - consolidates duplicate code
    void drawEnvelopeCurve(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                           float attack, float decay, float sustain, float release,
                           float curveAmount, juce::Colour strokeColour, juce::Colour handleOutlineColour);

private:
    // Colour scheme
    juce::Colour backgroundLight { 0xff3a3a3a };
    juce::Colour backgroundGlobal { 0xff2a2a4a };
    juce::Colour accentColour { 0xff6ab0ff };

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
    static constexpr float leftWidthRatio = 0.67f;
    static constexpr float rightWidthRatio = 0.33f;
    static constexpr int patchBarHeight = 40;
    static constexpr int drawbarSectionHeight = 200;

    // Reference to APVTS
    juce::AudioProcessorValueTreeState& apvts;
    
    // Drawbar components
    std::array<juce::Slider, 10> drawbarSliders;
    std::array<juce::Label, 10> fValueLabels;
    int selectedDrawbar = 0;
    
    // SliderAttachments for K1-K10 drawbars
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 10> drawbarAttachments;

    // Harmonic ADSR display
    std::array<juce::Label, 4> adsrLabels;
    std::array<juce::Label, 4> adsrValueEditors;
    float adsrValues[10][4] = {};
    
    // Current harmonic parameter IDs
    juce::String currentHarmonicParamIDs[4];

    // Harmonic LFO controls
    juce::ComboBox lfoShapeCombo;
    juce::Slider lfoSpeedKnob;
    juce::Slider lfoDepthKnob;
    juce::Label lfoShapeLabel, lfoSpeedLabel, lfoDepthLabel;
    juce::Label selectedFDisplay;

    // Envelope depth control
    juce::Slider envDepthKnob;
    juce::Label envDepthLabel;
    juce::Label envDepthValue;

    // Amplitude ADSR display
    std::array<juce::Label, 4> ampAdsrLabels;
    std::array<juce::Label, 4> ampAdsrValueEditors;
    float ampAdsrValues[4] = { 0.1f, 0.3f, 0.7f, 0.5f };

    // Velocity to Amplitude control
    juce::Slider velAmpSlider;
    juce::Label velAmpLabel;
    juce::Label velAmpValue;

    // Amplitude zone knobs (2x2 grid)
    juce::Slider velBrillKnob, velAttackKnob, envCurveKnob, vintageKnob;
    juce::Label velBrillLabel, velAttackLabel, envCurveLabel, vintageLabel;
    juce::Label velBrillValue, velAttackValue, envCurveValue, vintageValue;

    // ======================== RIGHT COLUMN CONTROLS ========================
    
    // Vibrato section
    juce::Slider vibratoRateKnob, vibratoDepthKnob, vibratoFadeKnob;
    juce::Label vibratoRateLabel, vibratoDepthLabel, vibratoFadeLabel;
    
    // Pitch section
    juce::Slider pitchDistKnob, pitchTimeKnob;
    juce::Label pitchDistLabel, pitchTimeLabel;
    
    // Brilliance section
    juce::Slider brillianceSlider;
    juce::Label brillianceMainLabel;
    
    // Mod wheel tracking
    std::atomic<float>* modWheelValue = nullptr;
    juce::Rectangle<int> brillianceSliderBounds;
    float cachedEffectiveBrilliance = 0.5f;

    // Waveform display component
    WaveformDisplay waveformDisplay;
    
    // Effects section
    juce::Slider detuneAmountSlider, detuneMixSlider;
     
    juce::Label detuneAmountLabel, detuneMixLabel;
     
    
    // ======================== SLIDER ATTACHMENTS ========================
    
    // Right column attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vibratoRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vibratoDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vibratoFadeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchDistAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchTimeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> brillianceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> detuneAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> detuneMixAttachment;
   
   
    
    // Amplitude section attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velAmpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velBrillAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velAttackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envCurveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vintageAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envDepthAttachment;
    
    // Context-sensitive attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoSpeedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoShapeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PLANETMainGui)
};

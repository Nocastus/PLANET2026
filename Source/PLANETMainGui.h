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
#include "IshtarLookAndFeel.h"
#include "PLANETDataStructures.h"


//==============================================================================
// Custom LookAndFeel for drawbar sliders with LFO and VelToHarmonic visual feedback
//==============================================================================
class DrawbarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearVertical)
        {
            auto trackWidth = juce::jmin(6.0f, (float)width * 0.25f);
            auto trackLeft = x + (width - trackWidth) * 0.5f;

            // Draw track
            g.setColour(slider.findColour(juce::Slider::backgroundColourId));
            g.fillRect(juce::Rectangle<float>(trackLeft, (float)y, trackWidth, (float)height));

            // Calculate thumb position
            auto sliderPosProportional = (float)slider.valueToProportionOfLength(slider.getValue());
            auto thumbCenterX = x + width * 0.5f;
            auto thumbCenterY = y + height * (1.0f - sliderPosProportional);
            float thumbRadius = 7.0f;

            // Check modulation states
            bool hasLFO = slider.getProperties()["hasActiveLFO"];
            bool hasVelHarm = slider.getProperties()["hasActiveVelHarm"];

            if (hasVelHarm)
            {
                // Draw Ishtar star instead of circle (scaled up for visibility)
                float starRadius = thumbRadius * 1.8f;
                drawMiniIshtarStar(g, thumbCenterX, thumbCenterY, starRadius,
                    slider.findColour(juce::Slider::thumbColourId), hasLFO);
            }
            else
            {
                // Draw standard circle thumb
                g.setColour(slider.findColour(juce::Slider::thumbColourId));
                g.fillEllipse(thumbCenterX - thumbRadius, thumbCenterY - thumbRadius,
                    thumbRadius * 2, thumbRadius * 2);

                // If LFO is active, draw pale outline stroke (larger than thumb)
                if (hasLFO)
                {
                    float outlineRadius = 10.0f;  // Original thumb size, now used just for LFO ring
                    g.setColour(juce::Colour(0x99ffffff));
                    g.drawEllipse(thumbCenterX - outlineRadius, thumbCenterY - outlineRadius,
                        outlineRadius * 2, outlineRadius * 2, 3.0f);
                }
            }
        }
        else
        {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                minSliderPos, maxSliderPos, style, slider);
        }
    }

private:
    void drawMiniIshtarStar(juce::Graphics& g, float cx, float cy, float radius,
        juce::Colour fillColour, bool hasLFO)
    {
        float outerRadius = radius;           // Ray tips extend to here
        float orbitRadius = radius * 0.6f;    // Orbit ring sits inside the rays
        float innerRadius = radius * 0.4f;

        // Draw outer circle (like orbit) - sized to match normal thumb LFO ring
        if (hasLFO)
        {
            g.setColour(juce::Colour(0x99ffffff));
            g.drawEllipse(cx - orbitRadius, cy - orbitRadius,
                orbitRadius * 2, orbitRadius * 2, 3.0f);
        }

        // Draw inner filled circle
        g.setColour(fillColour);
        g.fillEllipse(cx - innerRadius, cy - innerRadius,
            innerRadius * 2, innerRadius * 2);

        // Draw 8 rays
        float rayStroke = 1.5f;
        g.setColour(fillColour);

        for (int i = 0; i < 8; ++i)
        {
            float angle = i * juce::MathConstants<float>::pi / 4.0f;

            // Ray from inner circle edge to outer circle edge
            float innerX = cx + innerRadius * std::sin(angle);
            float innerY = cy - innerRadius * std::cos(angle);
            float outerX = cx + outerRadius * std::sin(angle);
            float outerY = cy - outerRadius * std::cos(angle);

            g.drawLine(innerX, innerY, outerX, outerY, rayStroke);
        }
    }
};

//==============================================================================
// ComboBox that never holds keyboard focus, so DAW transport keys (e.g. keypad
// Enter in Cubase) reach the host instead of being swallowed by the combo.
//
// setWantsKeyboardFocus(false) / setMouseClickGrabsKeyboardFocus(false) are NOT
// enough: after the popup closes, ComboBox's popup-finished callback calls
// getAccessibilityHandler()->grabFocus() (see juce_ComboBox.cpp), which puts
// focus back on the combo regardless of those flags. Once focused, ComboBox::
// keyPressed consumes Return by re-opening the popup. We bounce focus away the
// instant it is gained (covers the accessibility grab and every other path), so
// the combo never gets to consume keys. These combos are non-editable and
// mouse-operated, so they never legitimately need focus.
//==============================================================================
class FocuslessComboBox : public juce::ComboBox
{
public:
    FocuslessComboBox()
    {
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
    }

    void focusGained(FocusChangeType) override
    {
        // Don't recurse while the give-away is in flight.
        if (givingAway)
            return;

        const juce::ScopedValueSetter<bool> guard(givingAway, true);
        giveAwayKeyboardFocus();
    }

    // Belt-and-suspenders: if focus ever lands here anyway, don't consume the
    // keys ComboBox normally eats (Return/arrows) — let them propagate to host.
    bool keyPressed(const juce::KeyPress&) override { return false; }
    bool keyStateChanged(bool) override            { return false; }

private:
    bool givingAway = false;
};

//==============================================================================
// Main GUI Component
//==============================================================================
class PLANETMainGui : public juce::Component,
                       public juce::AudioProcessorValueTreeState::Listener,
                       public juce::Timer
{
public:
    PLANETMainGui(juce::AudioProcessorValueTreeState& apvts,
        juce::AudioProcessor* processor = nullptr,
        std::atomic<float>* rawModWheelPtr = nullptr,
        std::atomic<bool>* modWheelEngagedPtr = nullptr,
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
    void refreshAllGUIValues();  // Refresh all GUI elements from current parameter values
    void updatePatchNameDisplay(const juce::String& name);
    void updatePatchCommentDisplay(const juce::String& comment);
    void setLifeVoicingParams(LifeVoicingParams* p);   // wires the dev panel to the processor

    // Flag to suppress GUI updates during bulk parameter loading
    bool suppressParameterUpdates = false;

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
    static constexpr int drawbarSectionHeight = 220;

    // Reference to APVTS
    juce::AudioProcessorValueTreeState& apvts;
    juce::AudioProcessor* audioProcessor = nullptr;

    // Drawbar components
    std::array<juce::Slider, 10> drawbarSliders;
    std::array<juce::Label, 10> fValueLabels;
    int selectedDrawbar = 0;
    DrawbarLookAndFeel drawbarLookAndFeel;  // Custom LookAndFeel for LFO visual feedback

    IshtarLookAndFeel ishtarLookAndFeel;  // Custom rotary knob appearance

    // Custom fonts
    juce::Font amarnaRegular;
    juce::Font amarnaSemiBold;

    // SliderAttachments for K1-K10 drawbars
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 10> drawbarAttachments;

    // Harmonic ADSR display
    std::array<juce::Label, 4> adsrLabels;
    std::array<juce::Label, 4> adsrValueEditors;
    float adsrValues[10][4] = {};
    
    // Current harmonic parameter IDs
    juce::String currentHarmonicParamIDs[4];

    // Harmonic LFO controls
    FocuslessComboBox lfoShapeCombo;
    FocuslessComboBox lfoSyncCombo;
    juce::Slider lfoSpeedKnob;
    juce::Slider lfoDepthKnob;
    juce::Label lfoShapeLabel, lfoSyncLabel, lfoSpeedLabel, lfoDepthLabel;
    juce::Label selectedFDisplay;
    juce::Label lfoSpeedValue, lfoDepthValue;
    bool currentSyncMode = false;  // Track whether speed knob is in sync mode
    void updateLfoSyncMode();      // Switch speed knob between Hz and sync division modes


    // Envelope depth control
    juce::Slider envDepthKnob;
    juce::Label envDepthLabel;
    juce::Label envDepthValue;

    // Velocity to Drawbar control (context-sensitive)
    juce::Slider velToDrawbarKnob;
    juce::Label velToDrawbarLabel;
    juce::Label velToDrawbarValue;

    // Amplitude ADSR display
    std::array<juce::Label, 4> ampAdsrLabels;
    std::array<juce::Label, 4> ampAdsrValueEditors;
    float ampAdsrValues[4] = { 0.1f, 0.3f, 0.7f, 0.5f };

    // Velocity to Amplitude control
    juce::Slider velAmpSlider;
    juce::Label velAmpLabel;
    juce::Label velAmpValue;

    // Amplitude / Character zone knobs (2x2 grid: Vel->Attk, Env Curve / Vintage, Life)
    juce::Slider velAttackKnob, envCurveKnob, vintageKnob, lifeKnob;
    juce::Label velAttackLabel, envCurveLabel, vintageLabel, lifeLabel;
    juce::Label velAttackValue, envCurveValue, vintageValue, lifeValue;

    // LIFE seed: editable readout (type to recall) + Star-of-Ishtar "re-cast" button.
    juce::Label seedLabel, seedValue;
    juce::ShapeButton rerollButton { "reroll", juce::Colour(0xff6ab0ff),
                                     juce::Colour(0xffaad4ff), juce::Colour(0xffffffff) };
    // Bolt anchors (set in resized) + flash level (0..1, pulsed on re-cast, decayed in timer).
    juce::Rectangle<int> lifeKnobBounds, seedModuleBounds;
    float boltFlash = 0.0f;
    void rerollSeed();

    // ---- LIFE voicing dev panel (temporary tuning scaffold) ----
    // Shift-click the seed star to toggle. Functional-ugly by design: orange border
    // so it can't be mistaken for shipping GUI. Sliders write straight to the
    // processor's LifeVoicingParams atomics; Snapshot dumps them to a text file.
    struct VoicingPanel : juce::Component {
        void paint(juce::Graphics& g) override {
            g.fillAll(juce::Colour(0xf0141420));
            g.setColour(juce::Colours::orange);
            g.drawRect(getLocalBounds(), 2);
            g.drawText("LIFE VOICING (dev)", 10, 6, getWidth() - 20, 16, juce::Justification::left);
        }
    };
    VoicingPanel voicingPanel;
    static constexpr int NUM_VOICING_SLIDERS = 5;
    std::array<juce::Slider, NUM_VOICING_SLIDERS> voicingSliders;
    std::array<juce::Label, NUM_VOICING_SLIDERS> voicingSliderLabels;
    juce::TextButton voicingSnapshotButton { "Snapshot" };
    juce::TextButton voicingCloseButton { "x" };   // dismiss the dev panel (top-right corner)
    juce::Label voicingSavedLabel;
    LifeVoicingParams* lifeVoicingParams = nullptr;
    void toggleVoicingPanel();
    void saveVoicingSnapshot();

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
    std::atomic<float>* rawModWheelValue = nullptr;
    std::atomic<bool>* modWheelEngaged = nullptr;
    juce::Rectangle<int> brillianceSliderBounds;
    float cachedEffectiveBrilliance = 0.5f;

    // Waveform display component
    WaveformDisplay waveformDisplay;
    
    // Effects section
    juce::Slider detuneAmountSlider, detuneMixSlider;
    juce::Label detuneAmountLabel, detuneMixLabel;
    juce::Slider warmthSlider;
    juce::Label warmthLabel;
    juce::Slider punchSlider, punchFrequencySlider;
    juce::Label punchLabel, punchFrequencyLabel;
    juce::Label detuneAmountValue, detuneMixValue, warmthValue, punchValue, punchFrequencyValue;
    juce::Label brillianceValue;
    juce::Label vibratoRateValue, vibratoDepthValue, vibratoFadeValue;
    juce::Label pitchDistValue, pitchTimeValue;

    // ======================== PATCH MANAGEMENT UI ========================
    juce::TextButton loadPatchButton;
    juce::TextButton savePatchButton;
    juce::Label currentPatchLabel;
    juce::String currentPatchName;
    juce::Label patchCommentLabel;

    // Master controls in patch bar
    juce::Slider masterVolumeSlider;
    juce::Label masterVolumeLabel;
    juce::Label transposeLabel;
    juce::Label transposeValue;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterVolumeAttachment;

    void loadPatchButtonClicked();
    void savePatchButtonClicked();
   

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
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> warmthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> punchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> punchFrequencyAttachment;
   
   
    
    // Amplitude section attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velAmpAttachment;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velAttackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envCurveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vintageAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lifeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envDepthAttachment;
    
    // Context-sensitive attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoSpeedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoShapeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoSyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoSyncDivAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velToDrawbarAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PLANETMainGui)
};

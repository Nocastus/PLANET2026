/*
  ==============================================================================
    PLANETMainGui.cpp - User-Friendly GUI for PLANET Synthesizer
    With parameter binding to audio engine
  ==============================================================================
*/

#include "PLANETMainGui.h"

PLANETMainGui::PLANETMainGui(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    // Set up drawbar sliders
    for (int i = 0; i < 10; ++i)
    {
        drawbarSliders[i].setSliderStyle(juce::Slider::LinearVertical);
        drawbarSliders[i].setRange(-2.0, 2.0, 0.01);
        drawbarSliders[i].setValue(0.0);
        drawbarSliders[i].setDoubleClickReturnValue(true, 0.0);
        drawbarSliders[i].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        drawbarSliders[i].setSliderSnapsToMousePosition(false);  // Don't jump to click
        drawbarSliders[i].addMouseListener(this, false);  // Pass clicks to parent
        addAndMakeVisible(drawbarSliders[i]);
    }

    // Set up F value labels (editable)
    for (int i = 0; i < 10; ++i)
    {
        fValueLabels[i].setText(juce::String(i + 1), juce::dontSendNotification);
        fValueLabels[i].setEditable(true);
        fValueLabels[i].setJustificationType(juce::Justification::centred);
        fValueLabels[i].setColour(juce::Label::backgroundColourId, juce::Colours::black);
        fValueLabels[i].setColour(juce::Label::outlineColourId, juce::Colours::grey);
        fValueLabels[i].setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(fValueLabels[i]);
    }

    // Initialize ADSR placeholder values for each drawbar
    for (int i = 0; i < 10; ++i)
    {
        adsrValues[i][0] = 0.1f;   // Attack
        adsrValues[i][1] = 0.3f;   // Decay
        adsrValues[i][2] = 0.5f;   // Sustain (0-1 level)
        adsrValues[i][3] = 0.5f;   // Release
    }

    // Set up ADSR labels and value editors
    const char* adsrNames[] = { "A", "D", "S", "R" };
    for (int i = 0; i < 4; ++i)
    {
        adsrLabels[i].setText(adsrNames[i], juce::dontSendNotification);
        adsrLabels[i].setJustificationType(juce::Justification::centred);
        adsrLabels[i].setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(adsrLabels[i]);

        adsrValueEditors[i].setText(juce::String(adsrValues[0][i], 2), juce::dontSendNotification);
        adsrValueEditors[i].setEditable(true);
        adsrValueEditors[i].setJustificationType(juce::Justification::centred);
        adsrValueEditors[i].setColour(juce::Label::backgroundColourId, juce::Colours::black);
        adsrValueEditors[i].setColour(juce::Label::outlineColourId, juce::Colours::grey);
        adsrValueEditors[i].setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(adsrValueEditors[i]);
    }

    // Set up LFO shape dropdown
    lfoShapeCombo.addItem("Sine", 1);
    lfoShapeCombo.addItem("Triangle", 2);
    lfoShapeCombo.addItem("Square", 3);
    lfoShapeCombo.setSelectedId(1);
    addAndMakeVisible(lfoShapeCombo);

    lfoShapeLabel.setText("LFO Shape", juce::dontSendNotification);
    lfoShapeLabel.setJustificationType(juce::Justification::centredRight);
    lfoShapeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(lfoShapeLabel);

    // Set up LFO speed knob
    lfoSpeedKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lfoSpeedKnob.setRange(0.05, 20.0, 0.01);
    lfoSpeedKnob.setValue(4.0);
    lfoSpeedKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
    addAndMakeVisible(lfoSpeedKnob);

    lfoSpeedLabel.setText("Speed", juce::dontSendNotification);
    lfoSpeedLabel.setJustificationType(juce::Justification::centred);
    lfoSpeedLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(lfoSpeedLabel);

    // Set up LFO depth knob
    lfoDepthKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lfoDepthKnob.setRange(0.0, 5.0, 0.01);
    lfoDepthKnob.setValue(0.0);
    lfoDepthKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
    addAndMakeVisible(lfoDepthKnob);

    lfoDepthLabel.setText("Depth", juce::dontSendNotification);
    lfoDepthLabel.setJustificationType(juce::Justification::centred);
    lfoDepthLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(lfoDepthLabel);

    // Set up selected F display - white background for contrast
    selectedFDisplay.setText("F1", juce::dontSendNotification);
    selectedFDisplay.setFont(juce::Font(24.0f, juce::Font::bold));
    selectedFDisplay.setJustificationType(juce::Justification::centred);
    selectedFDisplay.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    selectedFDisplay.setColour(juce::Label::textColourId, drawbarColours[0]);
    addAndMakeVisible(selectedFDisplay);

    // Set up envelope depth slider (vertical, center zero)
    envDepthKnob.setSliderStyle(juce::Slider::LinearVertical);
    envDepthKnob.setRange(-5.0, 20.0, 0.01);
    envDepthKnob.setValue(0.0);
    envDepthKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    envDepthKnob.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(envDepthKnob);

    envDepthLabel.setText("Env Depth", juce::dontSendNotification);
    envDepthLabel.setJustificationType(juce::Justification::centred);
    envDepthLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(envDepthLabel);

    // Env depth value display
    envDepthValue.setText("0.00", juce::dontSendNotification);
    envDepthValue.setJustificationType(juce::Justification::centred);
    envDepthValue.setColour(juce::Label::textColourId, juce::Colours::white);
    envDepthValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    envDepthValue.setEditable(true);
    addAndMakeVisible(envDepthValue);

    // Set up Amplitude ADSR labels and value editors
    const char* ampAdsrNames[] = { "A", "D", "S", "R" };
    for (int i = 0; i < 4; ++i)
    {
        ampAdsrLabels[i].setText(ampAdsrNames[i], juce::dontSendNotification);
        ampAdsrLabels[i].setJustificationType(juce::Justification::centred);
        ampAdsrLabels[i].setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(ampAdsrLabels[i]);

        ampAdsrValueEditors[i].setText(juce::String(ampAdsrValues[i], 2), juce::dontSendNotification);
        ampAdsrValueEditors[i].setEditable(true);
        ampAdsrValueEditors[i].setJustificationType(juce::Justification::centred);
        ampAdsrValueEditors[i].setColour(juce::Label::backgroundColourId, juce::Colours::black);
        ampAdsrValueEditors[i].setColour(juce::Label::outlineColourId, juce::Colours::grey);
        ampAdsrValueEditors[i].setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(ampAdsrValueEditors[i]);
    }

    // Set up Velocity to Amplitude slider (vertical, center zero)
    velAmpSlider.setSliderStyle(juce::Slider::LinearVertical);
    velAmpSlider.setRange(0.0, 200.0, 1.0);
    velAmpSlider.setValue(100.0);
    velAmpSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    velAmpSlider.setDoubleClickReturnValue(true, 100.0);
    addAndMakeVisible(velAmpSlider);

    velAmpLabel.setText("Vel Ampli", juce::dontSendNotification);
    velAmpLabel.setJustificationType(juce::Justification::centred);
    velAmpLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(velAmpLabel);

    velAmpValue.setText("100", juce::dontSendNotification);
    velAmpValue.setJustificationType(juce::Justification::centred);
    velAmpValue.setColour(juce::Label::textColourId, juce::Colours::white);
    velAmpValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    velAmpValue.setEditable(true);
    addAndMakeVisible(velAmpValue);

    // Set up Vel to Brilliance knob
    velBrillKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    velBrillKnob.setRange(-100.0, 100.0, 1.0);
    velBrillKnob.setValue(100.0);
    velBrillKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    velBrillKnob.setDoubleClickReturnValue(true, 100.0);
    addAndMakeVisible(velBrillKnob);
    velBrillLabel.setText("Vel Brill", juce::dontSendNotification);
    velBrillLabel.setJustificationType(juce::Justification::centred);
    velBrillLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(velBrillLabel);
    velBrillValue.setText("100", juce::dontSendNotification);
    velBrillValue.setJustificationType(juce::Justification::centred);
    velBrillValue.setColour(juce::Label::textColourId, juce::Colours::white);
    velBrillValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    velBrillValue.setEditable(true);
    addAndMakeVisible(velBrillValue);

    // Set up Vel to Attack knob
    velAttackKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    velAttackKnob.setRange(0.0, 100.0, 1.0);
    velAttackKnob.setValue(0.0);
    velAttackKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    velAttackKnob.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(velAttackKnob);
    velAttackLabel.setText("Vel Attk", juce::dontSendNotification);
    velAttackLabel.setJustificationType(juce::Justification::centred);
    velAttackLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(velAttackLabel);
    velAttackValue.setText("0", juce::dontSendNotification);
    velAttackValue.setJustificationType(juce::Justification::centred);
    velAttackValue.setColour(juce::Label::textColourId, juce::Colours::white);
    velAttackValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    velAttackValue.setEditable(true);
    addAndMakeVisible(velAttackValue);

    // Set up Env Curve knob
    envCurveKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    envCurveKnob.setRange(0.0, 1.0, 0.01);
    envCurveKnob.setValue(0.5);
    envCurveKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    envCurveKnob.setDoubleClickReturnValue(true, 0.5);
    envCurveKnob.onValueChange = [this]() { repaint(); };  // Repaint envelopes when curve changes
    addAndMakeVisible(envCurveKnob);
    envCurveLabel.setText("Env Curve", juce::dontSendNotification);
    envCurveLabel.setJustificationType(juce::Justification::centred);
    envCurveLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(envCurveLabel);
    envCurveValue.setText("0.50", juce::dontSendNotification);
    envCurveValue.setJustificationType(juce::Justification::centred);
    envCurveValue.setColour(juce::Label::textColourId, juce::Colours::white);
    envCurveValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    envCurveValue.setEditable(true);
    addAndMakeVisible(envCurveValue);

    // Set up Vintage knob
    vintageKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    vintageKnob.setRange(0.0, 100.0, 1.0);
    vintageKnob.setValue(0.0);
    vintageKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    vintageKnob.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(vintageKnob);
    vintageLabel.setText("Vintage", juce::dontSendNotification);
    vintageLabel.setJustificationType(juce::Justification::centred);
    vintageLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(vintageLabel);
    vintageValue.setText("0", juce::dontSendNotification);
    vintageValue.setJustificationType(juce::Justification::centred);
    vintageValue.setColour(juce::Label::textColourId, juce::Colours::white);
    vintageValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    vintageValue.setEditable(true);
    addAndMakeVisible(vintageValue);

    // ======================== RIGHT COLUMN CONTROLS ========================
    
    // Helper lambda for setting up knobs consistently
    auto setupKnob = [this](juce::Slider& knob, juce::Label& label, const juce::String& name,
                            double min, double max, double defaultVal) {
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setRange(min, max, 0.01);
        knob.setValue(defaultVal);
        knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob.setDoubleClickReturnValue(true, defaultVal);
        addAndMakeVisible(knob);
        
        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(label);
    };
    
    // Vibrato knobs
    setupKnob(vibratoRateKnob, vibratoRateLabel, "Rate", 0.5, 12.0, 5.0);
    setupKnob(vibratoDepthKnob, vibratoDepthLabel, "Depth", 0.0, 2.0, 0.0);
    setupKnob(vibratoFadeKnob, vibratoFadeLabel, "Fade", 0.0, 10.0, 2.0);
    
    // Pitch knobs
    setupKnob(pitchDistKnob, pitchDistLabel, "Distance", -12.0, 12.0, 0.0);
    setupKnob(pitchTimeKnob, pitchTimeLabel, "Time", 0.01, 5.0, 0.5);
    
    // Brilliance horizontal slider
    brillianceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    brillianceSlider.setRange(0.0, 1.0, 0.01);
    brillianceSlider.setValue(0.5);
    brillianceSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    brillianceSlider.setDoubleClickReturnValue(true, 0.5);
    addAndMakeVisible(brillianceSlider);
    brillianceMainLabel.setText("Brilliance", juce::dontSendNotification);
    brillianceMainLabel.setJustificationType(juce::Justification::centred);
    brillianceMainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(brillianceMainLabel);
    
    // Effects vertical sliders
    auto setupVerticalSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& name,
                                      double min, double max, double defaultVal) {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setRange(min, max, 0.01);
        slider.setValue(defaultVal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setDoubleClickReturnValue(true, defaultVal);
        addAndMakeVisible(slider);
        
        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(label);
    };
    
    setupVerticalSlider(detuneAmountSlider, detuneAmountLabel, "Detune", 0.0, 1.0, 0.0);
    setupVerticalSlider(detuneMixSlider, detuneMixLabel, "Det Mix", 0.0, 1.0, 0.0);
    setupVerticalSlider(reverbTimeSlider, reverbTimeLabel, "Reverb", 0.0, 1.0, 0.3);
    setupVerticalSlider(reverbMixSlider, reverbMixLabel, "Rev Mix", 0.0, 1.0, 0.0);

    // ======================== CREATE SLIDER ATTACHMENTS ========================
    
    // K1-K10 drawbar attachments
    const char* kParamNames[] = { "k1", "k2", "k3", "k4", "k5", "k6", "k7", "k8", "k9", "k10" };
    for (int i = 0; i < 10; ++i)
    {
        drawbarAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, kParamNames[i], drawbarSliders[i]);
    }
    
    // Right column attachments (simple 1:1 bindings)
    vibratoRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "vibratoRate", vibratoRateKnob);
    vibratoDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "vibratoDepth", vibratoDepthKnob);
    vibratoFadeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "vibratoFadeIn", vibratoFadeKnob);
    pitchDistAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "pitchEnvDistance", pitchDistKnob);
    pitchTimeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "pitchEnvTime", pitchTimeKnob);
    brillianceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "brilliance", brillianceSlider);
    detuneAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "detuneAmount", detuneAmountSlider);
    detuneMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "detuneMix", detuneMixSlider);
    reverbTimeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "reverbLength", reverbTimeSlider);
    reverbMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "reverbMix", reverbMixSlider);
    
    // Amplitude section attachments
    velAmpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "velToAmplitude", velAmpSlider);
    velBrillAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "velToBrilliance", velBrillKnob);
    velAttackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "velToAttackTime", velAttackKnob);
    envCurveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "exponentialControl", envCurveKnob);
    vintageAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "vintageAmount", vintageKnob);
    
    // Initial binding to selected drawbar (K1)
    bindToSelectedDrawbar();
    
    // Load initial ADSR values from parameters
    for (int d = 0; d < 10; ++d)
    {
        juce::String prefix = "k" + juce::String(d + 1);
        if (auto* param = apvts.getParameter(prefix + "AttackTime"))
            adsrValues[d][0] = param->convertFrom0to1(param->getValue());
        if (auto* param = apvts.getParameter(prefix + "DecayTime"))
            adsrValues[d][1] = param->convertFrom0to1(param->getValue());
        if (auto* param = apvts.getParameter(prefix + "SustainLevel"))
            adsrValues[d][2] = param->convertFrom0to1(param->getValue());
        if (auto* param = apvts.getParameter(prefix + "ReleaseTime"))
            adsrValues[d][3] = param->convertFrom0to1(param->getValue());
    }
    
    // Load amplitude envelope values
    if (auto* param = apvts.getParameter("ampEnvAttackTime"))
        ampAdsrValues[0] = param->convertFrom0to1(param->getValue());
    if (auto* param = apvts.getParameter("ampEnvDecayTime"))
        ampAdsrValues[1] = param->convertFrom0to1(param->getValue());
    if (auto* param = apvts.getParameter("ampEnvSustainLevel"))
        ampAdsrValues[2] = param->convertFrom0to1(param->getValue());
    if (auto* param = apvts.getParameter("ampEnvReleaseTime"))
        ampAdsrValues[3] = param->convertFrom0to1(param->getValue());
    
    // Load F values from parameters
    for (int i = 0; i < 10; ++i)
    {
        if (auto* param = apvts.getParameter("input_f" + juce::String(i + 1)))
        {
            float fVal = param->convertFrom0to1(param->getValue());
            fValueLabels[i].setText(juce::String(fVal, 1), juce::dontSendNotification);
        }
    }
    
    // Wire up F value label editing to write back to APVTS
    for (int i = 0; i < 10; ++i)
    {
        fValueLabels[i].onTextChange = [this, i]() {
            float newVal = fValueLabels[i].getText().getFloatValue();
            newVal = std::round(newVal * 2.0f) / 2.0f;  // Round to nearest 0.5
            newVal = juce::jlimit(0.5f, 30.0f, newVal);
            if (auto* param = apvts.getParameter("input_f" + juce::String(i + 1)))
                param->setValueNotifyingHost(param->convertTo0to1(newVal));
            fValueLabels[i].setText(juce::String(newVal, 1), juce::dontSendNotification);
        };
    }
    
    // Wire up harmonic ADSR value editing
    const char* adsrSuffixes[] = { "AttackTime", "DecayTime", "SustainLevel", "ReleaseTime" };
    for (int i = 0; i < 4; ++i)
    {
        adsrValueEditors[i].onTextChange = [this, i, adsrSuffixes]() {
            float newVal = adsrValueEditors[i].getText().getFloatValue();
            juce::String paramID = "k" + juce::String(selectedDrawbar + 1) + adsrSuffixes[i];
            if (auto* param = apvts.getParameter(paramID))
            {
                param->setValueNotifyingHost(param->convertTo0to1(newVal));
                adsrValues[selectedDrawbar][i] = param->convertFrom0to1(param->getValue());
            }
            repaint();
        };
    }
    
    // Wire up amplitude ADSR value editing
    const char* ampParamNames[] = { "ampEnvAttackTime", "ampEnvDecayTime", "ampEnvSustainLevel", "ampEnvReleaseTime" };
    for (int i = 0; i < 4; ++i)
    {
        ampAdsrValueEditors[i].onTextChange = [this, i, ampParamNames]() {
            float newVal = ampAdsrValueEditors[i].getText().getFloatValue();
            if (auto* param = apvts.getParameter(ampParamNames[i]))
            {
                param->setValueNotifyingHost(param->convertTo0to1(newVal));
                ampAdsrValues[i] = param->convertFrom0to1(param->getValue());
            }
            repaint();
        };
    }
    
    // Register as listener for amplitude envelope parameters (for automation sync)
    apvts.addParameterListener("ampEnvAttackTime", this);
    apvts.addParameterListener("ampEnvDecayTime", this);
    apvts.addParameterListener("ampEnvSustainLevel", this);
    apvts.addParameterListener("ampEnvReleaseTime", this);

    setSize(1400, 800);
}

PLANETMainGui::~PLANETMainGui()
{
    // Remove parameter listeners to prevent dangling callbacks
    apvts.removeParameterListener("ampEnvAttackTime", this);
    apvts.removeParameterListener("ampEnvDecayTime", this);
    apvts.removeParameterListener("ampEnvSustainLevel", this);
    apvts.removeParameterListener("ampEnvReleaseTime", this);
}

void PLANETMainGui::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    int leftWidth = (int)(bounds.getWidth() * leftWidthRatio);
    int rightWidth = bounds.getWidth() - leftWidth;
    int mainHeight = bounds.getHeight() - patchBarHeight;
    
    // Calculate section heights for left side
    int harmonicAndAmpHeight = mainHeight - drawbarSectionHeight;
    int harmonicHeight = harmonicAndAmpHeight / 2;
    int ampHeight = harmonicAndAmpHeight - harmonicHeight;

    // ======================== LEFT SIDE ========================
    
    // Drawbar section (per-harmonic - light background)
    g.setColour(backgroundLight);
    g.fillRect(0, 0, leftWidth, drawbarSectionHeight);
    
    // Draw coloured backgrounds for each drawbar
    int drawbarMargin = 20;
    int drawbarWidth = (leftWidth - drawbarMargin * 2) / 10;
    for (int i = 0; i < 10; ++i)
    {
        int x = drawbarMargin + i * drawbarWidth;
        g.setColour(drawbarColours[i].withAlpha(0.3f));
        g.fillRoundedRectangle((float)x + 2, (float)drawbarMargin, 
                                (float)drawbarWidth - 4, (float)drawbarSectionHeight - drawbarMargin * 2, 5.0f);
    }
    
    // Harmonic section (per-harmonic - coloured based on selection)
    g.setColour(drawbarColours[selectedDrawbar].withAlpha(0.3f));
    g.fillRect(0, drawbarSectionHeight, leftWidth, harmonicHeight);

    // Draw ADSR envelope shape
    {
        int adsrZoneWidth = (int)(leftWidth * 0.65f);  // 2/3 of harmonic section
        int adsrMargin = 20;
        int adsrGraphHeight = harmonicHeight - 80;  // Leave room for labels below
        int adsrGraphY = drawbarSectionHeight + 10;
        int adsrGraphWidth = adsrZoneWidth - adsrMargin * 2;

        // Background for ADSR graph
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle((float)adsrMargin, (float)adsrGraphY, 
                                (float)adsrGraphWidth, (float)adsrGraphHeight, 5.0f);

        // Get current drawbar's ADSR values
        float attack = adsrValues[selectedDrawbar][0];
        float decay = adsrValues[selectedDrawbar][1];
        float sustain = adsrValues[selectedDrawbar][2];
        float release = adsrValues[selectedDrawbar][3];
        
        // Get curve amount from the Env Curve knob (0-1)
        float curveAmount = (float)envCurveKnob.getValue();
        float curveFactor = 1.0f + curveAmount * 6.0f;

        // Normalise times for display
        float totalTime = attack + decay + 0.3f + release;  // 0.3 for sustain hold
        if (totalTime < 0.1f) totalTime = 0.1f;
        float timeScale = (float)adsrGraphWidth / totalTime;

        // Calculate key points
        float x0 = (float)adsrMargin;
        float y0 = (float)(adsrGraphY + adsrGraphHeight);  // Bottom (zero level)
        float yTop = (float)(adsrGraphY + 10);              // Top (full level)
        float ySustain = yTop + (1.0f - sustain) * (y0 - yTop - 10);

        float x1 = x0 + attack * timeScale;                 // End of attack
        float x2 = x1 + decay * timeScale;                  // End of decay
        float x3 = x2 + 0.3f * timeScale;                   // End of sustain hold
        float x4 = x3 + release * timeScale;                // End of release

        // Draw envelope with curves
        juce::Path envPath;
        envPath.startNewSubPath(x0, y0);           // Start at zero
        
        const int numSegments = 20;
        
        // Attack phase (convex curve: 1 - exp(-curveFactor * t))
        if (curveAmount > 0.001f) {
            for (int i = 1; i <= numSegments; ++i) {
                float t = (float)i / numSegments;
                float curvedT = 1.0f - std::exp(-curveFactor * t);
                // Normalise curvedT since exp curve doesn't quite reach 1
                float maxCurve = 1.0f - std::exp(-curveFactor);
                curvedT /= maxCurve;
                float x = x0 + (x1 - x0) * t;
                float y = y0 + (yTop - y0) * curvedT;
                envPath.lineTo(x, y);
            }
        } else {
            envPath.lineTo(x1, yTop);  // Linear fallback
        }
        
        // Decay phase (concave curve: exp(-curveFactor * t))
        if (curveAmount > 0.001f) {
            for (int i = 1; i <= numSegments; ++i) {
                float t = (float)i / numSegments;
                float curvedProgress = std::exp(-curveFactor * t);
                // Normalise
                float minCurve = std::exp(-curveFactor);
                curvedProgress = (curvedProgress - minCurve) / (1.0f - minCurve);
                float x = x1 + (x2 - x1) * t;
                float y = yTop + (ySustain - yTop) * (1.0f - curvedProgress);
                envPath.lineTo(x, y);
            }
        } else {
            envPath.lineTo(x2, ySustain);  // Linear fallback
        }
        
        envPath.lineTo(x3, ySustain);              // Sustain hold (always linear)
        
        // Release phase (concave curve: exp(-curveFactor * t))
        if (curveAmount > 0.001f) {
            for (int i = 1; i <= numSegments; ++i) {
                float t = (float)i / numSegments;
                float curvedProgress = std::exp(-curveFactor * t);
                // Normalise
                float minCurve = std::exp(-curveFactor);
                curvedProgress = (curvedProgress - minCurve) / (1.0f - minCurve);
                float x = x3 + (x4 - x3) * t;
                float y = ySustain + (y0 - ySustain) * (1.0f - curvedProgress);
                envPath.lineTo(x, y);
            }
        } else {
            envPath.lineTo(x4, y0);  // Linear fallback
        }

        g.setColour(drawbarColours[selectedDrawbar]);
        g.strokePath(envPath, juce::PathStrokeType(2.5f));

        // Draw draggable handles
        float handleRadius = 6.0f;
        g.setColour(juce::Colours::white);
        g.fillEllipse(x1 - handleRadius, yTop - handleRadius, handleRadius * 2, handleRadius * 2);           // Attack peak
        g.fillEllipse(x2 - handleRadius, ySustain - handleRadius, handleRadius * 2, handleRadius * 2);       // Decay/Sustain
        g.fillEllipse(x4 - handleRadius, y0 - handleRadius, handleRadius * 2, handleRadius * 2);             // Release end
        
        // Handle outlines
        g.setColour(drawbarColours[selectedDrawbar]);
        g.drawEllipse(x1 - handleRadius, yTop - handleRadius, handleRadius * 2, handleRadius * 2, 2.0f);
        g.drawEllipse(x2 - handleRadius, ySustain - handleRadius, handleRadius * 2, handleRadius * 2, 2.0f);
        g.drawEllipse(x4 - handleRadius, y0 - handleRadius, handleRadius * 2, handleRadius * 2, 2.0f);
    }
    
    // Amplitude section (global - tinted background)
    g.setColour(backgroundGlobal);
    g.fillRect(0, drawbarSectionHeight + harmonicHeight, leftWidth, ampHeight);

    // Draw Amplitude ADSR envelope shape
    {
        int adsrZoneWidth = (int)(leftWidth * 0.65f);
        int adsrMargin = 20;
        int adsrGraphHeight = ampHeight - 80;
        int adsrGraphY = drawbarSectionHeight + harmonicHeight + 10;
        int adsrGraphWidth = adsrZoneWidth - adsrMargin * 2;

        // Background for ADSR graph
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle((float)adsrMargin, (float)adsrGraphY, 
                                (float)adsrGraphWidth, (float)adsrGraphHeight, 5.0f);

        // Get amplitude ADSR values
        float attack = ampAdsrValues[0];
        float decay = ampAdsrValues[1];
        float sustain = ampAdsrValues[2];
        float release = ampAdsrValues[3];
        
        // Get curve amount from the Env Curve knob (0-1)
        float curveAmount = (float)envCurveKnob.getValue();
        float curveFactor = 1.0f + curveAmount * 6.0f;

        // Normalise times for display
        float totalTime = attack + decay + 0.3f + release;
        if (totalTime < 0.1f) totalTime = 0.1f;
        float timeScale = (float)adsrGraphWidth / totalTime;

        // Calculate key points
        float x0 = (float)adsrMargin;
        float y0 = (float)(adsrGraphY + adsrGraphHeight);
        float yTop = (float)(adsrGraphY + 10);
        float ySustain = yTop + (1.0f - sustain) * (y0 - yTop - 10);

        float x1 = x0 + attack * timeScale;
        float x2 = x1 + decay * timeScale;
        float x3 = x2 + 0.3f * timeScale;
        float x4 = x3 + release * timeScale;

        // Draw envelope with curves
        juce::Path envPath;
        envPath.startNewSubPath(x0, y0);
        
        const int numSegments = 20;
        
        // Attack phase (convex curve)
        if (curveAmount > 0.001f) {
            for (int i = 1; i <= numSegments; ++i) {
                float t = (float)i / numSegments;
                float curvedT = 1.0f - std::exp(-curveFactor * t);
                float maxCurve = 1.0f - std::exp(-curveFactor);
                curvedT /= maxCurve;
                float x = x0 + (x1 - x0) * t;
                float y = y0 + (yTop - y0) * curvedT;
                envPath.lineTo(x, y);
            }
        } else {
            envPath.lineTo(x1, yTop);
        }
        
        // Decay phase (concave curve)
        if (curveAmount > 0.001f) {
            for (int i = 1; i <= numSegments; ++i) {
                float t = (float)i / numSegments;
                float curvedProgress = std::exp(-curveFactor * t);
                float minCurve = std::exp(-curveFactor);
                curvedProgress = (curvedProgress - minCurve) / (1.0f - minCurve);
                float x = x1 + (x2 - x1) * t;
                float y = yTop + (ySustain - yTop) * (1.0f - curvedProgress);
                envPath.lineTo(x, y);
            }
        } else {
            envPath.lineTo(x2, ySustain);
        }
        
        envPath.lineTo(x3, ySustain);  // Sustain hold
        
        // Release phase (concave curve)
        if (curveAmount > 0.001f) {
            for (int i = 1; i <= numSegments; ++i) {
                float t = (float)i / numSegments;
                float curvedProgress = std::exp(-curveFactor * t);
                float minCurve = std::exp(-curveFactor);
                curvedProgress = (curvedProgress - minCurve) / (1.0f - minCurve);
                float x = x3 + (x4 - x3) * t;
                float y = ySustain + (y0 - ySustain) * (1.0f - curvedProgress);
                envPath.lineTo(x, y);
            }
        } else {
            envPath.lineTo(x4, y0);
        }

        g.setColour(juce::Colours::white);
        g.strokePath(envPath, juce::PathStrokeType(2.5f));

        // Draw draggable handles
        float handleRadius = 6.0f;
        g.setColour(juce::Colours::white);
        g.fillEllipse(x1 - handleRadius, yTop - handleRadius, handleRadius * 2, handleRadius * 2);           // Attack peak
        g.fillEllipse(x2 - handleRadius, ySustain - handleRadius, handleRadius * 2, handleRadius * 2);       // Decay/Sustain
        g.fillEllipse(x4 - handleRadius, y0 - handleRadius, handleRadius * 2, handleRadius * 2);             // Release end
        
        // Handle outlines
        g.setColour(accentColour);
        g.drawEllipse(x1 - handleRadius, yTop - handleRadius, handleRadius * 2, handleRadius * 2, 2.0f);
        g.drawEllipse(x2 - handleRadius, ySustain - handleRadius, handleRadius * 2, handleRadius * 2, 2.0f);
        g.drawEllipse(x4 - handleRadius, y0 - handleRadius, handleRadius * 2, handleRadius * 2, 2.0f);
    }

    // ======================== RIGHT SIDE ========================
    
    // Entire right column is global (tinted background)
    g.setColour(backgroundGlobal);
    g.fillRect(leftWidth, 0, rightWidth, mainHeight);

    // Waveform display area (placeholder)
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle((float)leftWidth + 10, 10.0f, 
                           (float)rightWidth - 20, (float)drawbarSectionHeight - 20, 5.0f);
    g.setColour(accentColour.withAlpha(0.3f));
    g.drawHorizontalLine(drawbarSectionHeight / 2, (float)leftWidth + 15, (float)bounds.getWidth() - 15);
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawText("WAVEFORM", leftWidth + 10, 15, rightWidth - 20, 20, juce::Justification::left);

    // ======================== PATCH BAR ========================
    
    g.setColour(backgroundLight.darker(0.3f));
    g.fillRect(0, mainHeight, bounds.getWidth(), patchBarHeight);

    // ======================== SECTION LABELS (temporary) ========================
    
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(16.0f);
    
    // Left side labels
    g.drawText("DRAWBARS", 0, 0, leftWidth, drawbarSectionHeight, 
               juce::Justification::centred);
    g.drawText("HARMONIC (per-drawbar)", 0, drawbarSectionHeight, leftWidth, harmonicHeight, 
               juce::Justification::centred);
    g.drawText("AMPLITUDE (global)", 0, drawbarSectionHeight + harmonicHeight, leftWidth, ampHeight, 
               juce::Justification::centred);
    
    // Right side labels (adjusted for waveform at top)
    int rightX = leftWidth;
    int rightContentHeight = mainHeight - drawbarSectionHeight;  // Below waveform
    int rightSectionHeight = rightContentHeight / 4;  // 4 sections below waveform
    int rightSectionY = drawbarSectionHeight;  // Start below waveform
    
    g.drawText("VIBRATO", rightX, rightSectionY, rightWidth, rightSectionHeight, 
               juce::Justification::centred);
    g.drawText("PITCH", rightX, rightSectionY + rightSectionHeight, rightWidth, rightSectionHeight, 
               juce::Justification::centred);
    g.drawText("BRILLIANCE", rightX, rightSectionY + rightSectionHeight * 2, rightWidth, rightSectionHeight, 
               juce::Justification::centred);
    g.drawText("EFFECTS", rightX, rightSectionY + rightSectionHeight * 3, rightWidth, rightSectionHeight, 
               juce::Justification::centred);
    
    // Patch bar label
    g.drawText("PATCH / PRESET", 0, mainHeight, bounds.getWidth(), patchBarHeight, 
               juce::Justification::centred);

    // ======================== DIVIDING LINES (temporary guides) ========================
    
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    
    // Vertical divider
    g.drawVerticalLine(leftWidth, 0, (float)mainHeight);
    
    // Horizontal dividers on left
    g.drawHorizontalLine(drawbarSectionHeight, 0, (float)leftWidth);
    g.drawHorizontalLine(drawbarSectionHeight + harmonicHeight, 0, (float)leftWidth);
    
    // Horizontal dividers on right (below waveform)
    g.drawHorizontalLine(drawbarSectionHeight, (float)leftWidth, (float)bounds.getWidth());  // Below waveform
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine(drawbarSectionHeight + rightSectionHeight * i, (float)leftWidth, (float)bounds.getWidth());
    
    // Patch bar divider
    g.drawHorizontalLine(mainHeight, 0, (float)bounds.getWidth());
}

void PLANETMainGui::resized()
{
    auto bounds = getLocalBounds();
    int leftWidth = (int)(bounds.getWidth() * leftWidthRatio);
    
    // Drawbar section layout
    int drawbarMargin = 20;
    int fLabelHeight = 25;
    int drawbarWidth = (leftWidth - drawbarMargin * 2) / 10;
    int drawbarHeight = drawbarSectionHeight - fLabelHeight - drawbarMargin * 2;
    
    for (int i = 0; i < 10; ++i)
    {
        int x = drawbarMargin + i * drawbarWidth;
        
        // F value label at top
        fValueLabels[i].setBounds(x + 5, drawbarMargin, drawbarWidth - 10, fLabelHeight);
        
        // Drawbar slider below
        drawbarSliders[i].setBounds(x + 10, drawbarMargin + fLabelHeight + 5, drawbarWidth - 20, drawbarHeight);
    }

    // Position ADSR labels and value editors
    int mainHeight = bounds.getHeight() - patchBarHeight;
    int harmonicAndAmpHeight = mainHeight - drawbarSectionHeight;
    int harmonicHeight = harmonicAndAmpHeight / 2;
    int adsrZoneWidth = (int)(leftWidth * 0.65f);
    int adsrGraphHeight = harmonicHeight - 80;
    int adsrGraphY = drawbarSectionHeight + 10;
    int adsrGraphWidth = adsrZoneWidth - 40;
    int adsrLabelY = drawbarSectionHeight + adsrGraphHeight + 20;
    int adsrFieldWidth = 50;
    int adsrFieldHeight = 25;
    int adsrSpacing = (adsrZoneWidth - 40) / 4;

    // Store harmonic envelope bounds for mouse interaction
    harmonicEnvBounds = juce::Rectangle<int>(20, adsrGraphY, adsrGraphWidth, adsrGraphHeight);

    for (int i = 0; i < 4; ++i)
    {
        int xPos = 20 + i * adsrSpacing + (adsrSpacing - adsrFieldWidth) / 2;
        adsrLabels[i].setBounds(xPos, adsrLabelY, adsrFieldWidth, 20);
        adsrValueEditors[i].setBounds(xPos, adsrLabelY + 22, adsrFieldWidth, adsrFieldHeight);
    }

    // Position LFO controls in right portion of Harmonic section
    int envDepthSliderWidth = 50;
    int envDepthX = adsrZoneWidth + 10;
    int lfoZoneX = envDepthX + envDepthSliderWidth + 20;
    int lfoZoneWidth = leftWidth - lfoZoneX;
    int lfoZoneY = drawbarSectionHeight + 10;
    int knobSize = 80;

    // Envelope depth slider - same height as ADSR graphic, with label and value below
    int envDepthY = adsrGraphY;
    int envDepthSliderHeight = adsrGraphHeight;
    envDepthKnob.setBounds(envDepthX, envDepthY, envDepthSliderWidth, envDepthSliderHeight);
    envDepthLabel.setBounds(envDepthX - 10, envDepthY + envDepthSliderHeight + 5, envDepthSliderWidth + 20, 18);
    envDepthValue.setBounds(envDepthX, envDepthY + envDepthSliderHeight + 25, envDepthSliderWidth, 22);

    // F display at top - centered with rounded background
    int fDisplayWidth = 70;
    selectedFDisplay.setBounds(lfoZoneX + (lfoZoneWidth - fDisplayWidth) / 2, lfoZoneY, fDisplayWidth, 35);

    // LFO Shape label and dropdown on same line
    int comboY = lfoZoneY + 45;
    int comboWidth = (lfoZoneWidth - 30) / 2;
    lfoShapeLabel.setBounds(lfoZoneX + 10, comboY, comboWidth - 5, 25);
    lfoShapeCombo.setBounds(lfoZoneX + 10 + comboWidth, comboY, comboWidth, 25);

    // Two knobs side by side below dropdown - larger and lower
    int knobY = comboY + 45;
    int knobSpacing = (lfoZoneWidth - knobSize * 2) / 3;
    int knob1X = lfoZoneX + knobSpacing;
    int knob2X = knob1X + knobSize + knobSpacing;

    lfoSpeedLabel.setBounds(knob1X, knobY, knobSize, 18);
    lfoSpeedKnob.setBounds(knob1X, knobY + 18, knobSize, knobSize);

    lfoDepthLabel.setBounds(knob2X, knobY, knobSize, 18);
    lfoDepthKnob.setBounds(knob2X, knobY + 18, knobSize, knobSize);

    // ======================== AMPLITUDE ADSR SECTION ========================
    int ampHeight = harmonicAndAmpHeight - harmonicHeight;
    int ampAdsrGraphHeight = ampHeight - 80;
    int ampAdsrGraphY = drawbarSectionHeight + harmonicHeight + 10;
    int ampAdsrLabelY = ampAdsrGraphY + ampAdsrGraphHeight + 5;

    // Store amplitude envelope bounds for mouse interaction
    ampEnvBounds = juce::Rectangle<int>(20, ampAdsrGraphY, adsrGraphWidth, ampAdsrGraphHeight);

    for (int i = 0; i < 4; ++i)
    {
        int xPos = 20 + i * adsrSpacing + (adsrSpacing - adsrFieldWidth) / 2;
        ampAdsrLabels[i].setBounds(xPos, ampAdsrLabelY, adsrFieldWidth, 20);
        ampAdsrValueEditors[i].setBounds(xPos, ampAdsrLabelY + 22, adsrFieldWidth, adsrFieldHeight);
    }

    // Velocity to Amplitude slider - same position as Env Depth but in Amplitude section
    velAmpSlider.setBounds(envDepthX, ampAdsrGraphY, envDepthSliderWidth, ampAdsrGraphHeight);
    velAmpLabel.setBounds(envDepthX - 10, ampAdsrGraphY + ampAdsrGraphHeight + 5, envDepthSliderWidth + 20, 18);
    velAmpValue.setBounds(envDepthX, ampAdsrGraphY + ampAdsrGraphHeight + 25, envDepthSliderWidth, 22);

    // 2x2 knob grid in Amplitude zone - aligned with LFO knobs above
    int ampKnobSize = 60;
    int ampKnobValueHeight = 20;
    int ampKnobStartY = drawbarSectionHeight + harmonicHeight + 10;
    int ampKnobRowHeight = 18 + ampKnobSize + ampKnobValueHeight + 10;  // label + knob + value + gap

    // Row 1: Vel Brill (under LFO Speed), Vel Attack (under LFO Depth)
    velBrillLabel.setBounds(knob1X + (knobSize - ampKnobSize) / 2, ampKnobStartY, ampKnobSize, 18);
    velBrillKnob.setBounds(knob1X + (knobSize - ampKnobSize) / 2, ampKnobStartY + 18, ampKnobSize, ampKnobSize);
    velBrillValue.setBounds(knob1X + (knobSize - ampKnobSize) / 2, ampKnobStartY + 18 + ampKnobSize, ampKnobSize, ampKnobValueHeight);

    velAttackLabel.setBounds(knob2X + (knobSize - ampKnobSize) / 2, ampKnobStartY, ampKnobSize, 18);
    velAttackKnob.setBounds(knob2X + (knobSize - ampKnobSize) / 2, ampKnobStartY + 18, ampKnobSize, ampKnobSize);
    velAttackValue.setBounds(knob2X + (knobSize - ampKnobSize) / 2, ampKnobStartY + 18 + ampKnobSize, ampKnobSize, ampKnobValueHeight);

    // Row 2: Env Curve (under Vel Brill), Vintage (under Vel Attack)
    int row2Y = ampKnobStartY + ampKnobRowHeight;
    envCurveLabel.setBounds(knob1X + (knobSize - ampKnobSize) / 2, row2Y, ampKnobSize, 18);
    envCurveKnob.setBounds(knob1X + (knobSize - ampKnobSize) / 2, row2Y + 18, ampKnobSize, ampKnobSize);
    envCurveValue.setBounds(knob1X + (knobSize - ampKnobSize) / 2, row2Y + 18 + ampKnobSize, ampKnobSize, ampKnobValueHeight);

    vintageLabel.setBounds(knob2X + (knobSize - ampKnobSize) / 2, row2Y, ampKnobSize, 18);
    vintageKnob.setBounds(knob2X + (knobSize - ampKnobSize) / 2, row2Y + 18, ampKnobSize, ampKnobSize);
    vintageValue.setBounds(knob2X + (knobSize - ampKnobSize) / 2, row2Y + 18 + ampKnobSize, ampKnobSize, ampKnobValueHeight);

    // ======================== RIGHT COLUMN LAYOUT ========================
    int rightX = leftWidth + 10;
    int rightContentWidth = bounds.getWidth() - leftWidth - 20;
    int rightKnobSize = 80;  // Match LFO knob size
    
    // Calculate section heights for right column (below waveform)
    int waveformHeight = drawbarSectionHeight;
    int remainingHeight = mainHeight - waveformHeight;
    int rightSectionHeight = remainingHeight / 4;  // 4 sections: Vibrato, Pitch, Brilliance, Effects
    
    // Vibrato section (3 knobs)
    int vibratoY = waveformHeight + 5;
    int vibratoKnobSpacing = rightContentWidth / 3;
    
    int vkx0 = rightX + (vibratoKnobSpacing - rightKnobSize) / 2;
    vibratoRateLabel.setBounds(vkx0, vibratoY, rightKnobSize, 16);
    vibratoRateKnob.setBounds(vkx0, vibratoY + 16, rightKnobSize, rightKnobSize);
    
    int vkx1 = rightX + vibratoKnobSpacing + (vibratoKnobSpacing - rightKnobSize) / 2;
    vibratoDepthLabel.setBounds(vkx1, vibratoY, rightKnobSize, 16);
    vibratoDepthKnob.setBounds(vkx1, vibratoY + 16, rightKnobSize, rightKnobSize);
    
    int vkx2 = rightX + vibratoKnobSpacing * 2 + (vibratoKnobSpacing - rightKnobSize) / 2;
    vibratoFadeLabel.setBounds(vkx2, vibratoY, rightKnobSize, 16);
    vibratoFadeKnob.setBounds(vkx2, vibratoY + 16, rightKnobSize, rightKnobSize);
    
    // Pitch section (2 knobs)
    int pitchY = waveformHeight + rightSectionHeight + 5;
    int pitchKnobSpacing = rightContentWidth / 2;
    
    int pkx0 = rightX + (pitchKnobSpacing - rightKnobSize) / 2;
    pitchDistLabel.setBounds(pkx0, pitchY, rightKnobSize, 16);
    pitchDistKnob.setBounds(pkx0, pitchY + 16, rightKnobSize, rightKnobSize);
    
    int pkx1 = rightX + pitchKnobSpacing + (pitchKnobSpacing - rightKnobSize) / 2;
    pitchTimeLabel.setBounds(pkx1, pitchY, rightKnobSize, 16);
    pitchTimeKnob.setBounds(pkx1, pitchY + 16, rightKnobSize, rightKnobSize);
    
    // Brilliance section (horizontal slider filling width)
    int brillianceY = waveformHeight + rightSectionHeight * 2 + 10;
    int sliderMargin = 20;
    brillianceMainLabel.setBounds(rightX, brillianceY, rightContentWidth, 18);
    brillianceSlider.setBounds(rightX + sliderMargin, brillianceY + 22, rightContentWidth - sliderMargin * 2, 40);
    
    // Effects section (4 vertical sliders evenly spaced)
    int effectsY = waveformHeight + rightSectionHeight * 3 + 5;
    int effectsSliderWidth = 40;
    int effectsSliderHeight = rightSectionHeight - 40;
    int effectsSliderSpacing = rightContentWidth / 4;
    
    int esx0 = rightX + (effectsSliderSpacing - effectsSliderWidth) / 2;
    detuneAmountLabel.setBounds(esx0, effectsY, effectsSliderWidth, 14);
    detuneAmountSlider.setBounds(esx0, effectsY + 16, effectsSliderWidth, effectsSliderHeight);
    
    int esx1 = rightX + effectsSliderSpacing + (effectsSliderSpacing - effectsSliderWidth) / 2;
    detuneMixLabel.setBounds(esx1, effectsY, effectsSliderWidth, 14);
    detuneMixSlider.setBounds(esx1, effectsY + 16, effectsSliderWidth, effectsSliderHeight);
    
    int esx2 = rightX + effectsSliderSpacing * 2 + (effectsSliderSpacing - effectsSliderWidth) / 2;
    reverbTimeLabel.setBounds(esx2, effectsY, effectsSliderWidth, 14);
    reverbTimeSlider.setBounds(esx2, effectsY + 16, effectsSliderWidth, effectsSliderHeight);
    
    int esx3 = rightX + effectsSliderSpacing * 3 + (effectsSliderSpacing - effectsSliderWidth) / 2;
    reverbMixLabel.setBounds(esx3, effectsY, effectsSliderWidth, 14);
    reverbMixSlider.setBounds(esx3, effectsY + 16, effectsSliderWidth, effectsSliderHeight);
}

void PLANETMainGui::mouseDown(const juce::MouseEvent& event)
{
    // Check if click came from a drawbar slider
    for (int i = 0; i < 10; ++i)
    {
        if (event.eventComponent == &drawbarSliders[i])
        {
            if (i != selectedDrawbar)
            {
                selectedDrawbar = i;
                updateAdsrDisplay();
                repaint();
            }
            return;
        }
    }
    
    // Check for envelope handle clicks
    float handleRadius = 10.0f;  // Slightly larger hit area than visual
    
    // Check harmonic envelope handles
    if (harmonicEnvBounds.contains(event.x, event.y) || 
        (event.x >= harmonicEnvBounds.getX() - handleRadius && 
         event.x <= harmonicEnvBounds.getRight() + handleRadius &&
         event.y >= harmonicEnvBounds.getY() - handleRadius && 
         event.y <= harmonicEnvBounds.getBottom() + handleRadius))
    {
        auto attackPt = getEnvelopePoint(1, harmonicEnvBounds, 
            adsrValues[selectedDrawbar][0], adsrValues[selectedDrawbar][1],
            adsrValues[selectedDrawbar][2], adsrValues[selectedDrawbar][3]);
        auto decaySustainPt = getEnvelopePoint(2, harmonicEnvBounds,
            adsrValues[selectedDrawbar][0], adsrValues[selectedDrawbar][1],
            adsrValues[selectedDrawbar][2], adsrValues[selectedDrawbar][3]);
        auto releasePt = getEnvelopePoint(4, harmonicEnvBounds,
            adsrValues[selectedDrawbar][0], adsrValues[selectedDrawbar][1],
            adsrValues[selectedDrawbar][2], adsrValues[selectedDrawbar][3]);
        
        if (attackPt.getDistanceFrom(event.position) < handleRadius)
        {
            currentDragTarget = DragTarget::HarmonicAttack;
            return;
        }
        if (decaySustainPt.getDistanceFrom(event.position) < handleRadius)
        {
            currentDragTarget = DragTarget::HarmonicDecaySustain;
            return;
        }
        if (releasePt.getDistanceFrom(event.position) < handleRadius)
        {
            currentDragTarget = DragTarget::HarmonicRelease;
            return;
        }
    }
    
    // Check amplitude envelope handles
    if (ampEnvBounds.contains(event.x, event.y) ||
        (event.x >= ampEnvBounds.getX() - handleRadius && 
         event.x <= ampEnvBounds.getRight() + handleRadius &&
         event.y >= ampEnvBounds.getY() - handleRadius && 
         event.y <= ampEnvBounds.getBottom() + handleRadius))
    {
        auto attackPt = getEnvelopePoint(1, ampEnvBounds,
            ampAdsrValues[0], ampAdsrValues[1], ampAdsrValues[2], ampAdsrValues[3]);
        auto decaySustainPt = getEnvelopePoint(2, ampEnvBounds,
            ampAdsrValues[0], ampAdsrValues[1], ampAdsrValues[2], ampAdsrValues[3]);
        auto releasePt = getEnvelopePoint(4, ampEnvBounds,
            ampAdsrValues[0], ampAdsrValues[1], ampAdsrValues[2], ampAdsrValues[3]);
        
        if (attackPt.getDistanceFrom(event.position) < handleRadius)
        {
            currentDragTarget = DragTarget::AmpAttack;
            return;
        }
        if (decaySustainPt.getDistanceFrom(event.position) < handleRadius)
        {
            currentDragTarget = DragTarget::AmpDecaySustain;
            return;
        }
        if (releasePt.getDistanceFrom(event.position) < handleRadius)
        {
            currentDragTarget = DragTarget::AmpRelease;
            return;
        }
    }
    
    // Otherwise check if click is in the drawbar background area
    auto bounds = getLocalBounds();
    int leftWidth = (int)(bounds.getWidth() * leftWidthRatio);
    int drawbarMargin = 20;
    int drawbarWidth = (leftWidth - drawbarMargin * 2) / 10;
    
    if (event.y < drawbarSectionHeight && event.x < leftWidth)
    {
        int clickedDrawbar = (event.x - drawbarMargin) / drawbarWidth;
        clickedDrawbar = juce::jlimit(0, 9, clickedDrawbar);
        
        if (clickedDrawbar != selectedDrawbar)
        {
            selectedDrawbar = clickedDrawbar;
            updateAdsrDisplay();
            repaint();
        }
    }
}

void PLANETMainGui::mouseDrag(const juce::MouseEvent& event)
{
    if (currentDragTarget == DragTarget::None)
        return;
    
    updateAdsrFromDrag(event);
    repaint();
}

void PLANETMainGui::mouseUp(const juce::MouseEvent& event)
{
    currentDragTarget = DragTarget::None;
}

juce::Point<float> PLANETMainGui::getEnvelopePoint(int pointIndex, const juce::Rectangle<int>& bounds,
                                                    float attack, float decay, float sustain, float release)
{
    float totalTime = attack + decay + 0.3f + release;
    if (totalTime < 0.1f) totalTime = 0.1f;
    float timeScale = (float)bounds.getWidth() / totalTime;
    
    float x0 = (float)bounds.getX();
    float y0 = (float)bounds.getBottom();  // Bottom (zero level)
    float yTop = (float)bounds.getY() + 10;  // Top (full level)
    float ySustain = yTop + (1.0f - sustain) * (y0 - yTop - 10);
    
    float x1 = x0 + attack * timeScale;      // Attack peak
    float x2 = x1 + decay * timeScale;       // Decay/Sustain point
    float x3 = x2 + 0.3f * timeScale;        // End of sustain hold
    float x4 = x3 + release * timeScale;     // Release end
    
    switch (pointIndex)
    {
        case 0: return { x0, y0 };           // Start
        case 1: return { x1, yTop };         // Attack peak
        case 2: return { x2, ySustain };     // Decay/Sustain
        case 3: return { x3, ySustain };     // End of sustain hold
        case 4: return { x4, y0 };           // Release end
        default: return { x0, y0 };
    }
}

void PLANETMainGui::updateAdsrFromDrag(const juce::MouseEvent& event)
{
    bool isHarmonic = (currentDragTarget == DragTarget::HarmonicAttack ||
                       currentDragTarget == DragTarget::HarmonicDecaySustain ||
                       currentDragTarget == DragTarget::HarmonicRelease);
    
    auto& bounds = isHarmonic ? harmonicEnvBounds : ampEnvBounds;
    float* values = isHarmonic ? adsrValues[selectedDrawbar] : ampAdsrValues;
    
    float attack = values[0];
    float decay = values[1];
    float sustain = values[2];
    float release = values[3];
    
    float totalTime = attack + decay + 0.3f + release;
    if (totalTime < 0.1f) totalTime = 0.1f;
    float timeScale = (float)bounds.getWidth() / totalTime;
    
    float x0 = (float)bounds.getX();
    float y0 = (float)bounds.getBottom();
    float yTop = (float)bounds.getY() + 10;
    float yRange = y0 - yTop - 10;
    
    // Calculate x position of attack peak for reference
    float x1 = x0 + attack * timeScale;
    
    switch (currentDragTarget)
    {
        case DragTarget::HarmonicAttack:
        case DragTarget::AmpAttack:
        {
            // Horizontal drag changes attack time
            float newX = juce::jlimit(x0, (float)bounds.getRight(), (float)event.x);
            float newAttack = (newX - x0) / timeScale;
            values[0] = juce::jlimit(0.001f, 10.0f, newAttack);
            break;
        }
        
        case DragTarget::HarmonicDecaySustain:
        case DragTarget::AmpDecaySustain:
        {
            // Horizontal drag changes decay time (relative to attack peak)
            float currentX1 = x0 + values[0] * timeScale;
            float newX = juce::jlimit(currentX1, (float)bounds.getRight(), (float)event.x);
            float newDecay = (newX - currentX1) / timeScale;
            values[1] = juce::jlimit(0.001f, 10.0f, newDecay);
            
            // Vertical drag changes sustain level
            float newY = juce::jlimit(yTop, y0 - 10, (float)event.y);
            float newSustain = 1.0f - (newY - yTop) / yRange;
            values[2] = juce::jlimit(0.0f, 1.0f, newSustain);
            break;
        }
        
        case DragTarget::HarmonicRelease:
        case DragTarget::AmpRelease:
        {
            // Horizontal drag changes release time (relative to sustain end)
            float currentX3 = x0 + (values[0] + values[1] + 0.3f) * timeScale;
            float newX = juce::jlimit(currentX3, (float)bounds.getRight() + 50, (float)event.x);
            float newRelease = (newX - currentX3) / timeScale;
            values[3] = juce::jlimit(0.001f, 10.0f, newRelease);
            break;
        }
        
        default:
            break;
    }
    
    // Update text displays and write to APVTS
    if (isHarmonic)
    {
        const char* suffixes[] = { "AttackTime", "DecayTime", "SustainLevel", "ReleaseTime" };
        juce::String prefix = "k" + juce::String(selectedDrawbar + 1);
        for (int i = 0; i < 4; ++i)
        {
            adsrValueEditors[i].setText(juce::String(values[i], 2), juce::dontSendNotification);
            if (auto* param = apvts.getParameter(prefix + suffixes[i]))
                param->setValueNotifyingHost(param->convertTo0to1(values[i]));
        }
    }
    else
    {
        const char* ampParams[] = { "ampEnvAttackTime", "ampEnvDecayTime", "ampEnvSustainLevel", "ampEnvReleaseTime" };
        for (int i = 0; i < 4; ++i)
        {
            ampAdsrValueEditors[i].setText(juce::String(values[i], 2), juce::dontSendNotification);
            if (auto* param = apvts.getParameter(ampParams[i]))
                param->setValueNotifyingHost(param->convertTo0to1(values[i]));
        }
    }
}

void PLANETMainGui::updateAdsrDisplay()
{
    for (int i = 0; i < 4; ++i)
    {
        adsrValueEditors[i].setText(juce::String(adsrValues[selectedDrawbar][i], 2), 
                                     juce::dontSendNotification);
    }
    
    // Update F display with selected drawbar's F value
    selectedFDisplay.setText("F" + fValueLabels[selectedDrawbar].getText(), 
                              juce::dontSendNotification);
    selectedFDisplay.setColour(juce::Label::textColourId, drawbarColours[selectedDrawbar]);
    
    // Rebind context-sensitive controls to new drawbar
    bindToSelectedDrawbar();
}

void PLANETMainGui::bindToSelectedDrawbar()
{
    // Build parameter ID prefix for current drawbar (k1, k2, ... k10)
    juce::String prefix = "k" + juce::String(selectedDrawbar + 1);
    
    // Store current parameter IDs for reference
    currentHarmonicParamIDs[0] = prefix + "AttackTime";
    currentHarmonicParamIDs[1] = prefix + "DecayTime";
    currentHarmonicParamIDs[2] = prefix + "SustainLevel";
    currentHarmonicParamIDs[3] = prefix + "ReleaseTime";
    
    // Destroy old attachments first (must be done before creating new ones)
    lfoSpeedAttachment.reset();
    lfoDepthAttachment.reset();
    lfoShapeAttachment.reset();
    envDepthAttachment.reset();
    
    // Create new attachments for the selected drawbar's LFO parameters
    lfoSpeedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "LFORate", lfoSpeedKnob);
    lfoDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "LFOAmount", lfoDepthKnob);
    lfoShapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, prefix + "LFOShape", lfoShapeCombo);
    envDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "EnvelopeAmount", envDepthKnob);
    
    // Update ADSR display values from parameters
    if (auto* param = apvts.getParameter(currentHarmonicParamIDs[0]))
        adsrValues[selectedDrawbar][0] = param->convertFrom0to1(param->getValue());
    if (auto* param = apvts.getParameter(currentHarmonicParamIDs[1]))
        adsrValues[selectedDrawbar][1] = param->convertFrom0to1(param->getValue());
    if (auto* param = apvts.getParameter(currentHarmonicParamIDs[2]))
        adsrValues[selectedDrawbar][2] = param->convertFrom0to1(param->getValue());
    if (auto* param = apvts.getParameter(currentHarmonicParamIDs[3]))
        adsrValues[selectedDrawbar][3] = param->convertFrom0to1(param->getValue());
    
    // Update ADSR text displays
    for (int i = 0; i < 4; ++i)
        adsrValueEditors[i].setText(juce::String(adsrValues[selectedDrawbar][i], 2), juce::dontSendNotification);
    
    // Update envelope depth display
    if (auto* param = apvts.getParameter(prefix + "EnvelopeAmount"))
        envDepthValue.setText(juce::String(param->convertFrom0to1(param->getValue()), 2), juce::dontSendNotification);
}

void PLANETMainGui::parameterChanged(const juce::String& parameterID, float newValue)
{
    // This callback fires when parameters change from automation or other sources
    // We use it to keep the GUI in sync with parameter values
    
    // Check if it's one of the amplitude envelope parameters
    if (parameterID == "ampEnvAttackTime")
    {
        ampAdsrValues[0] = newValue;
        ampAdsrValueEditors[0].setText(juce::String(newValue, 2), juce::dontSendNotification);
        repaint();
    }
    else if (parameterID == "ampEnvDecayTime")
    {
        ampAdsrValues[1] = newValue;
        ampAdsrValueEditors[1].setText(juce::String(newValue, 2), juce::dontSendNotification);
        repaint();
    }
    else if (parameterID == "ampEnvSustainLevel")
    {
        ampAdsrValues[2] = newValue;
        ampAdsrValueEditors[2].setText(juce::String(newValue, 2), juce::dontSendNotification);
        repaint();
    }
    else if (parameterID == "ampEnvReleaseTime")
    {
        ampAdsrValues[3] = newValue;
        ampAdsrValueEditors[3].setText(juce::String(newValue, 2), juce::dontSendNotification);
        repaint();
    }
    // Check if it's one of the currently selected harmonic's ADSR parameters
    else if (parameterID == currentHarmonicParamIDs[0])
    {
        adsrValues[selectedDrawbar][0] = newValue;
        adsrValueEditors[0].setText(juce::String(newValue, 2), juce::dontSendNotification);
        repaint();
    }
    else if (parameterID == currentHarmonicParamIDs[1])
    {
        adsrValues[selectedDrawbar][1] = newValue;
        adsrValueEditors[1].setText(juce::String(newValue, 2), juce::dontSendNotification);
        repaint();
    }
    else if (parameterID == currentHarmonicParamIDs[2])
    {
        adsrValues[selectedDrawbar][2] = newValue;
        adsrValueEditors[2].setText(juce::String(newValue, 2), juce::dontSendNotification);
        repaint();
    }
    else if (parameterID == currentHarmonicParamIDs[3])
    {
        adsrValues[selectedDrawbar][3] = newValue;
        adsrValueEditors[3].setText(juce::String(newValue, 2), juce::dontSendNotification);
        repaint();
    }
}

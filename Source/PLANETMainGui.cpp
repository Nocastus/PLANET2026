/*
  ==============================================================================
    PLANETMainGui.cpp - User-Friendly GUI for PLANET Synthesizer
    With parameter binding to audio engine
    REFACTORED: Extracted envelope drawing into reusable helper function
  ==============================================================================
*/

#include "PLANETMainGui.h"
#include "PluginProcessor.h"

PLANETMainGui::PLANETMainGui(juce::AudioProcessorValueTreeState& apvtsRef,
    juce::AudioProcessor* processor,
    std::atomic<float>* rawModWheelPtr,
    std::atomic<bool>* modWheelEngagedPtr,
    std::array<float, 2048>* waveformSnapshotPtr,
    std::atomic<int>* snapshotLengthPtr,
    std::atomic<bool>* snapshotReadyPtr,
    std::atomic<bool>* snapshotRequestPtr,
    std::atomic<bool>* waveformActivePtr)
    : apvts(apvtsRef), audioProcessor(processor), rawModWheelValue(rawModWheelPtr), modWheelEngaged(modWheelEngagedPtr)
{
    // Load custom fonts from embedded binary data
    auto regularTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::AmarnaRegular_ttf, BinaryData::AmarnaRegular_ttfSize);
    auto semiBoldTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::AmarnaSemiBold_ttf, BinaryData::AmarnaSemiBold_ttfSize);

    amarnaRegular = juce::Font(regularTypeface);
    amarnaSemiBold = juce::Font(semiBoldTypeface);

    // Helper to apply consistent font styling to labels
    auto styleLabel = [this](juce::Label& label, bool isValue = false) {
        if (isValue)
            label.setFont(amarnaRegular.withHeight(18.0f));
        else
            label.setFont(amarnaSemiBold.withHeight(18.0f));
        };

    // Connect waveform display to data source
    waveformDisplay.setDataSource(waveformSnapshotPtr, snapshotLengthPtr,
        snapshotReadyPtr, snapshotRequestPtr,
        waveformActivePtr);

    // Set up drawbar sliders
    for (int i = 0; i < 10; ++i)
    {
        drawbarSliders[i].setSliderStyle(juce::Slider::LinearVertical);
        drawbarSliders[i].setRange(-2.0, 2.0, 0.01);
        drawbarSliders[i].setValue(0.0);
        drawbarSliders[i].setDoubleClickReturnValue(true, 0.0);
        drawbarSliders[i].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        drawbarSliders[i].setSliderSnapsToMousePosition(false);
        drawbarSliders[i].addMouseListener(this, false);
        drawbarSliders[i].setLookAndFeel(&drawbarLookAndFeel);  // Apply custom LookAndFeel for LFO feedback
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
        adsrValues[i][2] = 0.5f;   // Sustain
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
    lfoShapeCombo.addItem("Random", 4);
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
    lfoSpeedKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 0, 0);
    addAndMakeVisible(lfoSpeedKnob);
    lfoSpeedKnob.setLookAndFeel(&ishtarLookAndFeel);

    lfoSpeedLabel.setText("Speed", juce::dontSendNotification);
    lfoSpeedLabel.setJustificationType(juce::Justification::centred);
    lfoSpeedLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(lfoSpeedLabel);

    // Set up LFO depth knob
    lfoDepthKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lfoDepthKnob.setRange(0.0, 5.0, 0.01);
    lfoDepthKnob.setValue(0.0);
    lfoDepthKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 0, 0);
    addAndMakeVisible(lfoDepthKnob);
    lfoDepthKnob.setLookAndFeel(&ishtarLookAndFeel);

    lfoDepthLabel.setText("Depth", juce::dontSendNotification);
    lfoDepthLabel.setJustificationType(juce::Justification::centred);
    lfoDepthLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(lfoDepthLabel);

    // Set up LFO value labels
    lfoSpeedValue.setText("4.00", juce::dontSendNotification);
    lfoSpeedValue.setJustificationType(juce::Justification::centred);
    lfoSpeedValue.setColour(juce::Label::textColourId, juce::Colours::white);
    lfoSpeedValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    lfoSpeedValue.setEditable(true);
    addAndMakeVisible(lfoSpeedValue);

    lfoDepthValue.setText("0.00", juce::dontSendNotification);
    lfoDepthValue.setJustificationType(juce::Justification::centred);
    lfoDepthValue.setColour(juce::Label::textColourId, juce::Colours::white);
    lfoDepthValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    lfoDepthValue.setEditable(true);
    addAndMakeVisible(lfoDepthValue);

    // Set up selected F display
    selectedFDisplay.setText("F1", juce::dontSendNotification);
    selectedFDisplay.setFont(juce::Font(24.0f, juce::Font::bold));
    selectedFDisplay.setJustificationType(juce::Justification::centred);
    selectedFDisplay.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    selectedFDisplay.setColour(juce::Label::textColourId, drawbarColours[0]);
    addAndMakeVisible(selectedFDisplay);

    // Set up envelope depth slider
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

    // Set up Velocity to Drawbar knob
    velToDrawbarKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    velToDrawbarKnob.setRange(-100.0, 100.0, 1.0);
    velToDrawbarKnob.setValue(0.0);
    velToDrawbarKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    velToDrawbarKnob.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(velToDrawbarKnob);
    velToDrawbarKnob.setLookAndFeel(&ishtarLookAndFeel);

    velToDrawbarLabel.setText("Vel to Drawbar", juce::dontSendNotification);
    velToDrawbarLabel.setJustificationType(juce::Justification::centred);
    velToDrawbarLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(velToDrawbarLabel);

    velToDrawbarValue.setText("0", juce::dontSendNotification);
    velToDrawbarValue.setJustificationType(juce::Justification::centred);
    velToDrawbarValue.setColour(juce::Label::textColourId, juce::Colours::white);
    velToDrawbarValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    velToDrawbarValue.setEditable(true);
    addAndMakeVisible(velToDrawbarValue);

    velToDrawbarKnob.onValueChange = [this]() {
        velToDrawbarValue.setText(juce::String((int)velToDrawbarKnob.getValue()), juce::dontSendNotification);
        };

    envDepthValue.setText("0.00", juce::dontSendNotification);
    envDepthValue.setJustificationType(juce::Justification::centred);
    envDepthValue.setColour(juce::Label::textColourId, juce::Colours::white);
    envDepthValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    envDepthValue.setEditable(true);
    addAndMakeVisible(envDepthValue);

    // Set up Amplitude ADSR labels and value editors
    for (int i = 0; i < 4; ++i)
    {
        ampAdsrLabels[i].setText(adsrNames[i], juce::dontSendNotification);
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

    // Set up Velocity to Amplitude slider
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



    // Set up Vel to Attack knob
    velAttackKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    velAttackKnob.setRange(0.0, 100.0, 1.0);
    velAttackKnob.setValue(0.0);
    velAttackKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    velAttackKnob.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(velAttackKnob);
    velAttackKnob.setLookAndFeel(&ishtarLookAndFeel);
    velAttackLabel.setText("Vel to Attk", juce::dontSendNotification);
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
    envCurveKnob.onValueChange = [this]() { repaint(); };
    addAndMakeVisible(envCurveKnob);
    envCurveKnob.setLookAndFeel(&ishtarLookAndFeel);
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
    vintageKnob.setLookAndFeel(&ishtarLookAndFeel);
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
            knob.setLookAndFeel(&ishtarLookAndFeel);  // Apply Ishtar knob design
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
    setupVerticalSlider(detuneMixSlider, detuneMixLabel, "Mix", 0.0, 1.0, 0.0);
    setupVerticalSlider(warmthSlider, warmthLabel, "Warmth", 0.0, 1.0, 0.0);
    setupVerticalSlider(punchSlider, punchLabel, "Punch", 0.0, 1.0, 0.0);
    setupVerticalSlider(punchFrequencySlider, punchFrequencyLabel, "Freq", 500.0, 5000.0, 1800.0);

    // Set up effects value displays
    auto setupValueLabel = [this](juce::Label& label, const juce::String& initialText) {
        label.setText(initialText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        label.setColour(juce::Label::backgroundColourId, juce::Colours::black);
        label.setEditable(true);
        addAndMakeVisible(label);
        };

    setupValueLabel(detuneAmountValue, "0.00");
    setupValueLabel(detuneMixValue, "0.00");
    setupValueLabel(warmthValue, "0.00");
    setupValueLabel(punchValue, "0.00");
    setupValueLabel(punchFrequencyValue, "1800");
    setupValueLabel(brillianceValue, "0.50");
    brillianceValue.setVisible(false);
    brillianceMainLabel.setVisible(false);
    setupValueLabel(vibratoRateValue, "5.00");
    setupValueLabel(vibratoDepthValue, "0.00");
    setupValueLabel(vibratoFadeValue, "2.00");
    setupValueLabel(pitchDistValue, "0.00");
    setupValueLabel(pitchTimeValue, "0.50");

    // Wire up value display updates via onValueChange
    detuneAmountSlider.onValueChange = [this]() {
        detuneAmountValue.setText(juce::String(detuneAmountSlider.getValue(), 2), juce::dontSendNotification);
        };
    detuneMixSlider.onValueChange = [this]() {
        detuneMixValue.setText(juce::String(detuneMixSlider.getValue(), 2), juce::dontSendNotification);
        };
    warmthSlider.onValueChange = [this]() {
        warmthValue.setText(juce::String(warmthSlider.getValue(), 2), juce::dontSendNotification);
        };
    punchSlider.onValueChange = [this]() {
        punchValue.setText(juce::String(punchSlider.getValue(), 2), juce::dontSendNotification);
        };
    punchFrequencySlider.onValueChange = [this]() {
        punchFrequencyValue.setText(juce::String((int)punchFrequencySlider.getValue()), juce::dontSendNotification);
        };
    brillianceSlider.onValueChange = [this]() {
        brillianceValue.setText(juce::String(brillianceSlider.getValue(), 2), juce::dontSendNotification);
        };
    vibratoRateKnob.onValueChange = [this]() {
        vibratoRateValue.setText(juce::String(vibratoRateKnob.getValue(), 2), juce::dontSendNotification);
        };
    vibratoDepthKnob.onValueChange = [this]() {
        vibratoDepthValue.setText(juce::String(vibratoDepthKnob.getValue(), 2), juce::dontSendNotification);
        };
    vibratoFadeKnob.onValueChange = [this]() {
        vibratoFadeValue.setText(juce::String(vibratoFadeKnob.getValue(), 2), juce::dontSendNotification);
        };
    pitchDistKnob.onValueChange = [this]() {
        pitchDistValue.setText(juce::String(pitchDistKnob.getValue(), 2), juce::dontSendNotification);
        };
    pitchTimeKnob.onValueChange = [this]() {
        pitchTimeValue.setText(juce::String(pitchTimeKnob.getValue(), 2), juce::dontSendNotification);
        };
    lfoSpeedKnob.onValueChange = [this]() {
        lfoSpeedValue.setText(juce::String(lfoSpeedKnob.getValue(), 2), juce::dontSendNotification);
        };
    lfoDepthKnob.onValueChange = [this]() {
        lfoDepthValue.setText(juce::String(lfoDepthKnob.getValue(), 2), juce::dontSendNotification);
        };



    // ======================== CREATE SLIDER ATTACHMENTS ========================
    
    // K1-K10 drawbar attachments
    const char* kParamNames[] = { "k1", "k2", "k3", "k4", "k5", "k6", "k7", "k8", "k9", "k10" };
    for (int i = 0; i < 10; ++i)
    {
        drawbarAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, kParamNames[i], drawbarSliders[i]);
    }
    
    // Right column attachments
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
    warmthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "warmth", warmthSlider);
    punchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "punch", punchSlider);
    punchFrequencyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "punchFrequency", punchFrequencySlider);



    // Amplitude section attachments
    velAmpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "velToAmplitude", velAmpSlider);

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
    
    // Wire up F value label editing
    for (int i = 0; i < 10; ++i)
    {
        fValueLabels[i].onTextChange = [this, i]() {
            float newVal = fValueLabels[i].getText().getFloatValue();
            newVal = std::round(newVal * 2.0f) / 2.0f;
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
    
    // Register as listener for amplitude envelope parameters
    apvts.addParameterListener("ampEnvAttackTime", this);
    apvts.addParameterListener("ampEnvDecayTime", this);
    apvts.addParameterListener("ampEnvSustainLevel", this);
    apvts.addParameterListener("ampEnvReleaseTime", this);

    // Register as listener for amplitude zone parameters
    apvts.addParameterListener("exponentialControl", this);

    apvts.addParameterListener("velToAmplitude", this);
    apvts.addParameterListener("velToAttackTime", this);
    apvts.addParameterListener("vintageAmount", this);
    apvts.addParameterListener("transpose", this);

    // Register as listener for envelope depth parameters (K1-K10)
    for (int i = 1; i <= 10; ++i)
    {
        juce::String paramID = "k" + juce::String(i) + "EnvelopeAmount";
        apvts.addParameterListener(paramID, this);
    }

    // Register as listener for F multiplier parameters (input_f1-input_f10)
    for (int i = 1; i <= 10; ++i)
    {
        juce::String paramID = "input_f" + juce::String(i);
        apvts.addParameterListener(paramID, this);
    }

    // ======================== PATCH MANAGEMENT UI SETUP ========================
    loadPatchButton.setButtonText("Load");
    loadPatchButton.onClick = [this] { loadPatchButtonClicked(); };
    addAndMakeVisible(loadPatchButton);

    savePatchButton.setButtonText("Save");
    savePatchButton.onClick = [this] { savePatchButtonClicked(); };
    addAndMakeVisible(savePatchButton);

    // Get patch metadata from processor if available
    if (auto* proc = dynamic_cast<PLANETtest4AudioProcessor*>(audioProcessor))
    {
        currentPatchName = proc->currentPatchName;
        patchCommentLabel.setText(proc->currentPatchDescription, juce::dontSendNotification);
    }
    else
    {
        currentPatchName = "Init";
    }

    currentPatchLabel.setText(currentPatchName, juce::dontSendNotification);
    currentPatchLabel.setJustificationType(juce::Justification::centredLeft);
    currentPatchLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    currentPatchLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1a1a1a));
    addAndMakeVisible(currentPatchLabel);

    patchCommentLabel.setJustificationType(juce::Justification::centredLeft);
    patchCommentLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
    addAndMakeVisible(patchCommentLabel);

    // Master volume slider (horizontal)
    masterVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    masterVolumeSlider.setRange(0.0, 1.0, 0.01);
    masterVolumeSlider.setValue(0.8);
    masterVolumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterVolumeSlider.setDoubleClickReturnValue(true, 0.8);
    addAndMakeVisible(masterVolumeSlider);

    masterVolumeLabel.setText("Vol", juce::dontSendNotification);
    masterVolumeLabel.setJustificationType(juce::Justification::centredRight);
    masterVolumeLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    addAndMakeVisible(masterVolumeLabel);

    // Transpose control (editable numeric)
    transposeLabel.setText("Trans", juce::dontSendNotification);
    transposeLabel.setJustificationType(juce::Justification::centredRight);
    transposeLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    addAndMakeVisible(transposeLabel);

    transposeValue.setText("0", juce::dontSendNotification);
    transposeValue.setJustificationType(juce::Justification::centred);
    transposeValue.setColour(juce::Label::textColourId, juce::Colours::white);
    transposeValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    transposeValue.setColour(juce::Label::outlineColourId, juce::Colours::grey);
    transposeValue.setEditable(true);
    addAndMakeVisible(transposeValue);

    // Wire up transpose editing
    transposeValue.onTextChange = [this]() {
        int newVal = transposeValue.getText().getIntValue();
        newVal = juce::jlimit(-24, 24, newVal);
        if (auto* param = apvts.getParameter("transpose"))
            param->setValueNotifyingHost(param->convertTo0to1((float)newVal));
        transposeValue.setText(juce::String(newVal), juce::dontSendNotification);
        };

    // Attachments
    masterVolumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "masterVolume", masterVolumeSlider);

    addAndMakeVisible(waveformDisplay);

    // Apply Amarna font to all labels
    for (int i = 0; i < 10; ++i)
        styleLabel(fValueLabels[i], true);

    for (int i = 0; i < 4; ++i) {
        styleLabel(adsrLabels[i]);
        styleLabel(adsrValueEditors[i], true);
        styleLabel(ampAdsrLabels[i]);
        styleLabel(ampAdsrValueEditors[i], true);
    }

    styleLabel(lfoShapeLabel);
    styleLabel(lfoSpeedLabel);
    styleLabel(lfoDepthLabel);
    styleLabel(lfoSpeedValue, true);
    styleLabel(lfoDepthValue, true);

    styleLabel(selectedFDisplay);
    styleLabel(envDepthLabel);
    styleLabel(envDepthValue, true);
    styleLabel(velToDrawbarLabel);
    styleLabel(velToDrawbarValue, true);
    styleLabel(velAmpLabel);
    styleLabel(velAmpValue, true);

    styleLabel(velAttackLabel);
    styleLabel(velAttackValue, true);
    styleLabel(envCurveLabel);
    styleLabel(envCurveValue, true);
    styleLabel(vintageLabel);
    styleLabel(vintageValue, true);

    // Right column labels
    styleLabel(vibratoRateLabel);
    styleLabel(vibratoDepthLabel);
    styleLabel(vibratoFadeLabel);
    styleLabel(vibratoRateValue, true);
    styleLabel(vibratoDepthValue, true);
    styleLabel(vibratoFadeValue, true);
    styleLabel(pitchDistLabel);
    styleLabel(pitchTimeLabel);
    styleLabel(pitchDistValue, true);
    styleLabel(pitchTimeValue, true);
    styleLabel(brillianceMainLabel);
    styleLabel(brillianceValue, true);
    styleLabel(detuneAmountLabel);
    styleLabel(detuneMixLabel);
    styleLabel(warmthLabel);
    styleLabel(punchLabel);
    styleLabel(punchFrequencyLabel);
    styleLabel(detuneAmountValue, true);
    styleLabel(detuneMixValue, true);
    styleLabel(warmthValue, true);
    styleLabel(punchValue, true);
    styleLabel(punchFrequencyValue, true);

    // Patch bar
    styleLabel(currentPatchLabel);
    styleLabel(patchCommentLabel, true);
    styleLabel(masterVolumeLabel);
    styleLabel(transposeLabel);
    styleLabel(transposeValue, true);


    setSize(1400, 800);
    updateDrawbarColors();
    startTimerHz(30);
}

PLANETMainGui::~PLANETMainGui()
{
    stopTimer();

    // Reset LookAndFeel for drawbar sliders before destruction
    for (int i = 0; i < 10; ++i)
    {
        drawbarSliders[i].setLookAndFeel(nullptr);
    }

    // Reset LookAndFeel for rotary knobs
    vibratoRateKnob.setLookAndFeel(nullptr);
    vibratoDepthKnob.setLookAndFeel(nullptr);
    vibratoFadeKnob.setLookAndFeel(nullptr);
    pitchDistKnob.setLookAndFeel(nullptr);
    pitchTimeKnob.setLookAndFeel(nullptr);
    lfoSpeedKnob.setLookAndFeel(nullptr);
    lfoDepthKnob.setLookAndFeel(nullptr);
    velToDrawbarKnob.setLookAndFeel(nullptr);
    
    velAttackKnob.setLookAndFeel(nullptr);
    envCurveKnob.setLookAndFeel(nullptr);
    vintageKnob.setLookAndFeel(nullptr);

    apvts.removeParameterListener("ampEnvAttackTime", this);
    apvts.removeParameterListener("ampEnvDecayTime", this);
    apvts.removeParameterListener("ampEnvSustainLevel", this);
    apvts.removeParameterListener("ampEnvReleaseTime", this);
    apvts.removeParameterListener("exponentialControl", this);
  
    apvts.removeParameterListener("velToAmplitude", this);
    apvts.removeParameterListener("velToAttackTime", this);
    apvts.removeParameterListener("vintageAmount", this);
    apvts.removeParameterListener("transpose", this);

    // Remove listeners for envelope depth parameters (K1-K10)
    for (int i = 1; i <= 10; ++i)
    {
        juce::String paramID = "k" + juce::String(i) + "EnvelopeAmount";
        apvts.removeParameterListener(paramID, this);
    }

    // Remove listeners for F multiplier parameters (input_f1-input_f10)
    for (int i = 1; i <= 10; ++i)
    {
        juce::String paramID = "input_f" + juce::String(i);
        apvts.removeParameterListener(paramID, this);
    }
}

void PLANETMainGui::timerCallback()
{
    if (rawModWheelValue != nullptr && modWheelEngaged != nullptr)
    {
        float S = (float)brillianceSlider.getValue();
        float newEffective = S;  // Default to slider

        if (modWheelEngaged->load())
        {
            float M = rawModWheelValue->load();
            if (M < 0.5f)
                newEffective = M * 2.0f * S;
            else
                newEffective = S + (M - 0.5f) * 2.0f * (1.0f - S);
        }

        if (std::abs(newEffective - cachedEffectiveBrilliance) > 0.001f)
        {
            cachedEffectiveBrilliance = newEffective;
            repaint(brillianceSliderBounds.expanded(5));
        }
    }

    updateDrawbarColors();
}

void PLANETMainGui::updateDrawbarColors()
{
    for (int i = 0; i < 10; ++i)
    {
        // Get current drawbar value
        float drawbarValue = drawbarSliders[i].getValue();
        bool isNull = std::abs(drawbarValue) < 0.001f;

        // Get envelope amount (use RawParameterValue for direct access)
        juce::String envParamID = "k" + juce::String(i + 1) + "EnvelopeAmount";
        float envAmount = 0.0f;
        if (auto* envPtr = apvts.getRawParameterValue(envParamID))
            envAmount = envPtr->load();
        bool hasActiveEnvelope = std::abs(envAmount) > 0.001f;

        // Get LFO amount (use RawParameterValue for direct access)
        juce::String lfoParamID = "k" + juce::String(i + 1) + "LFOAmount";
        float lfoAmount = 0.0f;
        if (auto* lfoPtr = apvts.getRawParameterValue(lfoParamID))
            lfoAmount = lfoPtr->load();
        bool hasActiveLFO = std::abs(lfoAmount) > 0.001f;

        // Determine thumb color based on state priority:
        // 1. Red if envelope is active
        // 2. Blue if drawbar is non-null
        // 3. White if drawbar is at null position
        juce::Colour thumbColour;
        if (hasActiveEnvelope) {
            thumbColour = juce::Colour(0xffcc4444);  // Muted red
        } else if (!isNull) {
            thumbColour = accentColour;  // Pale blue (0xff6ab0ff)
        } else {
            thumbColour = juce::Colours::white;  // White for null position
        }

        drawbarSliders[i].setColour(juce::Slider::thumbColourId, thumbColour);

        // Store LFO state as a property for the custom LookAndFeel to read
        drawbarSliders[i].getProperties().set("hasActiveLFO", hasActiveLFO);

        // Check Vel to Harmonic amount
        juce::String velHarmParamID = "k" + juce::String(i + 1) + "VelToHarmonic";
        float velHarmAmount = 0.0f;
        if (auto* velHarmPtr = apvts.getRawParameterValue(velHarmParamID))
            velHarmAmount = velHarmPtr->load();
        bool hasActiveVelHarm = std::abs(velHarmAmount) > 0.1f;

        drawbarSliders[i].getProperties().set("hasActiveVelHarm", hasActiveVelHarm);

        // Always trigger repaint to update visual state immediately
        drawbarSliders[i].repaint();
    }
}

//==============================================================================
// ENVELOPE DRAWING HELPER - Eliminates duplication between harmonic and amplitude envelopes
//==============================================================================
void PLANETMainGui::drawEnvelopeCurve(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                                       float attack, float decay, float sustain, float release,
                                       float curveAmount, juce::Colour strokeColour, juce::Colour handleOutlineColour)
{
    float curveFactor = 1.0f + curveAmount * 6.0f;
    float totalTime = attack + decay + 0.3f + release;
    if (totalTime < 0.1f) totalTime = 0.1f;
    float timeScale = (float)bounds.getWidth() / totalTime;
    
    float x0 = (float)bounds.getX();
    float y0 = (float)bounds.getBottom();
    float yTop = (float)bounds.getY() + 10;
    float ySustain = yTop + (1.0f - sustain) * (y0 - yTop - 10);
    
    float x1 = x0 + attack * timeScale;
    float x2 = x1 + decay * timeScale;
    float x3 = x2 + 0.3f * timeScale;
    float x4 = x3 + release * timeScale;
    
    juce::Path envPath;
    envPath.startNewSubPath(x0, y0);
    const int numSegments = 20;
    
    // Helper lambda for curved segments
    auto addCurvedSegment = [&](float startX, float endX, float startY, float endY, bool isAttack) {
        if (curveAmount > 0.001f) {
            for (int i = 1; i <= numSegments; ++i) {
                float t = (float)i / numSegments;
                float curvedT;
                if (isAttack) {
                    curvedT = 1.0f - std::exp(-curveFactor * t);
                    float maxCurve = 1.0f - std::exp(-curveFactor);
                    curvedT /= maxCurve;
                } else {
                    curvedT = std::exp(-curveFactor * t);
                    float minCurve = std::exp(-curveFactor);
                    curvedT = (curvedT - minCurve) / (1.0f - minCurve);
                    curvedT = 1.0f - curvedT;
                }
                envPath.lineTo(startX + (endX - startX) * t, startY + (endY - startY) * curvedT);
            }
        } else {
            envPath.lineTo(endX, endY);
        }
    };
    
    addCurvedSegment(x0, x1, y0, yTop, true);        // Attack
    addCurvedSegment(x1, x2, yTop, ySustain, false); // Decay
    envPath.lineTo(x3, ySustain);                    // Sustain hold
    addCurvedSegment(x3, x4, ySustain, y0, false);   // Release
    
    g.setColour(strokeColour);
    g.strokePath(envPath, juce::PathStrokeType(2.5f));
    
    // Draw handles
    float handleRadius = 6.0f;
    g.setColour(juce::Colours::white);
    g.fillEllipse(x1 - handleRadius, yTop - handleRadius, handleRadius * 2, handleRadius * 2);
    g.fillEllipse(x2 - handleRadius, ySustain - handleRadius, handleRadius * 2, handleRadius * 2);
    g.fillEllipse(x4 - handleRadius, y0 - handleRadius, handleRadius * 2, handleRadius * 2);
    
    g.setColour(handleOutlineColour);
    g.drawEllipse(x1 - handleRadius, yTop - handleRadius, handleRadius * 2, handleRadius * 2, 2.0f);
    g.drawEllipse(x2 - handleRadius, ySustain - handleRadius, handleRadius * 2, handleRadius * 2, 2.0f);
    g.drawEllipse(x4 - handleRadius, y0 - handleRadius, handleRadius * 2, handleRadius * 2, 2.0f);
}

void PLANETMainGui::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    int leftWidth = (int)(bounds.getWidth() * leftWidthRatio);
    int rightWidth = bounds.getWidth() - leftWidth;
    int mainHeight = bounds.getHeight() - patchBarHeight;
    
    int harmonicAndAmpHeight = mainHeight - drawbarSectionHeight;
    int harmonicHeight = harmonicAndAmpHeight / 2;
    int ampHeight = harmonicAndAmpHeight - harmonicHeight;

    // ======================== LEFT SIDE ========================
    
    // Drawbar section
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
    
    // Harmonic section
    g.setColour(drawbarColours[selectedDrawbar].withAlpha(0.3f));
    g.fillRect(0, drawbarSectionHeight, leftWidth, harmonicHeight);

    // Draw Harmonic ADSR envelope
    {
        int adsrZoneWidth = (int)(leftWidth * 0.65f);
        int adsrMargin = 20;
        int adsrGraphHeight = harmonicHeight - 80;
        int adsrGraphY = drawbarSectionHeight + 10;
        int adsrGraphWidth = adsrZoneWidth - adsrMargin * 2;

        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle((float)adsrMargin, (float)adsrGraphY,
            (float)adsrGraphWidth, (float)adsrGraphHeight, 5.0f);

        // Draw DRAWBAR N watermark
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.setFont(amarnaSemiBold.withHeight(36.0f));
        juce::String watermarkText = "DRAWBAR " + juce::String(selectedDrawbar + 1);
        g.drawText(watermarkText, adsrMargin, adsrGraphY, adsrGraphWidth, adsrGraphHeight,
            juce::Justification::centred, false);
        
        juce::Rectangle<int> envBounds(adsrMargin, adsrGraphY, adsrGraphWidth, adsrGraphHeight);
        drawEnvelopeCurve(g, envBounds,
                          adsrValues[selectedDrawbar][0], adsrValues[selectedDrawbar][1],
                          adsrValues[selectedDrawbar][2], adsrValues[selectedDrawbar][3],
                          (float)envCurveKnob.getValue(),
                          drawbarColours[selectedDrawbar], drawbarColours[selectedDrawbar]);
    }
    
    // Amplitude section
    g.setColour(backgroundGlobal);
    g.fillRect(0, drawbarSectionHeight + harmonicHeight, leftWidth, ampHeight);

    // Draw Amplitude ADSR envelope
    {
        int adsrZoneWidth = (int)(leftWidth * 0.65f);
        int adsrMargin = 20;
        int adsrGraphHeight = ampHeight - 80;
        int adsrGraphY = drawbarSectionHeight + harmonicHeight + 10;
        int adsrGraphWidth = adsrZoneWidth - adsrMargin * 2;
        
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle((float)adsrMargin, (float)adsrGraphY, 
                                (float)adsrGraphWidth, (float)adsrGraphHeight, 5.0f);
        
        juce::Rectangle<int> envBounds(adsrMargin, adsrGraphY, adsrGraphWidth, adsrGraphHeight);
        drawEnvelopeCurve(g, envBounds,
                          ampAdsrValues[0], ampAdsrValues[1], ampAdsrValues[2], ampAdsrValues[3],
                          (float)envCurveKnob.getValue(),
                          juce::Colours::white, accentColour);
    }

    // ======================== RIGHT SIDE ========================
    
    g.setColour(backgroundGlobal);
    g.fillRect(leftWidth, 0, rightWidth, mainHeight);










    // ======================== PATCH BAR ========================
    
    g.setColour(backgroundLight.darker(0.3f));
    g.fillRect(0, mainHeight, bounds.getWidth(), patchBarHeight);

    // ======================== SECTION LABELS ========================

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(amarnaSemiBold.withHeight(20.0f));

    int labelHeight = 22;
    int labelMargin = 1;

    // Left column labels (centered at bottom of each zone)
    g.drawText("DRAWBARS", 0, drawbarSectionHeight - labelHeight - labelMargin, leftWidth, labelHeight, juce::Justification::centred);
    g.drawText("DRAWBAR ENVELOPE", 0, drawbarSectionHeight + harmonicHeight - labelHeight - labelMargin, leftWidth, labelHeight, juce::Justification::centred);
    g.drawText("AMPLITUDE ENVELOPE", 0, mainHeight - labelHeight - labelMargin, leftWidth, labelHeight, juce::Justification::centred);

    int rightX = leftWidth;
    int rightContentHeight = mainHeight - drawbarSectionHeight;
    int rightSectionHeight = rightContentHeight / 4;
    int rightSectionY = drawbarSectionHeight;

    // Right column labels (centered at bottom of each zone)
    g.drawText("VIBRATO", rightX, rightSectionY + rightSectionHeight - labelHeight - labelMargin, rightWidth, labelHeight, juce::Justification::centred);
    g.drawText("PITCH ENVELOPE", rightX, rightSectionY + rightSectionHeight * 2 - labelHeight - labelMargin, rightWidth, labelHeight, juce::Justification::centred);
    g.drawText("BRILLIANCE", rightX, rightSectionY + rightSectionHeight * 3 - 10 - labelHeight - labelMargin, rightWidth, labelHeight, juce::Justification::centred);
    g.drawText("EFFECTS", rightX, mainHeight - labelHeight - labelMargin, rightWidth, labelHeight, juce::Justification::centred);

    // ISHTAR name - aligned with left/right column divider
    g.setColour(accentColour.withAlpha(0.9f));
    g.setFont(amarnaSemiBold.withHeight(30.0f));
    g.drawText("ISHTAR", leftWidth + 10, mainHeight + (patchBarHeight - 22) / 2, 100, 22, juce::Justification::left);

    // Draw effective brilliance indicator (only when mod wheel is engaged and not at center)
    if (rawModWheelValue != nullptr && modWheelEngaged != nullptr && brillianceSliderBounds.getWidth() > 0)
    {
        if (modWheelEngaged->load())
        {
            float M = rawModWheelValue->load();

            // Only show indicator when mod wheel is away from center
            if (std::abs(M - 0.5f) > 0.01f)
            {
                float S = (float)brillianceSlider.getValue();
                float effectiveBrilliance;

                if (M < 0.5f)
                    effectiveBrilliance = M * 2.0f * S;
                else
                    effectiveBrilliance = S + (M - 0.5f) * 2.0f * (1.0f - S);

                int sliderX = brillianceSliderBounds.getX();
                int sliderWidth = brillianceSliderBounds.getWidth();
                int sliderY = brillianceSliderBounds.getY();
                int sliderHeight = brillianceSliderBounds.getHeight();

                int baseX = sliderX + (int)(S * sliderWidth);
                int effectiveX = sliderX + (int)(effectiveBrilliance * sliderWidth);

                g.setColour(accentColour.withAlpha(0.5f));
                int barY = sliderY + sliderHeight / 2 - 3;

                // Draw bar from slider position to effective position
                int barLeft = juce::jmin(baseX, effectiveX);
                int barRight = juce::jmax(baseX, effectiveX);
                g.fillRect(barLeft, barY, barRight - barLeft, 6);

                g.setColour(accentColour);
                g.fillRect(effectiveX - 2, sliderY + 5, 4, sliderHeight - 10);
            }
        }
    }
    
    

    // Draw brackets connecting related effect controls
    {
        int rightContentWidth = bounds.getWidth() - leftWidth - 20;
        int effectsSliderSpacing = rightContentWidth / 5;
        int effectsSliderWidth = 55;
        int effectsY = drawbarSectionHeight + rightSectionHeight * 3 + 7;
        int bracketY = effectsY + 15;  // Just below the labels
        int bracketHeight = 5;
        int bracketThickness = 2;

        // Calculate center positions for all effect sliders
        int esx0 = leftWidth + 10 + (effectsSliderSpacing - effectsSliderWidth) / 2;
        int esx1 = leftWidth + 10 + effectsSliderSpacing + (effectsSliderSpacing - effectsSliderWidth) / 2;
        int esx3 = leftWidth + 10 + effectsSliderSpacing * 3 + (effectsSliderSpacing - effectsSliderWidth) / 2;
        int esx4 = leftWidth + 10 + effectsSliderSpacing * 4 + (effectsSliderSpacing - effectsSliderWidth) / 2;

        int detuneCenter = esx0 + effectsSliderWidth / 2;
        int mixCenter = esx1 + effectsSliderWidth / 2;
        int punchCenter = esx3 + effectsSliderWidth / 2;
        int freqCenter = esx4 + effectsSliderWidth / 2;

        g.setColour(juce::Colours::white.withAlpha(0.5f));

        // Detune-Mix bracket
        g.fillRect(detuneCenter, bracketY, mixCenter - detuneCenter, bracketThickness);
        g.fillRect(detuneCenter, bracketY - bracketHeight, bracketThickness, bracketHeight);
        g.fillRect(mixCenter - bracketThickness + 1, bracketY - bracketHeight, bracketThickness, bracketHeight);

        // Punch-Freq bracket
        g.fillRect(punchCenter, bracketY, freqCenter - punchCenter, bracketThickness);
        g.fillRect(punchCenter, bracketY - bracketHeight, bracketThickness, bracketHeight);
        g.fillRect(freqCenter - bracketThickness + 1, bracketY - bracketHeight, bracketThickness, bracketHeight);
    }

    

    // ======================== DIVIDING LINES ========================
    
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawVerticalLine(leftWidth, 0, (float)mainHeight);
    g.drawHorizontalLine(drawbarSectionHeight, 0, (float)leftWidth);
    g.drawHorizontalLine(drawbarSectionHeight + harmonicHeight, 0, (float)leftWidth);
    g.drawHorizontalLine(drawbarSectionHeight, (float)leftWidth, (float)bounds.getWidth());
    // Right column dividers (Vibrato/Pitch and Pitch/Brilliance)
    for (int i = 1; i < 3; ++i)
        g.drawHorizontalLine(drawbarSectionHeight + rightSectionHeight * i, (float)leftWidth, (float)bounds.getWidth());

    // Brilliance/Effects divider (adjustable)<===============================================================================BRILLIANCE DIVIDER
    int brillianceEffectsDividerY = drawbarSectionHeight + rightSectionHeight * 3 - 10;  // Adjust this offset
    g.drawHorizontalLine(brillianceEffectsDividerY, (float)leftWidth, (float)bounds.getWidth());
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
        fValueLabels[i].setBounds(x + 5, drawbarMargin, drawbarWidth - 10, fLabelHeight);
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
    int adsrLabelY = drawbarSectionHeight + adsrGraphHeight + 5;
    int adsrFieldWidth = 50;
    int adsrFieldHeight = 25;
    int adsrSpacing = (adsrZoneWidth - 40) / 4;

    harmonicEnvBounds = juce::Rectangle<int>(20, adsrGraphY, adsrGraphWidth, adsrGraphHeight);

    for (int i = 0; i < 4; ++i)
    {
        int xPos = 20 + i * adsrSpacing + (adsrSpacing - adsrFieldWidth) / 2;
        adsrLabels[i].setBounds(xPos, adsrLabelY, adsrFieldWidth, 20);
        adsrValueEditors[i].setBounds(xPos, adsrLabelY + 22, adsrFieldWidth, adsrFieldHeight);
    }

    // Position LFO controls
    int envDepthSliderWidth = 50;
    int envDepthX = adsrZoneWidth + 10;
    int lfoZoneX = envDepthX + envDepthSliderWidth + 20;
    int lfoZoneWidth = leftWidth - lfoZoneX;
    int lfoZoneY = drawbarSectionHeight + 10;
    int knobSize = 80;

    int envDepthY = adsrGraphY;
    int envDepthSliderHeight = adsrGraphHeight;
    envDepthKnob.setBounds(envDepthX, envDepthY, envDepthSliderWidth, envDepthSliderHeight);
    envDepthLabel.setBounds(envDepthX - 10, envDepthY + envDepthSliderHeight + 2, envDepthSliderWidth + 20, 18);
    envDepthValue.setBounds(envDepthX, envDepthY + envDepthSliderHeight + 20, envDepthSliderWidth, 22);

    // Hide the old F display - now drawn as watermark in paint()
    selectedFDisplay.setVisible(false);

    // Triangle layout: Vel to Drawbar at apex, LFO Speed/Depth at base
    int smallKnobSize = 60;
    int smallKnobValueHeight = 18;
    

    // Apex knob (Vel to Drawbar) - centered at top
    int apexX = lfoZoneX + (lfoZoneWidth - smallKnobSize) / 2;
    int apexY = lfoZoneY + 5;
    velToDrawbarLabel.setBounds(apexX - 15, apexY, smallKnobSize + 30, 16);
    velToDrawbarKnob.setBounds(apexX, apexY + 16, smallKnobSize, smallKnobSize);
    velToDrawbarValue.setBounds(apexX, apexY + 16 + smallKnobSize, smallKnobSize, smallKnobValueHeight);

    // Base knobs (LFO Speed, LFO Depth) - spread below
    int baseY = apexY + 16 + smallKnobSize + smallKnobValueHeight + 15;
    int baseSpacing = (lfoZoneWidth - smallKnobSize * 2) / 3;
    int base1X = lfoZoneX + baseSpacing;
    int base2X = base1X + smallKnobSize + baseSpacing;

    lfoSpeedValue.setBounds(base1X, baseY + 16 + smallKnobSize, smallKnobSize, smallKnobValueHeight);
    lfoDepthValue.setBounds(base2X, baseY + 16 + smallKnobSize, smallKnobSize, smallKnobValueHeight);

    lfoSpeedLabel.setBounds(base1X, baseY, smallKnobSize, 16);
    lfoSpeedKnob.setBounds(base1X, baseY + 16, smallKnobSize, smallKnobSize);

    lfoDepthLabel.setBounds(base2X, baseY, smallKnobSize, 16);
    lfoDepthKnob.setBounds(base2X, baseY + 16, smallKnobSize, smallKnobSize);

    // LFO Shape combo below the base knobs and their values

    lfoSpeedValue.setBounds(base1X, baseY + 16 + smallKnobSize, smallKnobSize, smallKnobValueHeight);
    lfoDepthValue.setBounds(base2X, baseY + 16 + smallKnobSize, smallKnobSize, smallKnobValueHeight);
    int comboY = baseY + 16 + smallKnobSize + smallKnobValueHeight + 5;
    int comboWidth = (lfoZoneWidth - 30) / 2;
    lfoShapeLabel.setBounds(lfoZoneX + 10, comboY, comboWidth - 5, 22);
    lfoShapeCombo.setBounds(lfoZoneX + 10 + comboWidth, comboY, comboWidth, 22);

    // Define knob1X, knob2X, knobSize for Amplitude section below (legacy layout)
    int knob1X = base1X;
    int knob2X = base2X;
    // knobSize already defined as 80, keep that for Amplitude zone centering

    // ======================== AMPLITUDE ADSR SECTION ========================
    int ampHeight = harmonicAndAmpHeight - harmonicHeight;
    int ampAdsrGraphHeight = ampHeight - 80;
    int ampAdsrGraphY = drawbarSectionHeight + harmonicHeight + 10;
    int ampAdsrLabelY = ampAdsrGraphY + ampAdsrGraphHeight - 3;

    ampEnvBounds = juce::Rectangle<int>(20, ampAdsrGraphY, adsrGraphWidth, ampAdsrGraphHeight);

    for (int i = 0; i < 4; ++i)
    {
        int xPos = 20 + i * adsrSpacing + (adsrSpacing - adsrFieldWidth) / 2;
        ampAdsrLabels[i].setBounds(xPos, ampAdsrLabelY, adsrFieldWidth, 20);
        ampAdsrValueEditors[i].setBounds(xPos, ampAdsrLabelY + 22, adsrFieldWidth, adsrFieldHeight);
    }

    velAmpSlider.setBounds(envDepthX, ampAdsrGraphY, envDepthSliderWidth, ampAdsrGraphHeight + 5);
    velAmpLabel.setBounds(envDepthX - 10, ampAdsrGraphY + ampAdsrGraphHeight + 8, envDepthSliderWidth + 20, 18);
    velAmpValue.setBounds(envDepthX, ampAdsrGraphY + ampAdsrGraphHeight + 24, envDepthSliderWidth, 22);

    // Triangle layout in Amplitude zone: Vel to Attk at apex, Env Curve and Vintage at base
    int ampKnobSize = 60;
    int ampKnobValueHeight = 18;

    // Apex knob (Vel to Attk) - centered at top
    int ampApexX = lfoZoneX + (lfoZoneWidth - ampKnobSize) / 2;
    int ampApexY = drawbarSectionHeight + harmonicHeight + 30;
    velAttackLabel.setBounds(ampApexX - 10, ampApexY, ampKnobSize + 20, 16);
    velAttackKnob.setBounds(ampApexX, ampApexY + 16, ampKnobSize, ampKnobSize);
    velAttackValue.setBounds(ampApexX, ampApexY + 16 + ampKnobSize, ampKnobSize, ampKnobValueHeight);

    // Base knobs (Env Curve, Vintage) - spread below
    int ampBaseY = ampApexY + 16 + ampKnobSize + ampKnobValueHeight + 15;
    int ampBaseSpacing = (lfoZoneWidth - ampKnobSize * 2) / 3;
    int ampBase1X = lfoZoneX + ampBaseSpacing;
    int ampBase2X = ampBase1X + ampKnobSize + ampBaseSpacing;

    envCurveLabel.setBounds(ampBase1X, ampBaseY, ampKnobSize, 16);
    envCurveKnob.setBounds(ampBase1X, ampBaseY + 16, ampKnobSize, ampKnobSize);
    envCurveValue.setBounds(ampBase1X, ampBaseY + 16 + ampKnobSize, ampKnobSize, ampKnobValueHeight);

    vintageLabel.setBounds(ampBase2X, ampBaseY, ampKnobSize, 16);
    vintageKnob.setBounds(ampBase2X, ampBaseY + 16, ampKnobSize, ampKnobSize);
    vintageValue.setBounds(ampBase2X, ampBaseY + 16 + ampKnobSize, ampKnobSize, ampKnobValueHeight);

    // ======================== RIGHT COLUMN LAYOUT ========================
    int rightX = leftWidth + 10;
    int rightContentWidth = bounds.getWidth() - leftWidth - 20;
    int rightKnobSize = 80;

    // Waveform display
    waveformDisplay.setBounds(rightX, 10, rightContentWidth, drawbarSectionHeight - 20);
    
    int waveformHeight = drawbarSectionHeight;
    int remainingHeight = mainHeight - waveformHeight;
    int rightSectionHeight = remainingHeight / 4;
    
    // Vibrato section
    int vibratoY = waveformHeight + 5;
    int vibratoKnobSpacing = rightContentWidth / 3;
    int knobValueHeight = 20;

    int vkx0 = rightX + (vibratoKnobSpacing - rightKnobSize) / 2;
    vibratoRateLabel.setBounds(vkx0, vibratoY, rightKnobSize, 16);
    vibratoRateKnob.setBounds(vkx0, vibratoY + 16, rightKnobSize, rightKnobSize);
    vibratoRateValue.setBounds(vkx0 + 10, vibratoY + 16 + rightKnobSize, rightKnobSize - 20, knobValueHeight);

    int vkx1 = rightX + vibratoKnobSpacing + (vibratoKnobSpacing - rightKnobSize) / 2;
    vibratoDepthLabel.setBounds(vkx1, vibratoY, rightKnobSize, 16);
    vibratoDepthKnob.setBounds(vkx1, vibratoY + 16, rightKnobSize, rightKnobSize);
    vibratoDepthValue.setBounds(vkx1 + 10, vibratoY + 16 + rightKnobSize, rightKnobSize - 20, knobValueHeight);

    int vkx2 = rightX + vibratoKnobSpacing * 2 + (vibratoKnobSpacing - rightKnobSize) / 2;
    vibratoFadeLabel.setBounds(vkx2, vibratoY, rightKnobSize, 16);
    vibratoFadeKnob.setBounds(vkx2, vibratoY + 16, rightKnobSize, rightKnobSize);
    vibratoFadeValue.setBounds(vkx2 + 10, vibratoY + 16 + rightKnobSize, rightKnobSize - 20, knobValueHeight);
    
    // Pitch section
    int pitchY = waveformHeight + rightSectionHeight + 5;
    int pitchKnobSpacing = rightContentWidth / 2;

    int pkx0 = rightX + (pitchKnobSpacing - rightKnobSize) / 2;
    pitchDistLabel.setBounds(pkx0, pitchY, rightKnobSize, 16);
    pitchDistKnob.setBounds(pkx0, pitchY + 16, rightKnobSize, rightKnobSize);
    pitchDistValue.setBounds(pkx0 + 10, pitchY + 16 + rightKnobSize, rightKnobSize - 20, knobValueHeight);

    int pkx1 = rightX + pitchKnobSpacing + (pitchKnobSpacing - rightKnobSize) / 2;
    pitchTimeLabel.setBounds(pkx1, pitchY, rightKnobSize, 16);
    pitchTimeKnob.setBounds(pkx1, pitchY + 16, rightKnobSize, rightKnobSize);
    pitchTimeValue.setBounds(pkx1 + 10, pitchY + 16 + rightKnobSize, rightKnobSize - 20, knobValueHeight);
    
    // Brilliance section
    int brillianceY = waveformHeight + rightSectionHeight * 2 + 35;
    int sliderMargin = 20;
    int brillValueWidth = 50;
    brillianceMainLabel.setBounds(rightX, brillianceY, rightContentWidth - brillValueWidth - 10, 18);
    brillianceValue.setBounds(rightX + rightContentWidth - brillValueWidth - sliderMargin, brillianceY, brillValueWidth, 18);
    brillianceSliderBounds = juce::Rectangle<int>(rightX + sliderMargin, brillianceY + 22, rightContentWidth - sliderMargin * 2, 40);
    brillianceSlider.setBounds(brillianceSliderBounds);
    
    // Effects section (5 sliders: Detune, Mix, Warmth, Punch, Freq)
    int effectsY = waveformHeight + rightSectionHeight * 3 + 1;
    int effectsSliderWidth = 55;
    int effectsValueHeight = 20;
    int effectsSliderHeight = rightSectionHeight - 68;  // Room for label + value
    int effectsSliderSpacing = rightContentWidth / 5;

    int esx0 = rightX + (effectsSliderSpacing - effectsSliderWidth) / 2;
    detuneAmountLabel.setBounds(esx0, effectsY, effectsSliderWidth, 14);
    detuneAmountSlider.setBounds(esx0, effectsY + 16, effectsSliderWidth, effectsSliderHeight);
    detuneAmountValue.setBounds(esx0, effectsY + 18 + effectsSliderHeight, effectsSliderWidth, effectsValueHeight);

    int esx1 = rightX + effectsSliderSpacing + (effectsSliderSpacing - effectsSliderWidth) / 2;
    detuneMixLabel.setBounds(esx1, effectsY, effectsSliderWidth, 14);
    detuneMixSlider.setBounds(esx1, effectsY + 16, effectsSliderWidth, effectsSliderHeight);
    detuneMixValue.setBounds(esx1, effectsY + 18 + effectsSliderHeight, effectsSliderWidth, effectsValueHeight);

    int esx2 = rightX + effectsSliderSpacing * 2 + (effectsSliderSpacing - effectsSliderWidth) / 2;
    warmthLabel.setBounds(esx2, effectsY, effectsSliderWidth, 14);
    warmthSlider.setBounds(esx2, effectsY + 16, effectsSliderWidth, effectsSliderHeight);
    warmthValue.setBounds(esx2, effectsY + 18 + effectsSliderHeight, effectsSliderWidth, effectsValueHeight);

    int esx3 = rightX + effectsSliderSpacing * 3 + (effectsSliderSpacing - effectsSliderWidth) / 2;
    punchLabel.setBounds(esx3, effectsY, effectsSliderWidth, 14);
    punchSlider.setBounds(esx3, effectsY + 16, effectsSliderWidth, effectsSliderHeight);
    punchValue.setBounds(esx3, effectsY + 18 + effectsSliderHeight, effectsSliderWidth, effectsValueHeight);

    int esx4 = rightX + effectsSliderSpacing * 4 + (effectsSliderSpacing - effectsSliderWidth) / 2;
    punchFrequencyLabel.setBounds(esx4, effectsY, effectsSliderWidth, 14);
    punchFrequencySlider.setBounds(esx4, effectsY + 16, effectsSliderWidth, effectsSliderHeight);
    punchFrequencyValue.setBounds(esx4, effectsY + 18 + effectsSliderHeight, effectsSliderWidth, effectsValueHeight);

    // ======================== PATCH BAR LAYOUT ========================
    int patchBarY = bounds.getHeight() - patchBarHeight;
    int buttonWidth = 80;
    int buttonHeight = 28;
    int buttonY = patchBarY + (patchBarHeight - buttonHeight) / 2;
    int buttonMargin = 10;

    loadPatchButton.setBounds(buttonMargin, buttonY, buttonWidth, buttonHeight);
    savePatchButton.setBounds(buttonMargin + buttonWidth + 10, buttonY, buttonWidth, buttonHeight);

    // Patch name (fixed width, right after Save button)
    int patchNameX = buttonMargin + buttonWidth * 2 + 20;
    int patchNameWidth = 150;
    currentPatchLabel.setBounds(patchNameX, buttonY, patchNameWidth, buttonHeight);

    // Comment takes space up to ISHTAR label
    int commentX = patchNameX + patchNameWidth + 10;
    int commentWidth = leftWidth - commentX - 10;
    patchCommentLabel.setBounds(commentX, buttonY, commentWidth, buttonHeight);

    // ISHTAR name is drawn in paint() at leftWidth + 10
    // Master controls to the right of ISHTAR
    int masterControlsX = leftWidth + 120;  // After "ISHTAR" text

    // Transpose: label + value box
    transposeLabel.setBounds(masterControlsX, buttonY, 40, buttonHeight);
    transposeValue.setBounds(masterControlsX + 42, buttonY + 2, 40, buttonHeight - 4);

    // Volume: label + slider
    int volX = masterControlsX + 100;
    masterVolumeLabel.setBounds(volX, buttonY, 30, buttonHeight);
    masterVolumeSlider.setBounds(volX + 32, buttonY + 4, 100, buttonHeight - 8);

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
    
    float handleRadius = 10.0f;
    
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
    
    // Check drawbar background area
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
    float y0 = (float)bounds.getBottom();
    float yTop = (float)bounds.getY() + 10;
    float ySustain = yTop + (1.0f - sustain) * (y0 - yTop - 10);
    
    float x1 = x0 + attack * timeScale;
    float x2 = x1 + decay * timeScale;
    float x3 = x2 + 0.3f * timeScale;
    float x4 = x3 + release * timeScale;
    
    switch (pointIndex)
    {
        case 0: return { x0, y0 };
        case 1: return { x1, yTop };
        case 2: return { x2, ySustain };
        case 3: return { x3, ySustain };
        case 4: return { x4, y0 };
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
    
    float x1 = x0 + attack * timeScale;
    
    switch (currentDragTarget)
    {
        case DragTarget::HarmonicAttack:
        case DragTarget::AmpAttack:
        {
            float newX = juce::jlimit(x0, (float)bounds.getRight(), (float)event.x);
            float newAttack = (newX - x0) / timeScale;
            values[0] = juce::jlimit(0.001f, 10.0f, newAttack);
            break;
        }
        
        case DragTarget::HarmonicDecaySustain:
        case DragTarget::AmpDecaySustain:
        {
            float currentX1 = x0 + values[0] * timeScale;
            float newX = juce::jlimit(currentX1, (float)bounds.getRight(), (float)event.x);
            float newDecay = (newX - currentX1) / timeScale;
            values[1] = juce::jlimit(0.001f, 10.0f, newDecay);
            
            float newY = juce::jlimit(yTop, y0 - 10, (float)event.y);
            float newSustain = 1.0f - (newY - yTop) / yRange;
            values[2] = juce::jlimit(0.0f, 1.0f, newSustain);
            break;
        }
        
        case DragTarget::HarmonicRelease:
        case DragTarget::AmpRelease:
        {
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
    
    // Watermark is now drawn in paint(), just trigger repaint
    repaint();
    
    bindToSelectedDrawbar();
}

void PLANETMainGui::bindToSelectedDrawbar()
{
    juce::String prefix = "k" + juce::String(selectedDrawbar + 1);
    
    currentHarmonicParamIDs[0] = prefix + "AttackTime";
    currentHarmonicParamIDs[1] = prefix + "DecayTime";
    currentHarmonicParamIDs[2] = prefix + "SustainLevel";
    currentHarmonicParamIDs[3] = prefix + "ReleaseTime";
    
    lfoSpeedAttachment.reset();
    lfoDepthAttachment.reset();
    lfoShapeAttachment.reset();
    envDepthAttachment.reset();
    velToDrawbarAttachment.reset();

    lfoSpeedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "LFORate", lfoSpeedKnob);
    lfoDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "LFOAmount", lfoDepthKnob);
    lfoShapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, prefix + "LFOShape", lfoShapeCombo);
    envDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "EnvelopeAmount", envDepthKnob);
    velToDrawbarAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "VelToHarmonic", velToDrawbarKnob);
    
    if (auto* param = apvts.getParameter(currentHarmonicParamIDs[0]))
        adsrValues[selectedDrawbar][0] = param->convertFrom0to1(param->getValue());
    if (auto* param = apvts.getParameter(currentHarmonicParamIDs[1]))
        adsrValues[selectedDrawbar][1] = param->convertFrom0to1(param->getValue());
    if (auto* param = apvts.getParameter(currentHarmonicParamIDs[2]))
        adsrValues[selectedDrawbar][2] = param->convertFrom0to1(param->getValue());
    if (auto* param = apvts.getParameter(currentHarmonicParamIDs[3]))
        adsrValues[selectedDrawbar][3] = param->convertFrom0to1(param->getValue());
    
    for (int i = 0; i < 4; ++i)
        adsrValueEditors[i].setText(juce::String(adsrValues[selectedDrawbar][i], 2), juce::dontSendNotification);
    
    if (auto* param = apvts.getParameter(prefix + "EnvelopeAmount"))
        envDepthValue.setText(juce::String(param->convertFrom0to1(param->getValue()), 2), juce::dontSendNotification);

    if (auto* param = apvts.getParameter(prefix + "VelToHarmonic"))
        velToDrawbarValue.setText(juce::String((int)param->convertFrom0to1(param->getValue())), juce::dontSendNotification);
}

void PLANETMainGui::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Skip GUI updates during bulk patch loading
    if (suppressParameterUpdates)
        return;

    // Amplitude envelope parameters
    const char* ampParams[] = { "ampEnvAttackTime", "ampEnvDecayTime", "ampEnvSustainLevel", "ampEnvReleaseTime" };
    for (int i = 0; i < 4; ++i) {
        if (parameterID == ampParams[i]) {
            ampAdsrValues[i] = newValue;
            ampAdsrValueEditors[i].setText(juce::String(newValue, 2), juce::dontSendNotification);
            repaint();
            return;
        }
    }

    // Harmonic envelope parameters (currently selected)
    for (int i = 0; i < 4; ++i) {
        if (parameterID == currentHarmonicParamIDs[i]) {
            adsrValues[selectedDrawbar][i] = newValue;
            adsrValueEditors[i].setText(juce::String(newValue, 2), juce::dontSendNotification);
            repaint();
            return;
        }
    }

    // Amplitude zone parameters
    if (parameterID == "exponentialControl") {
        envCurveValue.setText(juce::String(newValue, 2), juce::dontSendNotification);
        return;
    }

    if (parameterID == "velToAmplitude") {
        velAmpValue.setText(juce::String(newValue, 2), juce::dontSendNotification);
        return;
    }
    if (parameterID == "velToAttackTime") {
        velAttackValue.setText(juce::String(newValue, 2), juce::dontSendNotification);
        return;
    }
    if (parameterID == "vintageAmount") {
        vintageValue.setText(juce::String(newValue, 2), juce::dontSendNotification);
        return;
    }
    if (parameterID == "transpose") {
        transposeValue.setText(juce::String((int)newValue), juce::dontSendNotification);
        return;
    }

    // Envelope depth for currently selected drawbar
    if (parameterID == juce::String("k") + juce::String(selectedDrawbar + 1) + "EnvelopeAmount") {
        envDepthValue.setText(juce::String(newValue, 2), juce::dontSendNotification);
        return;
    }

    // F multiplier parameters (input_f1-input_f10)
    if (parameterID.startsWith("input_f")) {
        juce::String numStr = parameterID.substring(7);  // Extract number after "input_f"
        int drawbarIndex = numStr.getIntValue() - 1;     // Convert to 0-based index
        if (drawbarIndex >= 0 && drawbarIndex < 10) {
            fValueLabels[drawbarIndex].setText(juce::String(newValue, 1), juce::dontSendNotification);
            return;
        }
    }
}

void PLANETMainGui::refreshAllGUIValues()
{
    // Refresh amplitude envelope values
    const char* ampParams[] = { "ampEnvAttackTime", "ampEnvDecayTime", "ampEnvSustainLevel", "ampEnvReleaseTime" };
    for (int i = 0; i < 4; ++i) {
        if (auto* param = apvts.getParameter(ampParams[i])) {
            float value = param->convertFrom0to1(param->getValue());
            ampAdsrValues[i] = value;
            ampAdsrValueEditors[i].setText(juce::String(value, 2), juce::dontSendNotification);
        }
    }

    // Refresh amplitude zone values
    if (auto* param = apvts.getParameter("exponentialControl"))
        envCurveValue.setText(juce::String(param->convertFrom0to1(param->getValue()), 2), juce::dontSendNotification);

    if (auto* param = apvts.getParameter("velToAmplitude"))
        velAmpValue.setText(juce::String(param->convertFrom0to1(param->getValue()), 2), juce::dontSendNotification);
    if (auto* param = apvts.getParameter("velToAttackTime"))
        velAttackValue.setText(juce::String(param->convertFrom0to1(param->getValue()), 2), juce::dontSendNotification);
    if (auto* param = apvts.getParameter("vintageAmount"))
        vintageValue.setText(juce::String(param->convertFrom0to1(param->getValue()), 2), juce::dontSendNotification);

    // Refresh harmonic envelope values for currently selected drawbar
    bindToSelectedDrawbar();

    // Refresh F multiplier values for all drawbars
    for (int i = 0; i < 10; ++i) {
        juce::String paramID = "input_f" + juce::String(i + 1);
        if (auto* param = apvts.getParameter(paramID)) {
            float value = param->convertFrom0to1(param->getValue());
            fValueLabels[i].setText(juce::String(value, 1), juce::dontSendNotification);
        }
    }

    // Trigger a full repaint
    repaint();
}

//==============================================================================
// PATCH MANAGEMENT METHODS
//==============================================================================

void PLANETMainGui::loadPatchButtonClicked()
{
    if (audioProcessor == nullptr)
        return;

    auto* processor = dynamic_cast<PLANETtest4AudioProcessor*>(audioProcessor);
    if (processor == nullptr)
        return;

    auto chooser = std::make_shared<juce::FileChooser>("Load Patch",
                                                         PLANETPatchManager::getDefaultPatchDirectory(),
                                                         "*.md");

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync(flags, [this, processor, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            processor->loadPatch(file);

            // Update patch name display
            currentPatchName = file.getFileNameWithoutExtension();
            updatePatchNameDisplay(currentPatchName);
        }
    });
}

void PLANETMainGui::savePatchButtonClicked()
{
    if (audioProcessor == nullptr)
        return;

    auto* processor = dynamic_cast<PLANETtest4AudioProcessor*>(audioProcessor);
    if (processor == nullptr)
        return;

    // Create a simple input dialog for patch metadata
    auto* window = new juce::AlertWindow("Save Patch", "Enter patch information:", juce::AlertWindow::NoIcon);

    window->addTextEditor("patchName", currentPatchName, "Patch Name:");
    window->addTextEditor("description", "", "Description:");
    window->addTextEditor("tags", "", "Tags (comma-separated):");

    juce::StringArray categories = { "Pads", "Plucks", "Leads", "Bass", "Keys", "FX", "User" };
    window->addComboBox("category", categories, "Category:");

    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    window->enterModalState(true, juce::ModalCallbackFunction::create([this, processor, window, categories](int result)
    {
        if (result == 1)
        {
            juce::String patchName = window->getTextEditorContents("patchName");
            juce::String description = window->getTextEditorContents("description");
            juce::String tags = window->getTextEditorContents("tags");
            int categoryIndex = window->getComboBoxComponent("category")->getSelectedItemIndex();
            juce::String category = categories[categoryIndex >= 0 ? categoryIndex : 6]; // Default to "User"

            if (patchName.isNotEmpty())
            {
                // Create directory if needed
                auto patchDir = PLANETPatchManager::getDefaultPatchDirectory().getChildFile(category);
                if (!patchDir.exists())
                    patchDir.createDirectory();

                juce::File patchFile = patchDir.getChildFile(patchName + ".md");

                // Save the patch
                processor->savePatch(patchFile, patchName, description, tags, category);

                // Update patch name display
                currentPatchName = patchName;

                // Store in processor for state save
                processor->currentPatchName = patchName;
                updatePatchNameDisplay(currentPatchName);
                processor->currentPatchDescription = description;
                updatePatchCommentDisplay(description);
            }
        }
        delete window;
    }), true);
}

void PLANETMainGui::updatePatchNameDisplay(const juce::String& name)
{
    currentPatchName = name;
    currentPatchLabel.setText(name, juce::dontSendNotification);
    repaint();
}

void PLANETMainGui::updatePatchCommentDisplay(const juce::String& comment)
{
    // Truncate for display (full comment preserved in patch file)
    const int maxDisplayLength = 120;
    if (comment.length() > maxDisplayLength)
        patchCommentLabel.setText(comment.substring(0, maxDisplayLength) + "...", juce::dontSendNotification);
    else
        patchCommentLabel.setText(comment, juce::dontSendNotification);
}
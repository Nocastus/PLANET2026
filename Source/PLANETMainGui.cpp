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
    std::atomic<bool>* waveformActivePtr,
    std::atomic<double>* bpmPtr,
    std::atomic<bool>* transportPlayingPtr,
    std::atomic<float>* effectiveBrilliancePtr,
    std::atomic<float>* effectiveCarrierMorphPtr)
    : apvts(apvtsRef), audioProcessor(processor), rawModWheelValue(rawModWheelPtr), modWheelEngaged(modWheelEngagedPtr)
{
    dawBpm = bpmPtr;
    transportPlaying = transportPlayingPtr;
    effectiveBrillianceValue = effectiveBrilliancePtr;
    effectiveCarrierMorphValue = effectiveCarrierMorphPtr;
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

    // Star-knob accent colours: global controls read steel grey; per-drawbar controls
    // take the selected drawbar's colour (refreshed in bindToSelectedDrawbar()).
    ishtarLookAndFeel.starColour = globalAccent;
    drawbarIshtarLookAndFeel.starColour = drawbarColours[selectedDrawbar];

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

        // Cache the routing-switch param atomics for live read while drawing the switches.
        toPMParamPtr[i]  = apvts.getRawParameterValue("k" + juce::String(i + 1) + "ToPM");
        toOutParamPtr[i] = apvts.getRawParameterValue("k" + juce::String(i + 1) + "ToOut");
        trigSingleParamPtr[i] = apvts.getRawParameterValue("k" + juce::String(i + 1) + "TrigSingle");  // F10 Perc switch
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
    // Keyboard-focus suppression now handled by FocuslessComboBox (see PLANETMainGui.h)
    addAndMakeVisible(lfoShapeCombo);

    lfoShapeLabel.setText("Shape", juce::dontSendNotification);
    lfoShapeLabel.setJustificationType(juce::Justification::centredRight);
    lfoShapeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(lfoShapeLabel);

    // Set up LFO sync dropdown (Free / Sync)
    lfoSyncCombo.addItem("Free", 1);
    lfoSyncCombo.addItem("Sync", 2);
    lfoSyncCombo.setSelectedId(1);
    lfoSyncCombo.onChange = [this]() {
        bool syncOn = (lfoSyncCombo.getSelectedId() == 2);
        if (syncOn != currentSyncMode) {
            currentSyncMode = syncOn;
            updateLfoSyncMode();
        }
    };
    // Keyboard-focus suppression now handled by FocuslessComboBox (see PLANETMainGui.h)
    addAndMakeVisible(lfoSyncCombo);

    lfoSyncLabel.setText("Tempo", juce::dontSendNotification);
    lfoSyncLabel.setJustificationType(juce::Justification::centredRight);
    lfoSyncLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(lfoSyncLabel);

    // Set up LFO speed knob
    lfoSpeedKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lfoSpeedKnob.setRange(0.05, 20.0, 0.01);
    lfoSpeedKnob.setValue(4.0);
    lfoSpeedKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 0, 0);
    addAndMakeVisible(lfoSpeedKnob);
    lfoSpeedKnob.setLookAndFeel(&drawbarIshtarLookAndFeel);  // per-drawbar accent

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
    lfoDepthKnob.setLookAndFeel(&drawbarIshtarLookAndFeel);  // per-drawbar accent

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
    velToDrawbarKnob.setLookAndFeel(&drawbarIshtarLookAndFeel);  // per-drawbar accent

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
    velAmpSlider.setColour(juce::Slider::thumbColourId, globalAccent);  // global control accent
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

    // ======================== LIFE knob (clone of Vintage idiom) ========================
    lifeKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lifeKnob.setRange(0.0, 100.0, 1.0);
    lifeKnob.setValue(0.0);
    lifeKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    lifeKnob.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(lifeKnob);
    lifeKnob.setLookAndFeel(&ishtarLookAndFeel);
    lifeLabel.setText("Life", juce::dontSendNotification);
    lifeLabel.setJustificationType(juce::Justification::centred);
    lifeLabel.setColour(juce::Label::textColourId, juce::Colours::white);  // match the white control legending
    addAndMakeVisible(lifeLabel);
    lifeValue.setText("0", juce::dontSendNotification);
    lifeValue.setJustificationType(juce::Justification::centred);
    lifeValue.setColour(juce::Label::textColourId, juce::Colours::white);
    lifeValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    lifeValue.setEditable(true);
    addAndMakeVisible(lifeValue);

    // ======================== LIFE SEED readout + re-cast button ========================
    seedLabel.setText("SEED", juce::dontSendNotification);
    seedLabel.setJustificationType(juce::Justification::centredRight);
    seedLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7e84a8));
    addAndMakeVisible(seedLabel);

    seedValue.setJustificationType(juce::Justification::centred);
    seedValue.setColour(juce::Label::textColourId, juce::Colour(0xffe8ecff));
    seedValue.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    seedValue.setEditable(true);
    addAndMakeVisible(seedValue);
    // Typing a seed recalls a specific "instrument".
    seedValue.onTextChange = [this]
    {
        if (auto* p = apvts.getParameter("lifeSeed"))
        {
            int v = juce::jlimit(0, 9999, seedValue.getText().getIntValue());
            p->setValueNotifyingHost(p->convertTo0to1((float)v));
        }
    };

    // Re-cast button: an 8-pointed Star of Ishtar. Click = draw a new luthier seed.
    {
        juce::Path star;
        const float outer = 1.0f, inner = 0.42f;
        for (int k = 0; k < 16; ++k)
        {
            const float r = (k % 2 == 0) ? outer : inner;
            const float a = juce::MathConstants<float>::pi * k / 8.0f;
            const float px = std::sin(a) * r, py = -std::cos(a) * r;
            if (k == 0) star.startNewSubPath(px, py);
            else        star.lineTo(px, py);
        }
        star.closeSubPath();
        rerollButton.setShape(star, false, true, false);
    }
    // Plain click = re-cast the seed; shift-click = toggle the dev voicing panel.
    rerollButton.onClick = [this]
    {
        if (juce::ModifierKeys::getCurrentModifiers().isShiftDown())
            toggleVoicingPanel();
        else
            rerollSeed();
    };
    addAndMakeVisible(rerollButton);

    // ======================== LIFE VOICING dev panel ========================
    {
        // Sliders 0/1 are the ridge controls: Intensity = ceiling, Ratio = response/ceiling.
        // The engine still reads splitCeiling + response, which we recompute from these two.
        const char* vNames[NUM_VOICING_SLIDERS] = { "Intensity", "Ratio", "Tilt", "Beat depth", "Strike spread" };
        const double vMins[NUM_VOICING_SLIDERS] = { 0.5,  0.05,  -3.0, 0.0, 0.0 };
        const double vMaxs[NUM_VOICING_SLIDERS] = { 24.0, 0.40,   3.0, 1.0, 1.0 };
        const double vSteps[NUM_VOICING_SLIDERS] = { 0.1, 0.0025, 0.01, 0.01, 0.01 };
        const double vDefs[NUM_VOICING_SLIDERS] = {
            LifeVoicingParams::kDefaultSplitCeiling, LifeVoicingParams::kDefaultRatio,
            LifeVoicingParams::kDefaultTilt, LifeVoicingParams::kDefaultBeatDepth,
            LifeVoicingParams::kDefaultStrikeSpread };

        for (int i = 0; i < NUM_VOICING_SLIDERS; ++i)
        {
            auto& s = voicingSliders[i];
            s.setSliderStyle(juce::Slider::LinearHorizontal);
            s.setRange(vMins[i], vMaxs[i], vSteps[i]);
            s.setValue(vDefs[i], juce::dontSendNotification);
            s.setDoubleClickReturnValue(true, vDefs[i]);
            s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 18);
            s.onValueChange = [this, i]
            {
                if (lifeVoicingParams == nullptr) return;
                const float v = (float)voicingSliders[i].getValue();
                switch (i)
                {
                    // Intensity (ceiling) and Ratio both drive response = ratio * ceiling.
                    case 0:
                    case 1:
                    {
                        const float ceiling = (float)voicingSliders[0].getValue();
                        const float ratio   = (float)voicingSliders[1].getValue();
                        lifeVoicingParams->splitCeiling.store(ceiling);
                        lifeVoicingParams->response.store(juce::jlimit(0.1f, 8.0f, ratio * ceiling));
                        break;
                    }
                    case 2: lifeVoicingParams->tilt.store(v);         break;
                    case 3: lifeVoicingParams->beatDepth.store(v);    break;
                    case 4: lifeVoicingParams->strikeSpread.store(v); break;
                }
            };
            voicingPanel.addAndMakeVisible(s);

            auto& l = voicingSliderLabels[i];
            l.setText(vNames[i], juce::dontSendNotification);
            l.setColour(juce::Label::textColourId, juce::Colours::orange);
            l.setFont(juce::Font(13.0f));
            voicingPanel.addAndMakeVisible(l);

            l.setBounds(10, 28 + i * 34, 100, 20);
            s.setBounds(112, 28 + i * 34, 208, 22);
        }

        voicingSnapshotButton.onClick = [this] { saveVoicingSnapshot(); };
        voicingPanel.addAndMakeVisible(voicingSnapshotButton);
        voicingSnapshotButton.setBounds(10, 28 + NUM_VOICING_SLIDERS * 34, 90, 24);

        // Close (x) in the top-right corner - panel is 330px wide (see resized()).
        voicingCloseButton.setColour(juce::TextButton::textColourOffId, juce::Colours::orange);
        voicingCloseButton.onClick = [this] { voicingPanel.setVisible(false); };
        voicingPanel.addAndMakeVisible(voicingCloseButton);
        voicingCloseButton.setBounds(330 - 26, 4, 22, 22);

        voicingSavedLabel.setColour(juce::Label::textColourId, juce::Colours::orange.withAlpha(0.8f));
        voicingSavedLabel.setFont(juce::Font(12.0f));
        voicingPanel.addAndMakeVisible(voicingSavedLabel);
        voicingSavedLabel.setBounds(106, 28 + NUM_VOICING_SLIDERS * 34, 214, 24);

        addChildComponent(voicingPanel);   // added invisible; shift-click the star to show
    }

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

    // Portamento (F2a): glide-time knob + a Rate/Time mode toggle. 0 s = off. The knob shares the
    // pitch zone with Distance/Time; the toggle is custom-painted (see paint()) to match the
    // Colour-zone mod-wheel buttons it sits above.
    setupKnob(portamentoTimeKnob, portamentoLabel, "Porta", 0.0, 5.0, 0.0);
    
    // Brilliance horizontal slider
    brillianceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    brillianceSlider.setRange(0.0, 1.0, 0.01);
    brillianceSlider.setValue(0.5);
    brillianceSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    brillianceSlider.setDoubleClickReturnValue(true, 0.5);
    brillianceSlider.setColour(juce::Slider::thumbColourId, globalAccent);  // global control accent
    addAndMakeVisible(brillianceSlider);
    brillianceMainLabel.setText("Brilliance", juce::dontSendNotification);
    brillianceMainLabel.setJustificationType(juce::Justification::centred);
    brillianceMainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(brillianceMainLabel);

    // Density horizontal slider (F5 carrier morph) - sits below Brilliance in the Colour zone
    densitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    densitySlider.setRange(0.0, 1.0, 0.01);
    densitySlider.setValue(0.0);
    densitySlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    densitySlider.setDoubleClickReturnValue(true, 0.0);
    densitySlider.setColour(juce::Slider::thumbColourId, globalAccent);  // global control accent
    addAndMakeVisible(densitySlider);

    // Labels above each slider, left-aligned, styled like the Pitch Envelope knob labels
    // (Distance / Time): white, amarnaSemiBold 18px.
    brillianceSubLabel.setText("Brilliance", juce::dontSendNotification);
    brillianceSubLabel.setJustificationType(juce::Justification::centredLeft);
    brillianceSubLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    brillianceSubLabel.setFont(amarnaSemiBold.withHeight(18.0f));
    addAndMakeVisible(brillianceSubLabel);
    densitySubLabel.setText("Density", juce::dontSendNotification);
    densitySubLabel.setJustificationType(juce::Justification::centredLeft);
    densitySubLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    densitySubLabel.setFont(amarnaSemiBold.withHeight(18.0f));
    addAndMakeVisible(densitySubLabel);
    densityValue.setJustificationType(juce::Justification::centredRight);
    densityValue.setColour(juce::Label::textColourId, juce::Colours::white);
    densityValue.setFont(amarnaRegular.withHeight(13.0f));
    densityValue.setText("0.00", juce::dontSendNotification);
    addChildComponent(densityValue);   // present but hidden, matching brillianceValue's minimal style

    // Cache the per-slider mod-wheel mode params for the Colour-zone buttons.
    brillianceMWParam = apvts.getRawParameterValue("brillianceModWheel");
    densityMWParam    = apvts.getRawParameterValue("carrierMorphModWheel");

    // Effects vertical sliders
    auto setupVerticalSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& name,
                                      double min, double max, double defaultVal) {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setRange(min, max, 0.01);
        slider.setValue(defaultVal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setDoubleClickReturnValue(true, defaultVal);
        slider.setColour(juce::Slider::thumbColourId, globalAccent);  // global control accent
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
    setupValueLabel(portamentoValue, "0.00");

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
        repaint(brillianceSliderBounds.expanded(5));   // keep the diff bar's thumb-end in sync
        };
    densitySlider.onValueChange = [this]() {
        densityValue.setText(juce::String(densitySlider.getValue(), 2), juce::dontSendNotification);
        repaint(densitySliderBounds.expanded(5));
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
    portamentoTimeKnob.onValueChange = [this]() {
        portamentoValue.setText(juce::String(portamentoTimeKnob.getValue(), 2), juce::dontSendNotification);
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
    portamentoTimeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "portamentoTime", portamentoTimeKnob);
    // portamentoMode is a custom-painted toggle (no attachment): paint() reads the param, mouseDown()
    // flips it via the host. Cache the pointer for paint().
    portamentoModeParam = apvts.getRawParameterValue("portamentoMode");
    brillianceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "brilliance", brillianceSlider);
    carrierMorphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "carrierMorph", densitySlider);
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
    lifeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "lifeAmount", lifeKnob);
    
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
    apvts.addParameterListener("lifeAmount", this);
    apvts.addParameterListener("lifeSeed", this);
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
    loadPatchButton.setWantsKeyboardFocus(false);  // don't hold keyboard focus from the host (DAW transport keys)
    addAndMakeVisible(loadPatchButton);

    savePatchButton.setButtonText("Save");
    savePatchButton.onClick = [this] { savePatchButtonClicked(); };
    savePatchButton.setWantsKeyboardFocus(false);  // don't hold keyboard focus from the host (DAW transport keys)
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
    styleLabel(lifeLabel);
    styleLabel(lifeValue, true);
    styleLabel(seedValue, true);

    // Right column labels
    styleLabel(vibratoRateLabel);
    styleLabel(vibratoDepthLabel);
    styleLabel(vibratoFadeLabel);
    styleLabel(vibratoRateValue, true);
    styleLabel(vibratoDepthValue, true);
    styleLabel(vibratoFadeValue, true);
    styleLabel(pitchDistLabel);
    styleLabel(pitchTimeLabel);
    styleLabel(portamentoLabel);
    styleLabel(pitchDistValue, true);
    styleLabel(pitchTimeValue, true);
    styleLabel(portamentoValue, true);
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

    // ---- DAW keyboard pass-through ----
    // When any editable value field finishes editing, release keyboard focus so the host
    // (Cubase) regains transport keys (e.g. keypad Enter for play). Applied to every Label
    // child, so it covers all current and future editable value fields. Harmless on
    // non-editable labels (onEditorHide never fires for them).
    for (auto* child : getChildren())
        if (auto* lbl = dynamic_cast<juce::Label*>(child))
            lbl->onEditorHide = [lbl] { lbl->giveAwayKeyboardFocus(); };

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
    portamentoTimeKnob.setLookAndFeel(nullptr);
    lfoSpeedKnob.setLookAndFeel(nullptr);
    lfoDepthKnob.setLookAndFeel(nullptr);
    velToDrawbarKnob.setLookAndFeel(nullptr);
    
    velAttackKnob.setLookAndFeel(nullptr);
    envCurveKnob.setLookAndFeel(nullptr);
    vintageKnob.setLookAndFeel(nullptr);
    lifeKnob.setLookAndFeel(nullptr);

    apvts.removeParameterListener("ampEnvAttackTime", this);
    apvts.removeParameterListener("ampEnvDecayTime", this);
    apvts.removeParameterListener("ampEnvSustainLevel", this);
    apvts.removeParameterListener("ampEnvReleaseTime", this);
    apvts.removeParameterListener("exponentialControl", this);
  
    apvts.removeParameterListener("velToAmplitude", this);
    apvts.removeParameterListener("velToAttackTime", this);
    apvts.removeParameterListener("vintageAmount", this);
    apvts.removeParameterListener("lifeAmount", this);
    apvts.removeParameterListener("lifeSeed", this);
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
    // Repaint the Colour-zone diff indicators when the published effective values move (so they
    // track the wheel, and settle/freeze exactly where the sound is).
    if (effectiveBrillianceValue != nullptr)
    {
        float e = effectiveBrillianceValue->load();
        if (std::abs(e - cachedEffectiveBrilliance) > 0.001f)
        {
            cachedEffectiveBrilliance = e;
            repaint(brillianceSliderBounds.expanded(5));
        }
    }
    if (effectiveCarrierMorphValue != nullptr)
    {
        float e = effectiveCarrierMorphValue->load();
        if (std::abs(e - cachedEffectiveCarrierMorph) > 0.001f)
        {
            cachedEffectiveCarrierMorph = e;
            repaint(densitySliderBounds.expanded(5));
        }
    }

    // Decay the lightning-bolt flash after a re-cast (the "spark of life").
    if (boltFlash > 0.0f)
    {
        boltFlash *= 0.78f;
        if (boltFlash < 0.01f) boltFlash = 0.0f;
        if (!lifeKnobBounds.isEmpty() && !seedModuleBounds.isEmpty())
            repaint(lifeKnobBounds.getUnion(seedModuleBounds).expanded(12));
    }

    updateLfoPulses();
    updateDrawbarColors();
}

void PLANETMainGui::rerollSeed()
{
    if (auto* p = apvts.getParameter("lifeSeed"))
    {
        // Draw a fresh "luthier seed" and let it save with the patch.
        int newSeed = juce::Random::getSystemRandom().nextInt({ 0, 10000 });
        p->setValueNotifyingHost(p->convertTo0to1((float)newSeed));
    }
    boltFlash = 1.0f;   // fire the spark
}

void PLANETMainGui::setLifeVoicingParams(LifeVoicingParams* p)
{
    lifeVoicingParams = p;
    if (p == nullptr) return;
    // The processor's atomics outlive the editor: when the window reopens, show the
    // values that are actually in force, not the slider defaults.
    const float ceiling = p->splitCeiling.load();
    const float ratio   = ceiling > 0.0f ? p->response.load() / ceiling : LifeVoicingParams::kDefaultRatio;
    voicingSliders[0].setValue(ceiling, juce::dontSendNotification);  // Intensity
    voicingSliders[1].setValue(ratio,   juce::dontSendNotification);  // Ratio = response/ceiling
    voicingSliders[2].setValue(p->tilt.load(),         juce::dontSendNotification);
    voicingSliders[3].setValue(p->beatDepth.load(),    juce::dontSendNotification);
    voicingSliders[4].setValue(p->strikeSpread.load(), juce::dontSendNotification);
}

void PLANETMainGui::toggleVoicingPanel()
{
    voicingPanel.setVisible(!voicingPanel.isVisible());
    if (voicingPanel.isVisible())
        voicingPanel.toFront(false);
}

void PLANETMainGui::saveVoicingSnapshot()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("PLANET2026").getChildFile("VoicingSnapshots");
    dir.createDirectory();
    auto now = juce::Time::getCurrentTime();
    auto file = dir.getChildFile("voicing_" + now.formatted("%Y%m%d_%H%M%S") + ".txt");

    // Capture the patch context alongside the voicing constants - a voicing only
    // means something relative to the patch and LIFE/seed it was judged against.
    juce::String s;
    s << "ISHTAR LIFE voicing snapshot  " << now.toString(true, true) << "\n";
    s << "patch = " << currentPatchLabel.getText() << "\n\n";
    for (int i = 0; i < NUM_VOICING_SLIDERS; ++i)
        s << voicingSliderLabels[i].getText() << " = "
          << juce::String(voicingSliders[i].getValue(), 4) << "\n";
    // Intensity/Ratio are the ridge controls; record the derived response = ratio * ceiling.
    s << "(derived response = "
      << juce::String(voicingSliders[0].getValue() * voicingSliders[1].getValue(), 3) << ")\n";
    s << "\n";
    for (auto* id : { "lifeAmount", "lifeSeed", "brilliance" })
        if (auto* p = apvts.getParameter(id))
            s << id << " = " << juce::String(p->convertFrom0to1(p->getValue()), 3) << "\n";
    s << "\nnotes: \n";

    if (file.replaceWithText(s))
        voicingSavedLabel.setText("saved: " + file.getFileName(), juce::dontSendNotification);
    else
        voicingSavedLabel.setText("SAVE FAILED", juce::dontSendNotification);
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

        // F10: the Perc switch is shown only while the envelope is active and is drawn in
        // paintOverChildren (parent), which the timer doesn't otherwise repaint. When the
        // active-state flips, repaint this column's strip so the switch appears/disappears live.
        if (hasActiveEnvelope != prevEnvelopeActive[i])
        {
            prevEnvelopeActive[i] = hasActiveEnvelope;
            if (!drawbarColumnBounds[i].isEmpty())
                repaint(drawbarColumnBounds[i]);
        }

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
// LFO-RATE "PING" PULSE INDICATORS (item #5)
// Advances a per-drawbar phase at each drawbar's effective LFO rate and pushes a
// brightness value into the slider properties: "lfoPulse" on every drawbar (its ring),
// and on the LFO-speed knob for the selected drawbar (its inner circle). The pulse is a
// hard onset (bright) at phase 0 that decays exponentially across the cycle. Tempo-synced
// LFOs pulse at the tempo-derived rate while the transport runs; with no tempo (stopped or
// bpm<=0) they show solid on. Driven off LFO config, so it shows without a note playing.
//==============================================================================
void PLANETMainGui::updateLfoPulses()
{
    // Elapsed time since last advance (real dt, robust to timer jitter). First call: dt=0.
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    double dt = (lfoPulseLastMs > 0.0) ? (nowMs - lfoPulseLastMs) / 1000.0 : 0.0;
    lfoPulseLastMs = nowMs;
    dt = juce::jlimit(0.0, 0.1, dt);   // clamp so a stalled/backgrounded editor can't jump the phase

    const double bpm     = (dawBpm != nullptr) ? dawBpm->load() : 0.0;
    const bool   playing = (transportPlaying != nullptr) && transportPlaying->load();

    for (int i = 0; i < 10; ++i)
    {
        const juce::String n = juce::String(i + 1);
        auto* amtPtr  = apvts.getRawParameterValue("k" + n + "LFOAmount");
        auto* syncPtr = apvts.getRawParameterValue("k" + n + "LFOSync");
        auto* ratePtr = apvts.getRawParameterValue("k" + n + "LFORate");
        auto* divPtr  = apvts.getRawParameterValue("k" + n + "LFOSyncDiv");

        const bool active = (amtPtr != nullptr) && std::abs(amtPtr->load()) > 0.001f;
        const bool synced = (syncPtr != nullptr) && syncPtr->load() > 0.5f;

        // Resolve the effective rate (Hz) and whether the pulse animates.
        bool  solidOn = false;
        float hz = 0.0f;
        if (synced)
        {
            if (playing && bpm > 0.0)
                hz = syncDivisionToHz(divPtr != nullptr ? divPtr->load() : 0.0f, bpm);
            else
                solidOn = true;                       // synced but no tempo -> constant on
        }
        else
        {
            hz = (ratePtr != nullptr) ? ratePtr->load() : 0.0f;   // free-running Hz
        }

        // Advance phase (capped so very fast LFOs flutter rather than strobe).
        if (!solidOn && active)
        {
            const float visHz = juce::jmin(hz, LFO_PULSE_MAX_HZ);
            lfoPulsePhase[i] += (double)visHz * dt;
            lfoPulsePhase[i] -= std::floor(lfoPulsePhase[i]);   // wrap to [0,1)
        }

        // Phase -> brightness: hard onset (ping=1) at phase 0, exponential decay across the cycle.
        // Ring and knob share the ping shape but use different floors (the knob's is lower so its
        // subtler circle shows the ping clearly). solidOn / inactive -> steady full brightness.
        float ringBrightness = 1.0f;
        float knobBrightness = 1.0f;
        if (active && !solidOn)
        {
            const float ping = std::exp(-LFO_PULSE_DECAY * (float)lfoPulsePhase[i]);
            ringBrightness = LFO_PULSE_FLOOR      + (1.0f - LFO_PULSE_FLOOR)      * ping;
            knobBrightness = LFO_PULSE_KNOB_FLOOR + (1.0f - LFO_PULSE_KNOB_FLOOR) * ping;
        }

        drawbarSliders[i].getProperties().set("lfoPulse", ringBrightness);
        drawbarSliders[i].getProperties().set("lfoSynced", synced);   // colours the ring: synced vs free-running

        // Indicator 2: a pulsing dot in the LFO-speed knob's central hole, tracking the selected
        // drawbar's LFO — absent when its LFO is off, white free / amber synced (drawn in IshtarLookAndFeel).
        if (i == selectedDrawbar)
        {
            auto& kp = lfoSpeedKnob.getProperties();
            kp.set("lfoActive", active);
            kp.set("lfoSynced", synced);
            kp.set("lfoPulse", knobBrightness);
            lfoSpeedKnob.repaint();
        }
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
    
    // Drawbar section — same tinted base as the Drawbar-Envelope zone below, so the two
    // per-drawbar zones read as one connected unit.
    g.setColour(drawbarColours[selectedDrawbar].withAlpha(0.3f));
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

    // Draw Harmonic ADSR envelope — geometry comes from resized() via harmonicEnvBounds,
    // so the graph, its drag handles and the value fields can never drift apart.
    {
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(harmonicEnvBounds.toFloat(), 5.0f);

        drawEnvelopeCurve(g, harmonicEnvBounds,
                          adsrValues[selectedDrawbar][0], adsrValues[selectedDrawbar][1],
                          adsrValues[selectedDrawbar][2], adsrValues[selectedDrawbar][3],
                          (float)envCurveKnob.getValue(),
                          drawbarColours[selectedDrawbar], drawbarColours[selectedDrawbar]);
    }
    
    // Amplitude section
    g.setColour(backgroundGlobal);
    g.fillRect(0, drawbarSectionHeight + harmonicHeight, leftWidth, ampHeight);

    // Draw Amplitude ADSR envelope — geometry from resized() via ampEnvBounds (see above).
    {
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(ampEnvBounds.toFloat(), 5.0f);

        drawEnvelopeCurve(g, ampEnvBounds,
                          ampAdsrValues[0], ampAdsrValues[1], ampAdsrValues[2], ampAdsrValues[3],
                          (float)envCurveKnob.getValue(),
                          juce::Colours::white, globalAccent);
    }

    // ======================== LIFE: spark-of-life lightning bolt ========================
    // A bolt arcs from the Life knob down into the SEED readout - the seed is what the
    // spark "creates". It flashes brighter for a moment on each re-cast (boltFlash).
    if (!lifeKnobBounds.isEmpty() && !seedModuleBounds.isEmpty())
    {
        juce::Point<float> S((float)lifeKnobBounds.getX() - 3.0f,
                             (float)lifeKnobBounds.getBottom() - 16.0f);
        juce::Point<float> E((float)seedModuleBounds.getCentreX(),
                             (float)seedModuleBounds.getY() - 1.0f);

        juce::Point<float> d = E - S;
        juce::Point<float> perp(-d.y, d.x);
        if (auto plen = perp.getDistanceFromOrigin(); plen > 0.0f) perp /= plen;
        const float J = 7.0f;   // zigzag notch amplitude

        juce::Path bolt;
        bolt.startNewSubPath(S);
        bolt.lineTo(S + d * 0.28f + perp * ( J));
        bolt.lineTo(S + d * 0.46f + perp * (-J));
        bolt.lineTo(S + d * 0.72f + perp * ( J * 0.6f));
        bolt.lineTo(E);

        // faint wide underlay (fake glow), then a bright thin core
        g.setColour(accentColour.withAlpha(0.16f + 0.40f * boltFlash));
        g.strokePath(bolt, juce::PathStrokeType(5.0f + 4.0f * boltFlash,
                     juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xffcfe6ff).withAlpha(0.55f + 0.45f * boltFlash));
        g.strokePath(bolt, juce::PathStrokeType(2.2f,
                     juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
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

    const int zoneLabelH = 24;     // must match resized(): zone contents are shifted down by this
    int labelHeight = 20;
    int labelLeftPad = 12;         // left inset for the top-left zone labels
    int labelTopPad = 3;
    int labelW = 280;

    int rightX = leftWidth;
    int rightContentHeight = mainHeight - drawbarSectionHeight;
    int rightSectionHeight = rightContentHeight / 4;
    int rightSectionY = drawbarSectionHeight;

    // Top-row labels (DRAWBARS, WAVEFORM) — centred in the margin above the columns/waveform
    g.drawText("DRAWBARS", labelLeftPad, 0, labelW, drawbarMargin, juce::Justification::centredLeft);
    g.drawText("WAVEFORM", rightX + labelLeftPad, 0, labelW, drawbarMargin, juce::Justification::centredLeft);

    // Drawbar-envelope header: names the selected drawbar in its own highlight colour
    // (replaces the old faint in-graph watermark).
    g.setColour(drawbarColours[selectedDrawbar]);
    g.drawText("DRAWBAR " + juce::String(selectedDrawbar + 1) + " ENVELOPE",
        labelLeftPad, drawbarSectionHeight + labelTopPad, labelW, labelHeight, juce::Justification::left);
    g.setColour(juce::Colours::white.withAlpha(0.7f));

    g.drawText("AMPLITUDE ENVELOPE", labelLeftPad, drawbarSectionHeight + harmonicHeight + labelTopPad, labelW, labelHeight, juce::Justification::left);

    // Right column labels — top-left of each zone
    g.drawText("VIBRATO", rightX + labelLeftPad, rightSectionY + labelTopPad, labelW, labelHeight, juce::Justification::left);
    g.drawText("PITCH ENVELOPE", rightX + labelLeftPad, rightSectionY + rightSectionHeight + labelTopPad, labelW, labelHeight, juce::Justification::left);
    g.drawText("COLOUR", rightX + labelLeftPad, rightSectionY + rightSectionHeight * 2 + labelTopPad, labelW, labelHeight, juce::Justification::left);
    g.drawText("EFFECTS", rightX + labelLeftPad, rightSectionY + rightSectionHeight * 3 + labelTopPad - 8, labelW, labelHeight, juce::Justification::left);

    // Colour-zone per-slider "Mod wheel" buttons. Two zones: left "MW" half toggles Off<->On;
    // right half is a big polarity triangle (up = Normal / wheel-up-raises, down = Inverse). Off =
    // greyed; the triangle still shows the remembered polarity (dim) so you can pre-set it.
    auto drawMWButton = [&](juce::Rectangle<int> b, int mode, int lastPol)
    {
        if (b.isEmpty()) return;
        auto rf = b.toFloat();
        bool on = (mode != 0);
        int pol = on ? mode : lastPol;   // triangle shows live polarity, or the remembered one when Off

        if (on) { g.setColour(globalAccent); g.fillRoundedRectangle(rf, 3.0f); }
        else    { g.setColour(juce::Colour(0xff202020)); g.fillRoundedRectangle(rf, 3.0f);
                  g.setColour(juce::Colour(0xff555555)); g.drawRoundedRectangle(rf, 3.0f, 1.0f); }

        int splitX = b.getCentreX();
        g.setColour((on ? juce::Colours::black : juce::Colour(0xff555555)).withAlpha(0.4f));
        g.drawLine((float)splitX, rf.getY() + 3.0f, (float)splitX, rf.getBottom() - 3.0f, 1.0f);

        // "MW" (left zone)
        g.setColour(on ? juce::Colours::black.withAlpha(0.85f) : juce::Colour(0xff8a8a8a));
        g.setFont(11.0f);
        g.drawText("MW", juce::Rectangle<int>(b.getX(), b.getY(), splitX - b.getX(), b.getHeight()),
                   juce::Justification::centred);

        // Big polarity triangle (right zone)
        float cx = (float)(splitX + b.getRight()) * 0.5f, cy = rf.getCentreY();
        float hw = 5.5f, hh = 5.0f;
        juce::Path tri;
        if (pol == 2) tri.addTriangle(cx - hw, cy - hh, cx + hw, cy - hh, cx, cy + hh);   // down = Inverse
        else          tri.addTriangle(cx - hw, cy + hh, cx + hw, cy + hh, cx, cy - hh);   // up = Normal
        g.setColour(on ? juce::Colours::black.withAlpha(0.9f) : juce::Colour(0xff777777));
        g.fillPath(tri);
    };

    int brillMode = brillianceMWParam ? (int)brillianceMWParam->load() : 0;
    int densMode  = densityMWParam    ? (int)densityMWParam->load()    : 0;
    if (brillMode != 0) brillLastPolarity = brillMode;   // keep remembered polarity in sync with the param
    if (densMode  != 0) densLastPolarity  = densMode;
    drawMWButton(brillianceMWButtonBounds, brillMode, brillLastPolarity);
    drawMWButton(densityMWButtonBounds,    densMode,  densLastPolarity);

    // Portamento Rate/Time toggle (F2a) - same visual language as the MW buttons above it:
    // dark/grey rounded rect = off = "Time" (the subtler default), accent fill = on = "Rate".
    if (!portamentoModeButtonBounds.isEmpty())
    {
        auto rf = portamentoModeButtonBounds.toFloat();
        bool rate = portamentoModeParam && portamentoModeParam->load() > 0.5f;
        if (rate) { g.setColour(globalAccent); g.fillRoundedRectangle(rf, 3.0f); }
        else      { g.setColour(juce::Colour(0xff202020)); g.fillRoundedRectangle(rf, 3.0f);
                    g.setColour(juce::Colour(0xff555555)); g.drawRoundedRectangle(rf, 3.0f, 1.0f); }
        g.setColour(rate ? juce::Colours::black.withAlpha(0.85f) : juce::Colour(0xff8a8a8a));
        g.setFont(11.0f);
        g.drawText(rate ? "Rate" : "Time", portamentoModeButtonBounds, juce::Justification::centred);
    }

    // ISHTAR name - aligned with left/right column divider
    g.setColour(accentColour.withAlpha(0.9f));
    g.setFont(amarnaSemiBold.withHeight(30.0f));
    g.drawText("ISHTAR", leftWidth + 10, mainHeight + (patchBarHeight - 22) / 2, 100, 22, juce::Justification::left);

    // Colour-zone mod-wheel diff indicators (Brilliance + Density). Draw the effective (heard) value
    // as a bar from the slider thumb (the null) to the effective position, with a marker. Reads the
    // processor's PUBLISHED effective value, so it matches the sound exactly - including Inverse
    // direction and the Off-latch hold (frozen and persisting when the button is switched Off while
    // off the thumb). Hidden when the effective value sits at the thumb (no diff).
    auto drawColourDiff = [&](juce::Rectangle<int> bounds, float sliderVal, std::atomic<float>* effPtr)
    {
        if (bounds.getWidth() <= 0 || effPtr == nullptr) return;
        float eff = effPtr->load();
        if (std::abs(eff - sliderVal) <= 0.005f) return;   // at null: nothing to show

        int sx = bounds.getX(), sw = bounds.getWidth(), sy = bounds.getY(), sh = bounds.getHeight();
        int baseX = sx + (int)(sliderVal * sw);
        int effX  = sx + (int)(eff * sw);

        g.setColour(globalAccent.withAlpha(0.5f));
        g.fillRect(juce::jmin(baseX, effX), sy + sh / 2 - 3, std::abs(effX - baseX), 6);
        g.setColour(globalAccent);
        g.fillRect(effX - 2, sy + 5, 4, sh - 10);
    };
    drawColourDiff(brillianceSliderBounds, (float)brillianceSlider.getValue(), effectiveBrillianceValue);
    drawColourDiff(densitySliderBounds,    (float)densitySlider.getValue(),    effectiveCarrierMorphValue);
    
    

    // Draw brackets connecting related effect controls
    {
        int rightContentWidth = bounds.getWidth() - leftWidth - 20;
        int effectsSliderSpacing = rightContentWidth / 5;
        int effectsSliderWidth = 55;
        int effectsY = drawbarSectionHeight + rightSectionHeight * 3 + 7 + zoneLabelH;  // follow the shifted sliders
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

void PLANETMainGui::paintOverChildren(juce::Graphics& g)
{
    // Outline the selected drawbar (over the top of the sliders/labels) so it's obvious
    // which drawbar the envelope / LFO / Vel-to-Drawbar controls are addressing. Drawn in
    // that drawbar's accent colour, matching the per-drawbar colour scheme.
    auto r = drawbarColumnBounds[selectedDrawbar].toFloat();
    if (!r.isEmpty())
    {
        g.setColour(drawbarColours[selectedDrawbar]);
        g.drawRoundedRectangle(r, 4.0f, 2.0f);
    }

    // F1: during a copy-drag, outline the drawbar column under the pointer as the drop target
    // (in the source drawbar's colour, so it reads as "copying from N to here").
    if (copyDrag != CopyDragKind::None && copyDragHoverTarget >= 0
        && !drawbarColumnBounds[copyDragHoverTarget].isEmpty())
    {
        auto tr = drawbarColumnBounds[copyDragHoverTarget].toFloat();
        juce::Colour hl = (copyDragSource >= 0) ? drawbarColours[copyDragSource] : juce::Colours::white;
        g.setColour(hl.withAlpha(0.9f));
        g.drawRoundedRectangle(tr, 4.0f, 3.0f);
    }

    // ---- F7/F8 per-drawbar routing switches (console channel-strip paradigm) ----
    // Two circles in a routing strip to the right of each fader, each with a legend label below:
    // top circle -> phase-distortion path ("Shape"), bottom -> direct output / additive ("Direct").
    // On = filled in the drawbar's colour; off = hollow/grey ring. Both off = the bar is muted,
    // shown as a faint wash over the whole column. Circle size echoes the Ishtar-star centre.
    for (int i = 0; i < 10; ++i)
    {
        const bool pmOn  = toPMParamPtr[i]  && toPMParamPtr[i]->load()  >= 0.5f;
        const bool outOn = toOutParamPtr[i] && toOutParamPtr[i]->load() >= 0.5f;

        // Muted (neither destination): dim the whole column so it reads as "off" at a glance.
        if (!pmOn && !outOn && !drawbarColumnBounds[i].isEmpty())
        {
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.fillRoundedRectangle(drawbarColumnBounds[i].toFloat(), 4.0f);
        }

        auto drawSwitch = [&](juce::Rectangle<int> b, bool on, const juce::String& legend)
        {
            auto rf = b.toFloat();
            if (on)
            {
                g.setColour(drawbarColours[i]);
                g.fillEllipse(rf);
            }
            else
            {
                g.setColour(juce::Colour(0xff202020));
                g.fillEllipse(rf);
                g.setColour(juce::Colour(0xff666666));
                g.drawEllipse(rf, 1.2f);
            }

            // Legend label below the circle (brightens when routed, to reinforce the state).
            g.setFont(juce::Font(10.0f));
            g.setColour(on ? juce::Colour(0xffd6d6d6) : juce::Colour(0xff8a8a8a));
            g.drawText(legend, juce::Rectangle<int>(b.getCentreX() - 24, b.getBottom() + 1, 48, 12),
                       juce::Justification::centred);
        };

        drawSwitch(pmSwitchBounds[i],  pmOn,  "Shape");
        drawSwitch(outSwitchBounds[i], outOn, "Direct");

        // F10 "Perc" switch, above the routing pair. Only shown when this drawbar's envelope is
        // active (the red-thumb condition) - single-trigger is meaningless without an envelope.
        // On = Single (fire only on a phrase start); off = Multi (retrigger every note, default).
        if (drawbarEnvelopeActive(i))
        {
            const bool percOn = trigSingleParamPtr[i] && trigSingleParamPtr[i]->load() >= 0.5f;
            drawSwitch(percSwitchBounds[i], percOn, "Perc");
        }
    }
}

void PLANETMainGui::resized()
{
    auto bounds = getLocalBounds();
    int leftWidth = (int)(bounds.getWidth() * leftWidthRatio);
    
    // Height of the top-left zone-label strip. Zone labels moved here from bottom-centre;
    // each zone's contents are translated down by this amount (see paint()).
    const int zoneLabelH = 24;

    // Drawbar section layout — contents sit below the zone-label strip
    int drawbarMargin = 20;
    int fLabelHeight = 25;
    int drawbarWidth = (leftWidth - drawbarMargin * 2) / 10;
    int drawbarTop = zoneLabelH;
    int drawbarHeight = drawbarSectionHeight - drawbarMargin - drawbarTop - fLabelHeight - 5;

    // Console channel-strip paradigm: fader on the left, routing controls in a strip to its right.
    // The fader is narrowed and shifted left to open a dedicated routing strip on the right side
    // of each column, holding the two routing circles + a legend label under each.
    const int routeStripW  = 36;   // width reserved on the right of each column for the routing controls
    const int faderLeftPad = 8;    // fader inset from the column's left edge
    for (int i = 0; i < 10; ++i)
    {
        int x = drawbarMargin + i * drawbarWidth;
        int faderTop = drawbarTop + fLabelHeight + 5;
        int faderW   = drawbarWidth - faderLeftPad - routeStripW - 4;  // leave the strip + a small gap

        fValueLabels[i].setBounds(x + 5, drawbarTop, drawbarWidth - 10, fLabelHeight);
        drawbarSliders[i].setBounds(x + faderLeftPad, faderTop, faderW, drawbarHeight);

        // Full column (F-value label + fader + routing strip) — outlines the selected drawbar
        drawbarColumnBounds[i] = juce::Rectangle<int>(x + 2, drawbarTop - 2,
            drawbarWidth - 4, fLabelHeight + 5 + drawbarHeight + 4);

        // Two routing circles (~Ishtar-star-centre size), each with a legend label below it,
        // stacked vertically in the right strip and centred on the fader travel. Top = phase-
        // distortion path, bottom = direct/additive path. The label rects are derived from these
        // circle bounds in paintOverChildren().
        const int circleD    = 14;
        const int groupH     = circleD + 2 + 12;   // circle + gap + legend label
        const int gapBetween = 18;                 // space between successive labelled groups
        const int step       = groupH + gapBetween;// vertical pitch from one switch to the next
        const int cx         = x + faderLeftPad + faderW + 4 + routeStripW / 2;  // centre of the routing strip

        // Anchor the switch stack from the BOTTOM so the Direct switch's legend lines up with the
        // bottom of the fader. This pushes the whole group (Perc / Shape / Direct) down, leaving a
        // clear gap at the top for the conditionally-shown Perc switch to appear without colliding
        // with the F-number box above the fader. Direct (bottom) -> Shape -> Perc, evenly spaced.
        const int faderBottom = faderTop + drawbarHeight;
        const int outY  = faderBottom - groupH;   // Direct: circle + legend end at the fader bottom
        const int pmY   = outY - step;            // Shape
        const int percY = pmY  - step;            // Perc (topmost, only painted when envelope active)
        pmSwitchBounds[i]   = juce::Rectangle<int>(cx - circleD / 2, pmY,   circleD, circleD);
        outSwitchBounds[i]  = juce::Rectangle<int>(cx - circleD / 2, outY,  circleD, circleD);
        percSwitchBounds[i] = juce::Rectangle<int>(cx - circleD / 2, percY, circleD, circleD);
    }

    // Position ADSR labels and value editors
    int mainHeight = bounds.getHeight() - patchBarHeight;
    int harmonicAndAmpHeight = mainHeight - drawbarSectionHeight;
    int harmonicHeight = harmonicAndAmpHeight / 2;
    int adsrZoneWidth = (int)(leftWidth * 0.65f);
    int adsrGraphHeight = harmonicHeight - 101;   // shrunk to leave the top label strip + a bottom border
    int adsrGraphY = drawbarSectionHeight + zoneLabelH + 8;
    int adsrGraphWidth = adsrZoneWidth - 40;
    int adsrLabelY = adsrGraphY + adsrGraphHeight + 8;
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
    int lfoZoneY = drawbarSectionHeight + zoneLabelH;   // triangle nudged up; combos stay put (gap below grows)
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

    // LFO Shape and Sync combos below the base knobs and their values

    lfoSpeedValue.setBounds(base1X, baseY + 16 + smallKnobSize, smallKnobSize, smallKnobValueHeight);
    lfoDepthValue.setBounds(base2X, baseY + 16 + smallKnobSize, smallKnobSize, smallKnobValueHeight);
    int comboY = baseY + 16 + smallKnobSize + smallKnobValueHeight + 15;  // +10 keeps combos put after the triangle moved up
    int comboHalfWidth = (lfoZoneWidth - 30) / 2;
    int comboLabelW = 40;

    // Shape combo (left half)
    lfoShapeLabel.setBounds(lfoZoneX + 5, comboY, comboLabelW, 22);
    lfoShapeCombo.setBounds(lfoZoneX + 5 + comboLabelW, comboY, comboHalfWidth - comboLabelW + 5, 22);

    // Sync combo (right half)
    int syncX = lfoZoneX + 10 + comboHalfWidth;
    lfoSyncLabel.setBounds(syncX, comboY, comboLabelW, 22);
    lfoSyncCombo.setBounds(syncX + comboLabelW, comboY, comboHalfWidth - comboLabelW + 5, 22);

    // F1: the LFO/velocity zone — a background drag anywhere in here (i.e. not on a knob/combo,
    // which grab their own clicks) starts a mod-params copy. Spans the triangle down through the combos.
    modZoneBounds = juce::Rectangle<int>(lfoZoneX, lfoZoneY, lfoZoneWidth, (comboY + 22) - lfoZoneY);

    // ======================== AMPLITUDE ADSR SECTION ========================
    int ampHeight = harmonicAndAmpHeight - harmonicHeight;
    int ampAdsrGraphHeight = ampHeight - 90;   // shrunk to leave the top label strip + a bottom border
    int ampAdsrGraphY = drawbarSectionHeight + harmonicHeight + zoneLabelH + 8;
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

    // Character zone: 2x2 square sharing the LFO base columns (ampBase1X/ampBase2X line up
    // with LFO Speed/Depth above). Envelope-shaping on top (Vel->Attk, Env Curve),
    // analog-character on the bottom (Vintage, Life). SEED + re-cast sit centred below,
    // joined to Life by a lightning bolt drawn in paint().
    int ampKnobSize = 60;
    int ampKnobValueHeight = 18;
    int ampLabelGap = 18;   // a touch more air between label and knob than before
    int ampBlockH = ampLabelGap + ampKnobSize + ampKnobValueHeight;

    int ampBaseSpacing = (lfoZoneWidth - ampKnobSize * 2) / 3;
    int ampCol1X = lfoZoneX + ampBaseSpacing;
    int ampCol2X = ampCol1X + ampKnobSize + ampBaseSpacing;

    int ampRow1Y = drawbarSectionHeight + harmonicHeight + 14 + zoneLabelH;  // nudged up for more bottom border
    int ampRow2Y = ampRow1Y + ampBlockH + 12;

    auto placeKnob = [&](juce::Label& lbl, juce::Slider& knob, juce::Label& val, int cx, int cy)
    {
        lbl.setBounds(cx, cy, ampKnobSize, 16);
        knob.setBounds(cx, cy + ampLabelGap, ampKnobSize, ampKnobSize);
        val.setBounds(cx, cy + ampLabelGap + ampKnobSize, ampKnobSize, ampKnobValueHeight);
    };

    placeKnob(velAttackLabel, velAttackKnob, velAttackValue, ampCol1X, ampRow1Y);
    placeKnob(envCurveLabel,  envCurveKnob,  envCurveValue,  ampCol2X, ampRow1Y);
    placeKnob(vintageLabel,   vintageKnob,   vintageValue,   ampCol1X, ampRow2Y);
    placeKnob(lifeLabel,      lifeKnob,      lifeValue,      ampCol2X, ampRow2Y);

    lifeKnobBounds = lifeKnob.getBounds();

    // SEED module, centred under the square: [SEED] [number] [* re-cast]
    int seedModW = 150, seedModH = 24;
    int seedModX = lfoZoneX + (lfoZoneWidth - seedModW) / 2;
    int seedModY = ampRow2Y + ampBlockH + 10;
    seedLabel.setBounds(seedModX, seedModY, 38, seedModH);
    seedValue.setBounds(seedModX + 40, seedModY, 62, seedModH);
    rerollButton.setBounds(seedModX + 110, seedModY + 1, seedModH - 2, seedModH - 2);
    seedModuleBounds = juce::Rectangle<int>(seedModX, seedModY, seedModW, seedModH);

    // Dev voicing panel: floats over the harmonic-envelope zone when toggled
    voicingPanel.setBounds(20, drawbarSectionHeight + 20, 330, 240);

    // ======================== RIGHT COLUMN LAYOUT ========================
    int rightX = leftWidth + 10;
    int rightContentWidth = bounds.getWidth() - leftWidth - 20;
    int rightKnobSize = 80;

    // Waveform display — top aligned with the drawbar columns; "Waveform" label sits above it
    waveformDisplay.setBounds(rightX, drawbarMargin, rightContentWidth, drawbarSectionHeight - drawbarMargin * 2);
    
    int waveformHeight = drawbarSectionHeight;
    int remainingHeight = mainHeight - waveformHeight;
    int rightSectionHeight = remainingHeight / 4;
    
    // Vibrato + Pitch sections share one 3-knob row (F2a). The right-hand column - aligned above
    // the Colour-zone mod-wheel buttons - holds the Portamento Rate/Time toggle in the Pitch row and
    // is left empty in the Vibrato row (reserved for a future control). Both rows spread their knobs
    // evenly in the space LEFT of that column, so the two rows line up vertically.
    int knobValueHeight = 20;

    // Right-hand reserved column X mirrors the Colour-section mod-wheel-button formula below.
    const int mwBtnW_       = 38;
    const int rightColX     = (rightX + 20) + (int)((rightContentWidth - 40) * 0.9f) + 4;
    const int mwCenterX     = rightColX + mwBtnW_ / 2;
    const int knobAreaRight = rightColX - 6;                  // small gap before the column
    const int triKnobSpacing = (knobAreaRight - rightX) / 3;  // three evenly-spaced knob slots

    auto triKnobX = [&](int i) {
        return rightX + i * triKnobSpacing + (triKnobSpacing - rightKnobSize) / 2;
    };

    // ---- Vibrato section: Rate / Depth / Fade (right slot intentionally empty) ----
    int vibratoY = waveformHeight + 5 + zoneLabelH;
    juce::Slider* vibKnobs[3]  = { &vibratoRateKnob,  &vibratoDepthKnob,  &vibratoFadeKnob };
    juce::Label*  vibLabels[3] = { &vibratoRateLabel, &vibratoDepthLabel, &vibratoFadeLabel };
    juce::Label*  vibValues[3] = { &vibratoRateValue, &vibratoDepthValue, &vibratoFadeValue };
    for (int i = 0; i < 3; ++i) {
        int x = triKnobX(i);
        vibLabels[i]->setBounds(x, vibratoY, rightKnobSize, 16);
        vibKnobs[i]->setBounds(x, vibratoY + 16, rightKnobSize, rightKnobSize);
        vibValues[i]->setBounds(x + 10, vibratoY + 16 + rightKnobSize, rightKnobSize - 20, knobValueHeight);
    }

    // ---- Pitch section: Distance / Time / Porta, with the Rate/Time toggle in the right column ----
    int pitchY = waveformHeight + rightSectionHeight + 5 + zoneLabelH;
    juce::Slider* pitchKnobs[3]  = { &pitchDistKnob,  &pitchTimeKnob,  &portamentoTimeKnob };
    juce::Label*  pitchLabels[3] = { &pitchDistLabel, &pitchTimeLabel, &portamentoLabel };
    juce::Label*  pitchValues[3] = { &pitchDistValue, &pitchTimeValue, &portamentoValue };
    for (int i = 0; i < 3; ++i) {
        int x = triKnobX(i);
        pitchLabels[i]->setBounds(x, pitchY, rightKnobSize, 16);
        pitchKnobs[i]->setBounds(x, pitchY + 16, rightKnobSize, rightKnobSize);
        pitchValues[i]->setBounds(x + 10, pitchY + 16 + rightKnobSize, rightKnobSize - 20, knobValueHeight);
    }

    // Rate/Time toggle: same size as the Colour-zone MW buttons and left-aligned to the same column
    // (mwCenterX - mwBtnW_/2 == rightColX), so it sits directly above them. Vertically centred on the
    // Pitch-row knob bodies. Drawn in paint(); hit-tested in mouseDown().
    const int toggleH = 20;   // == mwBtnH below
    portamentoModeButtonBounds = juce::Rectangle<int>(mwCenterX - mwBtnW_ / 2,
                                                      pitchY + 16 + (rightKnobSize - toggleH) / 2,
                                                      mwBtnW_, toggleH);

    // Colour section: Brilliance + Density, each a horizontal slider with its label ABOVE it,
    // left-aligned. The pair sits low in the zone; sliders are shortened 10% at the right end
    // (left end fixed) to leave room for a future per-slider "Mod wheel" button.
    int colStartY = waveformHeight + rightSectionHeight * 2 + 40;   // dropped low in the zone
    int sliderMargin = 20;
    int colSliderX = rightX + sliderMargin;
    int colFullW = rightContentWidth - sliderMargin * 2;
    int colSliderW = (int)(colFullW * 0.9f);   // 10% shorter, left end fixed; right end reserved for Mod-wheel button
    int colLabelH = 16;
    int colSliderH = 26;
    int colRowGap = 10;

    brillianceMainLabel.setBounds(rightX, colStartY, rightContentWidth, 18);   // hidden; kept positioned

    // Mod-wheel button geometry (in the reserved gap at the right of each slider); two zones -
    // "MW" (left half) + polarity triangle (right half).
    int mwBtnW = 38, mwBtnH = 20;
    int mwBtnX = colSliderX + colSliderW + 4;

    // Brilliance: label above, slider below, mod-wheel button at the right
    brillianceSubLabel.setBounds(colSliderX, colStartY, colFullW, colLabelH);
    int brillSliderY = colStartY + colLabelH + 2;
    brillianceSliderBounds = juce::Rectangle<int>(colSliderX, brillSliderY, colSliderW, colSliderH);
    brillianceSlider.setBounds(brillianceSliderBounds);
    brillianceValue.setBounds(colSliderX, colStartY, colFullW, colLabelH);   // hidden
    brillianceMWButtonBounds = juce::Rectangle<int>(mwBtnX, brillSliderY + (colSliderH - mwBtnH) / 2, mwBtnW, mwBtnH);

    // Density: label above, slider below, mod-wheel button at the right
    int densityRowY = colStartY + colLabelH + 2 + colSliderH + colRowGap;
    densitySubLabel.setBounds(colSliderX, densityRowY, colFullW, colLabelH);
    int densSliderY = densityRowY + colLabelH + 2;
    densitySliderBounds = juce::Rectangle<int>(colSliderX, densSliderY, colSliderW, colSliderH);
    densitySlider.setBounds(densitySliderBounds);
    densityValue.setBounds(colSliderX, densityRowY, colFullW, colLabelH);   // hidden
    densityMWButtonBounds = juce::Rectangle<int>(mwBtnX, densSliderY + (colSliderH - mwBtnH) / 2, mwBtnW, mwBtnH);
    
    // Effects section (5 sliders: Detune, Mix, Warmth, Punch, Freq)
    int effectsY = waveformHeight + rightSectionHeight * 3 + 1 + zoneLabelH;
    int effectsSliderWidth = 55;
    int effectsValueHeight = 20;
    int effectsSliderHeight = rightSectionHeight - 68 - zoneLabelH;  // Room for top label strip + value
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

    // F7/F8 routing switches: two small squares per drawbar column, below the fader. A click
    // toggles the destination without changing which drawbar is selected. (These sit on the
    // parent background, so the event reaches here rather than a slider.)
    for (int i = 0; i < 10; ++i)
    {
        // F10 Perc switch (only live when the drawbar's envelope is active - matches its visibility).
        if (drawbarEnvelopeActive(i) && percSwitchBounds[i].contains(event.getPosition()))
        {
            toggleRoutingParam("k" + juce::String(i + 1) + "TrigSingle");
            return;
        }
        if (pmSwitchBounds[i].contains(event.getPosition()))
        {
            toggleRoutingParam("k" + juce::String(i + 1) + "ToPM");
            return;
        }
        if (outSwitchBounds[i].contains(event.getPosition()))
        {
            toggleRoutingParam("k" + juce::String(i + 1) + "ToOut");
            return;
        }
    }

    // Colour-zone mod-wheel buttons: left "MW" half toggles on/off, right half flips polarity.
    if (brillianceMWButtonBounds.contains(event.getPosition()))
    {
        handleMWButtonClick("brillianceModWheel", brillianceMWButtonBounds, event.getPosition(), brillLastPolarity);
        return;
    }
    if (densityMWButtonBounds.contains(event.getPosition()))
    {
        handleMWButtonClick("carrierMorphModWheel", densityMWButtonBounds, event.getPosition(), densLastPolarity);
        return;
    }

    // Portamento Rate/Time toggle: a plain binary flip via the host (like the routing switches).
    if (portamentoModeButtonBounds.contains(event.getPosition()))
    {
        toggleRoutingParam("portamentoMode");
        return;
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

        // Not on a handle: begin a copy-drag of this drawbar's envelope from the graph background.
        if (harmonicEnvBounds.contains(event.x, event.y))
        {
            copyDrag = CopyDragKind::Envelope;
            copyDragSource = selectedDrawbar;
            copyDragHoverTarget = -1;
            setMouseCursor(juce::MouseCursor::CopyingCursor);
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
    
    // Background drag in the LFO/velocity zone: begin a copy-drag of this drawbar's mod params.
    if (copyDrag == CopyDragKind::None && modZoneBounds.contains(event.x, event.y))
    {
        copyDrag = CopyDragKind::Mod;
        copyDragSource = selectedDrawbar;
        copyDragHoverTarget = -1;
        setMouseCursor(juce::MouseCursor::CopyingCursor);
        return;
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
    // Copy-drag in progress: track which drawbar column the pointer is over (for the drop highlight).
    if (copyDrag != CopyDragKind::None)
    {
        int t = drawbarColumnAt(event.getPosition());
        if (t == copyDragSource) t = -1;      // the source itself isn't a valid drop target
        if (t != copyDragHoverTarget)
        {
            copyDragHoverTarget = t;
            repaint();
        }
        return;
    }

    if (currentDragTarget == DragTarget::None)
        return;

    updateAdsrFromDrag(event);
    repaint();
}

void PLANETMainGui::mouseUp(const juce::MouseEvent& event)
{
    // Complete a copy-drag: if released over a different drawbar's column, copy the params.
    if (copyDrag != CopyDragKind::None)
    {
        const int target = drawbarColumnAt(event.getPosition());
        if (target >= 0 && target != copyDragSource)
        {
            if (copyDrag == CopyDragKind::Envelope)
                copyEnvelopeParamsBetweenDrawbars(copyDragSource, target);
            else
                copyModParamsBetweenDrawbars(copyDragSource, target);

            // Switch focus to the target so you can immediately fine-tune what you just copied
            // (e.g. tweak the env depth on the new drawbar). Rebinds all context controls.
            selectedDrawbar = target;
            updateAdsrDisplay();
        }

        copyDrag = CopyDragKind::None;
        copyDragHoverTarget = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
        return;
    }

    currentDragTarget = DragTarget::None;
}

int PLANETMainGui::drawbarColumnAt(juce::Point<int> p) const
{
    for (int i = 0; i < 10; ++i)
        if (drawbarColumnBounds[i].contains(p))
            return i;
    return -1;
}

void PLANETMainGui::toggleRoutingParam(const juce::String& paramID)
{
    // Flip the 0/1 routing switch via the host (automatable / undoable / notifies listeners).
    if (auto* p = apvts.getParameter(paramID))
        p->setValueNotifyingHost(p->getValue() >= 0.5f ? 0.0f : 1.0f);

    repaint();
}

// F10: is drawbar i's envelope active? Same test that turns its fader thumb red (see the thumb-colour
// logic in the timer update). Gates whether the "Perc" single-trigger switch is shown / clickable.
bool PLANETMainGui::drawbarEnvelopeActive(int drawbarIndex) const
{
    if (drawbarIndex < 0 || drawbarIndex >= 10)
        return false;
    if (auto* envPtr = apvts.getRawParameterValue("k" + juce::String(drawbarIndex + 1) + "EnvelopeAmount"))
        return std::abs(envPtr->load()) > 0.001f;
    return false;
}

void PLANETMainGui::handleMWButtonClick(const juce::String& paramID, juce::Rectangle<int> bounds,
                                        juce::Point<int> pos, int& lastPolarity)
{
    auto* p = apvts.getParameter(paramID);
    if (p == nullptr) return;

    int cur = (int)std::round(p->convertFrom0to1(p->getValue()));   // 0 Off / 1 Normal / 2 Inverse
    bool triangleZone = pos.getX() >= bounds.getCentreX();

    if (triangleZone)
    {
        // Right half: flip polarity. If on, apply live; if off, just flip the remembered polarity
        // (stays off) so it can be pre-set without connecting.
        if (cur != 0)
        {
            lastPolarity = (cur == 1) ? 2 : 1;
            p->setValueNotifyingHost(p->convertTo0to1((float)lastPolarity));
        }
        else
        {
            lastPolarity = (lastPolarity == 1) ? 2 : 1;
        }
    }
    else
    {
        // Left "MW" half: toggle on/off. Turning off remembers the polarity; turning on restores it.
        // Reaching Off is one clean click and never passes through a polarity flip.
        if (cur == 0)
            p->setValueNotifyingHost(p->convertTo0to1((float)lastPolarity));
        else
        {
            lastPolarity = cur;
            p->setValueNotifyingHost(p->convertTo0to1(0.0f));
        }
    }
    repaint();
}

void PLANETMainGui::copyEnvelopeParamsBetweenDrawbars(int from, int to)
{
    // ADSR + envelope depth. Copies the normalised value; every k{n} param shares the same range,
    // so the normalised value maps to the identical real value on the target. Notifies the host so
    // it's automatable/undoable and the GUI refreshes.
    static const char* const suffixes[] =
        { "AttackTime", "DecayTime", "SustainLevel", "ReleaseTime", "EnvelopeAmount" };
    const juce::String fromPrefix = "k" + juce::String(from + 1);
    const juce::String toPrefix   = "k" + juce::String(to + 1);
    for (auto* s : suffixes)
    {
        auto* src = apvts.getParameter(fromPrefix + s);
        auto* dst = apvts.getParameter(toPrefix + s);
        if (src != nullptr && dst != nullptr)
            dst->setValueNotifyingHost(src->getValue());
    }
}

void PLANETMainGui::copyModParamsBetweenDrawbars(int from, int to)
{
    // LFO (shape/rate/amount/sync/division) + the per-drawbar velocity param. Same normalised-copy
    // approach as the envelope copy above.
    static const char* const suffixes[] =
        { "LFOShape", "LFORate", "LFOAmount", "LFOSync", "LFOSyncDiv", "VelToHarmonic" };
    const juce::String fromPrefix = "k" + juce::String(from + 1);
    const juce::String toPrefix   = "k" + juce::String(to + 1);
    for (auto* s : suffixes)
    {
        auto* src = apvts.getParameter(fromPrefix + s);
        auto* dst = apvts.getParameter(toPrefix + s);
        if (src != nullptr && dst != nullptr)
            dst->setValueNotifyingHost(src->getValue());
    }
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
    
    // CRITICAL: Reset attachments BEFORE creating new ones
    lfoSpeedAttachment.reset();
    lfoSyncDivAttachment.reset();
    lfoDepthAttachment.reset();
    lfoShapeAttachment.reset();
    lfoSyncAttachment.reset();
    envDepthAttachment.reset();
    velToDrawbarAttachment.reset();

    // Read sync state for the newly selected drawbar BEFORE creating attachments
    bool syncOn = false;
    if (auto* syncParam = apvts.getRawParameterValue(prefix + "LFOSync"))
        syncOn = syncParam->load() > 0.5f;
    currentSyncMode = syncOn;

    // Create non-speed attachments
    lfoDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "LFOAmount", lfoDepthKnob);
    lfoShapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, prefix + "LFOShape", lfoShapeCombo);
    lfoSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, prefix + "LFOSync", lfoSyncCombo);
    envDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "EnvelopeAmount", envDepthKnob);
    velToDrawbarAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "VelToHarmonic", velToDrawbarKnob);

    // Bind speed knob to correct parameter based on sync mode
    updateLfoSyncMode();
    
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

    // Re-tint the per-drawbar controls to the selected drawbar's colour.
    drawbarIshtarLookAndFeel.starColour = drawbarColours[selectedDrawbar];
    envDepthKnob.setColour(juce::Slider::thumbColourId, drawbarColours[selectedDrawbar]);
    lfoSpeedKnob.repaint();
    lfoDepthKnob.repaint();
    velToDrawbarKnob.repaint();
    envDepthKnob.repaint();
}

void PLANETMainGui::updateLfoSyncMode()
{
    juce::String prefix = "k" + juce::String(selectedDrawbar + 1);

    // Always detach speed knob first
    lfoSpeedAttachment.reset();
    lfoSyncDivAttachment.reset();

    if (currentSyncMode) {
        // SYNC MODE: Speed knob becomes division selector (0-12, integer steps)
        lfoSpeedKnob.setRange(0.0, 12.0, 1.0);
        lfoSpeedKnob.setSkewFactor(1.0);  // Linear for discrete steps

        // Bind to sync division parameter
        lfoSyncDivAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "LFOSyncDiv", lfoSpeedKnob);

        lfoSpeedLabel.setText("Div", juce::dontSendNotification);

        // Update value label to show division name
        lfoSpeedKnob.onValueChange = [this]() {
            int divIndex = juce::jlimit(0, NUM_SYNC_DIVISIONS - 1, (int)lfoSpeedKnob.getValue());
            lfoSpeedValue.setText(SYNC_DIVISION_NAMES[divIndex], juce::dontSendNotification);
        };

        // Set initial value display
        int divIndex = juce::jlimit(0, NUM_SYNC_DIVISIONS - 1, (int)lfoSpeedKnob.getValue());
        lfoSpeedValue.setText(SYNC_DIVISION_NAMES[divIndex], juce::dontSendNotification);
    }
    else {
        // FREE MODE: Speed knob is Hz rate (original behaviour)
        lfoSpeedKnob.setRange(0.05, 20.0, 0.01);
        lfoSpeedKnob.setSkewFactor(0.5);  // More precision in low range

        // Bind to LFO rate parameter
        lfoSpeedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "LFORate", lfoSpeedKnob);

        lfoSpeedLabel.setText("Speed", juce::dontSendNotification);

        // Update value label to show Hz
        lfoSpeedKnob.onValueChange = [this]() {
            lfoSpeedValue.setText(juce::String(lfoSpeedKnob.getValue(), 2), juce::dontSendNotification);
        };

        // Set initial value display
        lfoSpeedValue.setText(juce::String(lfoSpeedKnob.getValue(), 2), juce::dontSendNotification);
    }
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
    if (parameterID == "lifeAmount") {
        lifeValue.setText(juce::String((int)newValue), juce::dontSendNotification);
        return;
    }
    if (parameterID == "lifeSeed") {
        seedValue.setText(juce::String((int)newValue), juce::dontSendNotification);
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
    if (auto* param = apvts.getParameter("lifeAmount"))
        lifeValue.setText(juce::String((int)param->convertFrom0to1(param->getValue())), juce::dontSendNotification);
    if (auto* param = apvts.getParameter("lifeSeed"))
        seedValue.setText(juce::String((int)param->convertFrom0to1(param->getValue())), juce::dontSendNotification);

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
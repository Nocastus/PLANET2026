/*
  ==============================================================================
    PLANETMainGui.cpp - User-Friendly GUI for PLANET Synthesizer
    Mockup Phase - No parameter binding yet
  ==============================================================================
*/

#include "PLANETMainGui.h"

PLANETMainGui::PLANETMainGui()
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
    
    // Brilliance knob
    setupKnob(brillianceKnob, brillianceMainLabel, "Brilliance", 0.0, 1.0, 0.5);
    
    // Effects knobs
    setupKnob(detuneAmountKnob, detuneAmountLabel, "Detune", 0.0, 1.0, 0.0);
    setupKnob(detuneMixKnob, detuneMixLabel, "Det Mix", 0.0, 1.0, 0.0);
    setupKnob(reverbTimeKnob, reverbTimeLabel, "Reverb", 0.0, 1.0, 0.3);
    setupKnob(reverbMixKnob, reverbMixLabel, "Rev Mix", 0.0, 1.0, 0.0);

    setSize(1400, 800);
}

PLANETMainGui::~PLANETMainGui()
{
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

        // Normalise times for display
        float totalTime = attack + decay + 0.3f + release;  // 0.3 for sustain hold
        if (totalTime < 0.1f) totalTime = 0.1f;
        float timeScale = (float)adsrGraphWidth / totalTime;

        // Calculate points
        float x0 = (float)adsrMargin;
        float y0 = (float)(adsrGraphY + adsrGraphHeight);  // Bottom (zero level)
        float yTop = (float)(adsrGraphY + 10);              // Top (full level)
        float ySustain = yTop + (1.0f - sustain) * (y0 - yTop - 10);

        float x1 = x0 + attack * timeScale;                 // End of attack
        float x2 = x1 + decay * timeScale;                  // End of decay
        float x3 = x2 + 0.3f * timeScale;                   // End of sustain hold
        float x4 = x3 + release * timeScale;                // End of release

        // Draw envelope line
        juce::Path envPath;
        envPath.startNewSubPath(x0, y0);           // Start at zero
        envPath.lineTo(x1, yTop);                  // Attack to peak
        envPath.lineTo(x2, ySustain);              // Decay to sustain
        envPath.lineTo(x3, ySustain);              // Sustain hold
        envPath.lineTo(x4, y0);                    // Release to zero

        g.setColour(drawbarColours[selectedDrawbar]);
        g.strokePath(envPath, juce::PathStrokeType(2.5f));
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

        // Normalise times for display
        float totalTime = attack + decay + 0.3f + release;
        if (totalTime < 0.1f) totalTime = 0.1f;
        float timeScale = (float)adsrGraphWidth / totalTime;

        // Calculate points
        float x0 = (float)adsrMargin;
        float y0 = (float)(adsrGraphY + adsrGraphHeight);
        float yTop = (float)(adsrGraphY + 10);
        float ySustain = yTop + (1.0f - sustain) * (y0 - yTop - 10);

        float x1 = x0 + attack * timeScale;
        float x2 = x1 + decay * timeScale;
        float x3 = x2 + 0.3f * timeScale;
        float x4 = x3 + release * timeScale;

        // Draw envelope line in white for global section
        juce::Path envPath;
        envPath.startNewSubPath(x0, y0);
        envPath.lineTo(x1, yTop);
        envPath.lineTo(x2, ySustain);
        envPath.lineTo(x3, ySustain);
        envPath.lineTo(x4, y0);

        g.setColour(juce::Colours::white);
        g.strokePath(envPath, juce::PathStrokeType(2.5f));
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
    int adsrLabelY = drawbarSectionHeight + adsrGraphHeight + 20;
    int adsrFieldWidth = 50;
    int adsrFieldHeight = 25;
    int adsrSpacing = (adsrZoneWidth - 40) / 4;

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

    // 2x2 knob grid in Amplitude zone (below LFO knobs position)
    int ampKnobSize = 60;
    int ampKnobValueHeight = 20;
    int ampKnobSpacing = 10;
    int ampKnobStartX = lfoZoneX + 10;
    int ampKnobStartY = drawbarSectionHeight + harmonicHeight + 10;
    int ampKnobColWidth = ampKnobSize + ampKnobSpacing;
    int ampKnobRowHeight = 18 + ampKnobSize + ampKnobValueHeight + 10;  // label + knob + value + gap

    // Row 1: Vel Brill, Vel Attack
    velBrillLabel.setBounds(ampKnobStartX, ampKnobStartY, ampKnobSize, 18);
    velBrillKnob.setBounds(ampKnobStartX, ampKnobStartY + 18, ampKnobSize, ampKnobSize);
    velBrillValue.setBounds(ampKnobStartX, ampKnobStartY + 18 + ampKnobSize, ampKnobSize, ampKnobValueHeight);

    velAttackLabel.setBounds(ampKnobStartX + ampKnobColWidth, ampKnobStartY, ampKnobSize, 18);
    velAttackKnob.setBounds(ampKnobStartX + ampKnobColWidth, ampKnobStartY + 18, ampKnobSize, ampKnobSize);
    velAttackValue.setBounds(ampKnobStartX + ampKnobColWidth, ampKnobStartY + 18 + ampKnobSize, ampKnobSize, ampKnobValueHeight);

    // Row 2: Env Curve, Vintage
    int row2Y = ampKnobStartY + ampKnobRowHeight;
    envCurveLabel.setBounds(ampKnobStartX, row2Y, ampKnobSize, 18);
    envCurveKnob.setBounds(ampKnobStartX, row2Y + 18, ampKnobSize, ampKnobSize);
    envCurveValue.setBounds(ampKnobStartX, row2Y + 18 + ampKnobSize, ampKnobSize, ampKnobValueHeight);

    vintageLabel.setBounds(ampKnobStartX + ampKnobColWidth, row2Y, ampKnobSize, 18);
    vintageKnob.setBounds(ampKnobStartX + ampKnobColWidth, row2Y + 18, ampKnobSize, ampKnobSize);
    vintageValue.setBounds(ampKnobStartX + ampKnobColWidth, row2Y + 18 + ampKnobSize, ampKnobSize, ampKnobValueHeight);

    // ======================== RIGHT COLUMN LAYOUT ========================
    int rightX = leftWidth + 10;
    int rightContentWidth = bounds.getWidth() - leftWidth - 20;
    int rightKnobSize = 50;
    
    // Calculate section heights for right column (below waveform)
    int waveformHeight = drawbarSectionHeight;
    int remainingHeight = mainHeight - waveformHeight;
    int rightSectionHeight = remainingHeight / 4;  // 4 sections: Vibrato, Pitch, Brilliance, Effects
    
    // Vibrato section (3 knobs)
    int vibratoY = waveformHeight + 15;
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
    int pitchY = waveformHeight + rightSectionHeight + 15;
    int pitchKnobSpacing = rightContentWidth / 2;
    
    int pkx0 = rightX + (pitchKnobSpacing - rightKnobSize) / 2;
    pitchDistLabel.setBounds(pkx0, pitchY, rightKnobSize, 16);
    pitchDistKnob.setBounds(pkx0, pitchY + 16, rightKnobSize, rightKnobSize);
    
    int pkx1 = rightX + pitchKnobSpacing + (pitchKnobSpacing - rightKnobSize) / 2;
    pitchTimeLabel.setBounds(pkx1, pitchY, rightKnobSize, 16);
    pitchTimeKnob.setBounds(pkx1, pitchY + 16, rightKnobSize, rightKnobSize);
    
    // Brilliance section (1 knob, centred)
    int brillianceY = waveformHeight + rightSectionHeight * 2 + 15;
    int bkx = rightX + (rightContentWidth - rightKnobSize) / 2;
    brillianceMainLabel.setBounds(bkx, brillianceY, rightKnobSize, 16);
    brillianceKnob.setBounds(bkx, brillianceY + 16, rightKnobSize, rightKnobSize);
    
    // Effects section (4 knobs in 2x2 grid)
    int effectsY = waveformHeight + rightSectionHeight * 3 + 10;
    int effectsKnobSpacing = rightContentWidth / 2;
    int effectsRowHeight = (rightSectionHeight - 20) / 2;
    
    // Row 1: Detune Amount, Detune Mix
    int ekx0 = rightX + (effectsKnobSpacing - rightKnobSize) / 2;
    detuneAmountLabel.setBounds(ekx0, effectsY, rightKnobSize, 14);
    detuneAmountKnob.setBounds(ekx0, effectsY + 14, rightKnobSize, rightKnobSize);
    
    int ekx1 = rightX + effectsKnobSpacing + (effectsKnobSpacing - rightKnobSize) / 2;
    detuneMixLabel.setBounds(ekx1, effectsY, rightKnobSize, 14);
    detuneMixKnob.setBounds(ekx1, effectsY + 14, rightKnobSize, rightKnobSize);
    
    // Row 2: Reverb Time, Reverb Mix
    int effectsRow2Y = effectsY + effectsRowHeight;
    reverbTimeLabel.setBounds(ekx0, effectsRow2Y, rightKnobSize, 14);
    reverbTimeKnob.setBounds(ekx0, effectsRow2Y + 14, rightKnobSize, rightKnobSize);
    
    reverbMixLabel.setBounds(ekx1, effectsRow2Y, rightKnobSize, 14);
    reverbMixKnob.setBounds(ekx1, effectsRow2Y + 14, rightKnobSize, rightKnobSize);
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
}

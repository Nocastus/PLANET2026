/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PLANETtest4AudioProcessorEditor::PLANETtest4AudioProcessorEditor(PLANETtest4AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Set up the Brilliance slider
    brillianceSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    brillianceSlider.setRange(0.0, 1.0, 0.01);
    brillianceSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(brillianceSlider);

    brillianceLabel.setText("Brilliance", juce::dontSendNotification);
    brillianceLabel.attachToComponent(&brillianceSlider, false);
    addAndMakeVisible(brillianceLabel);

    brillianceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "brilliance", brillianceSlider);

    vibratoRateSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    vibratoRateSlider.setRange(0.5f, 12.0f, 0.1f);
    vibratoRateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(vibratoRateSlider);
    vibratoRateLabel.setText("Vibrato Rate", juce::dontSendNotification);
    vibratoRateLabel.attachToComponent(&vibratoRateSlider, false);
    addAndMakeVisible(vibratoRateLabel);
    vibratoRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "vibratoRate", vibratoRateSlider);

    // Vibrato Depth
    vibratoDepthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    vibratoDepthSlider.setRange(0.0f, 2.0f, 0.01f);
    vibratoDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(vibratoDepthSlider);
    vibratoDepthLabel.setText("Vibrato Depth", juce::dontSendNotification);
    vibratoDepthLabel.attachToComponent(&vibratoDepthSlider, false);
    addAndMakeVisible(vibratoDepthLabel);
    vibratoDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "vibratoDepth", vibratoDepthSlider);

    // Vibrato Fade-In
    vibratoFadeInSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    vibratoFadeInSlider.setRange(0.0f, 10.0f, 0.1f);
    vibratoFadeInSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(vibratoFadeInSlider);
    vibratoFadeInLabel.setText("Vibrato Fade-In", juce::dontSendNotification);
    vibratoFadeInLabel.attachToComponent(&vibratoFadeInSlider, false);
    addAndMakeVisible(vibratoFadeInLabel);
    vibratoFadeInAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "vibratoFadeIn", vibratoFadeInSlider);

    // ======================== EFFECTS CONTROLS SETUP ========================
    // Detune Amount
    detuneAmountSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    detuneAmountSlider.setRange(0.0f, 1.0f, 0.01f);
    detuneAmountSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(detuneAmountSlider);
    detuneAmountLabel.setText("Detune Amount", juce::dontSendNotification);
    detuneAmountLabel.attachToComponent(&detuneAmountSlider, false);
    addAndMakeVisible(detuneAmountLabel);
    detuneAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "detuneAmount", detuneAmountSlider);

    // Detune Mix
    detuneMixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    detuneMixSlider.setRange(0.0f, 1.0f, 0.01f);
    detuneMixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(detuneMixSlider);
    detuneMixLabel.setText("Detune Mix", juce::dontSendNotification);
    detuneMixLabel.attachToComponent(&detuneMixSlider, false);
    addAndMakeVisible(detuneMixLabel);
    detuneMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "detuneMix", detuneMixSlider);

    // Reverb Length
    reverbLengthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    reverbLengthSlider.setRange(0.0f, 1.0f, 0.01f);
    reverbLengthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(reverbLengthSlider);
    reverbLengthLabel.setText("Reverb Length", juce::dontSendNotification);
    reverbLengthLabel.attachToComponent(&reverbLengthSlider, false);
    addAndMakeVisible(reverbLengthLabel);
    reverbLengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "reverbLength", reverbLengthSlider);

    // Reverb Damping
    reverbDampingSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    reverbDampingSlider.setRange(0.0f, 1.0f, 0.01f);
    reverbDampingSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(reverbDampingSlider);
    reverbDampingLabel.setText("Reverb Damping", juce::dontSendNotification);
    reverbDampingLabel.attachToComponent(&reverbDampingSlider, false);
    addAndMakeVisible(reverbDampingLabel);
    reverbDampingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "reverbDamping", reverbDampingSlider);

    // Reverb Width
    reverbWidthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    reverbWidthSlider.setRange(0.0f, 1.0f, 0.01f);
    reverbWidthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(reverbWidthSlider);
    reverbWidthLabel.setText("Reverb Width", juce::dontSendNotification);
    reverbWidthLabel.attachToComponent(&reverbWidthSlider, false);
    addAndMakeVisible(reverbWidthLabel);
    reverbWidthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "reverbWidth", reverbWidthSlider);

    // Reverb Mix
    reverbMixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    reverbMixSlider.setRange(0.0f, 1.0f, 0.01f);
    reverbMixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(reverbMixSlider);
    reverbMixLabel.setText("Reverb Mix", juce::dontSendNotification);
    reverbMixLabel.attachToComponent(&reverbMixSlider, false);
    addAndMakeVisible(reverbMixLabel);
    reverbMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "reverbMix", reverbMixSlider);



    // Envelope Exponential Control
    exponentialControlSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    exponentialControlSlider.setRange(0.0, 1.0, 0.01);
    exponentialControlSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(exponentialControlSlider);

    exponentialControlLabel.setText("Env Curve", juce::dontSendNotification);
    exponentialControlLabel.attachToComponent(&exponentialControlSlider, false);
    addAndMakeVisible(exponentialControlLabel);

    exponentialControlAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "exponentialControl", exponentialControlSlider);

    // ======================== SET UP K1-K10 SLIDERS (ONLY THESE ARE SLIDERS) ========================
    std::array<juce::Slider*, 10> kSliders = { &k1Slider, &k2Slider, &k3Slider, &k4Slider, &k5Slider,
                                              &k6Slider, &k7Slider, &k8Slider, &k9Slider, &k10Slider };

    std::array<juce::Label*, 10> kLabels = { &k1Label, &k2Label, &k3Label, &k4Label, &k5Label,
                                            &k6Label, &k7Label, &k8Label, &k9Label, &k10Label };

    std::array<std::string, 10> kNames = { "K1", "K2", "K3", "K4", "K5", "K6", "K7", "K8", "K9", "K10" };

    for (int i = 0; i < 10; ++i)
    {
        // Set up each K slider as vertical linear slider
        kSliders[i]->setSliderStyle(juce::Slider::LinearVertical);
        kSliders[i]->setRange(-2.0, 2.0, 0.01);  // Fine precision for smooth control
        kSliders[i]->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
        kSliders[i]->setValue(0.0);  // Start at zero
        kSliders[i]->setDoubleClickReturnValue(true, 0.0);  // Double-click resets to zero
        addAndMakeVisible(*kSliders[i]);

        // Set up each label BELOW the slider
        kLabels[i]->setText(kNames[i], juce::dontSendNotification);
        kLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*kLabels[i]);
    }

    // Set up parameter attachments for K1-K10
    k1Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "k1", k1Slider);
    k2Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "k2", k2Slider);
    k3Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "k3", k3Slider);
    k4Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "k4", k4Slider);
    k5Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "k5", k5Slider);
    k6Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "k6", k6Slider);
    k7Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "k7", k7Slider);
    k8Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "k8", k8Slider);
    k9Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "k9", k9Slider);
    k10Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "k10", k10Slider);

    // ======================== SET UP ROW LABELS FOR PARAMETER GRID ========================
    attackTimeRowLabel.setText("Attack Time", juce::dontSendNotification);
    attackTimeRowLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(attackTimeRowLabel);

    decayTimeRowLabel.setText("Decay Time", juce::dontSendNotification);
    decayTimeRowLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(decayTimeRowLabel);

    sustainLevelRowLabel.setText("Sustain Level", juce::dontSendNotification);
    sustainLevelRowLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(sustainLevelRowLabel);

    releaseTimeRowLabel.setText("Release Time", juce::dontSendNotification);
    releaseTimeRowLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(releaseTimeRowLabel);

    envelopeAmountRowLabel.setText("Envelope Amount", juce::dontSendNotification);
    envelopeAmountRowLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(envelopeAmountRowLabel);

    lfoShapeRowLabel.setText("LFO Shape", juce::dontSendNotification);
    lfoShapeRowLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(lfoShapeRowLabel);

    lfoRateRowLabel.setText("LFO Rate", juce::dontSendNotification);
    lfoRateRowLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(lfoRateRowLabel);

    lfoAmountRowLabel.setText("LFO Amount", juce::dontSendNotification);
    lfoAmountRowLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(lfoAmountRowLabel);

    spectralMultiplierRowLabel.setText("Spectral Multiplier", juce::dontSendNotification);
    spectralMultiplierRowLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(spectralMultiplierRowLabel);

    // ======================== SET UP ALL TEXT INPUT FIELDS (90 total) ========================

// Replace the setupTextEditor calls with this dynamic approach:

// Helper function to set up editable text fields with actual parameter defaults
    auto setupTextEditorFromParam = [this](juce::Label& editor, const juce::String& parameterID) {
        editor.setEditable(true);
        editor.setJustificationType(juce::Justification::centred);
        editor.setColour(juce::Label::backgroundColourId, juce::Colours::black);
        editor.setColour(juce::Label::outlineColourId, juce::Colours::grey);
        addAndMakeVisible(editor);

        // Get actual parameter default value and display it
        if (auto* param = audioProcessor.parameters.getParameter(parameterID))
        {
            float defaultValue = param->convertFrom0to1(param->getDefaultValue());
            int decimalPlaces = (defaultValue == std::floor(defaultValue)) ? 0 : 3;
            editor.setText(juce::String(defaultValue, decimalPlaces), juce::dontSendNotification);
        }

        // Handle editor hide (update parameter and show actual clamped value when editing ends)
        editor.onEditorHide = [this, &editor, parameterID]() {
            // First, update the parameter with the typed value
            float typedValue = editor.getText().getFloatValue();
            if (auto* param = audioProcessor.parameters.getParameter(parameterID))
            {
                // Add rounding for spectral multiplier parameters to nearest 0.5
                if (parameterID.startsWith("input_f")) {
                    typedValue = std::round(typedValue * 2.0f) / 2.0f;
                }

                param->setValueNotifyingHost(param->convertTo0to1(typedValue));

                // Then show the actual clamped value
                float actualValue = param->convertFrom0to1(param->getValue());
                int decimalPlaces = (actualValue == std::floor(actualValue)) ? 0 : 3;
                editor.setText(juce::String(actualValue, decimalPlaces), juce::dontSendNotification);
            }
            };
        };

    // Now use it (no more hardcoded defaults!):
    // Attack Time row
    setupTextEditorFromParam(k1AttackTimeEditor, "k1AttackTime");  setupTextEditorFromParam(k2AttackTimeEditor, "k2AttackTime");
    setupTextEditorFromParam(k3AttackTimeEditor, "k3AttackTime");  setupTextEditorFromParam(k4AttackTimeEditor, "k4AttackTime");
    setupTextEditorFromParam(k5AttackTimeEditor, "k5AttackTime");  setupTextEditorFromParam(k6AttackTimeEditor, "k6AttackTime");
    setupTextEditorFromParam(k7AttackTimeEditor, "k7AttackTime");  setupTextEditorFromParam(k8AttackTimeEditor, "k8AttackTime");
    setupTextEditorFromParam(k9AttackTimeEditor, "k9AttackTime");  setupTextEditorFromParam(k10AttackTimeEditor, "k10AttackTime");

    // Decay Time row
    setupTextEditorFromParam(k1DecayTimeEditor, "k1DecayTime");   setupTextEditorFromParam(k2DecayTimeEditor, "k2DecayTime");
    setupTextEditorFromParam(k3DecayTimeEditor, "k3DecayTime");   setupTextEditorFromParam(k4DecayTimeEditor, "k4DecayTime");
    setupTextEditorFromParam(k5DecayTimeEditor, "k5DecayTime");   setupTextEditorFromParam(k6DecayTimeEditor, "k6DecayTime");
    setupTextEditorFromParam(k7DecayTimeEditor, "k7DecayTime");   setupTextEditorFromParam(k8DecayTimeEditor, "k8DecayTime");
    setupTextEditorFromParam(k9DecayTimeEditor, "k9DecayTime");   setupTextEditorFromParam(k10DecayTimeEditor, "k10DecayTime");

    // Sustain Level row (now shows correct 0.5 default!)
    setupTextEditorFromParam(k1SustainLevelEditor, "k1SustainLevel"); setupTextEditorFromParam(k2SustainLevelEditor, "k2SustainLevel");
    setupTextEditorFromParam(k3SustainLevelEditor, "k3SustainLevel"); setupTextEditorFromParam(k4SustainLevelEditor, "k4SustainLevel");
    setupTextEditorFromParam(k5SustainLevelEditor, "k5SustainLevel"); setupTextEditorFromParam(k6SustainLevelEditor, "k6SustainLevel");
    setupTextEditorFromParam(k7SustainLevelEditor, "k7SustainLevel"); setupTextEditorFromParam(k8SustainLevelEditor, "k8SustainLevel");
    setupTextEditorFromParam(k9SustainLevelEditor, "k9SustainLevel"); setupTextEditorFromParam(k10SustainLevelEditor, "k10SustainLevel");

    // Release Time row
    setupTextEditorFromParam(k1ReleaseTimeEditor, "k1ReleaseTime");  setupTextEditorFromParam(k2ReleaseTimeEditor, "k2ReleaseTime");
    setupTextEditorFromParam(k3ReleaseTimeEditor, "k3ReleaseTime");  setupTextEditorFromParam(k4ReleaseTimeEditor, "k4ReleaseTime");
    setupTextEditorFromParam(k5ReleaseTimeEditor, "k5ReleaseTime");  setupTextEditorFromParam(k6ReleaseTimeEditor, "k6ReleaseTime");
    setupTextEditorFromParam(k7ReleaseTimeEditor, "k7ReleaseTime");  setupTextEditorFromParam(k8ReleaseTimeEditor, "k8ReleaseTime");
    setupTextEditorFromParam(k9ReleaseTimeEditor, "k9ReleaseTime");  setupTextEditorFromParam(k10ReleaseTimeEditor, "k10ReleaseTime");

    // Envelope Amount row
    setupTextEditorFromParam(k1EnvelopeAmountEditor, "k1EnvelopeAmount"); setupTextEditorFromParam(k2EnvelopeAmountEditor, "k2EnvelopeAmount");
    setupTextEditorFromParam(k3EnvelopeAmountEditor, "k3EnvelopeAmount"); setupTextEditorFromParam(k4EnvelopeAmountEditor, "k4EnvelopeAmount");
    setupTextEditorFromParam(k5EnvelopeAmountEditor, "k5EnvelopeAmount"); setupTextEditorFromParam(k6EnvelopeAmountEditor, "k6EnvelopeAmount");
    setupTextEditorFromParam(k7EnvelopeAmountEditor, "k7EnvelopeAmount"); setupTextEditorFromParam(k8EnvelopeAmountEditor, "k8EnvelopeAmount");
    setupTextEditorFromParam(k9EnvelopeAmountEditor, "k9EnvelopeAmount"); setupTextEditorFromParam(k10EnvelopeAmountEditor, "k10EnvelopeAmount");

    // LFO Shape row
    setupTextEditorFromParam(k1LFOShapeEditor, "k1LFOShape");        setupTextEditorFromParam(k2LFOShapeEditor, "k2LFOShape");
    setupTextEditorFromParam(k3LFOShapeEditor, "k3LFOShape");        setupTextEditorFromParam(k4LFOShapeEditor, "k4LFOShape");
    setupTextEditorFromParam(k5LFOShapeEditor, "k5LFOShape");        setupTextEditorFromParam(k6LFOShapeEditor, "k6LFOShape");
    setupTextEditorFromParam(k7LFOShapeEditor, "k7LFOShape");        setupTextEditorFromParam(k8LFOShapeEditor, "k8LFOShape");
    setupTextEditorFromParam(k9LFOShapeEditor, "k9LFOShape");        setupTextEditorFromParam(k10LFOShapeEditor, "k10LFOShape");

    // LFO Rate row
    setupTextEditorFromParam(k1LFORateEditor, "k1LFORate");       setupTextEditorFromParam(k2LFORateEditor, "k2LFORate");
    setupTextEditorFromParam(k3LFORateEditor, "k3LFORate");       setupTextEditorFromParam(k4LFORateEditor, "k4LFORate");
    setupTextEditorFromParam(k5LFORateEditor, "k5LFORate");       setupTextEditorFromParam(k6LFORateEditor, "k6LFORate");
    setupTextEditorFromParam(k7LFORateEditor, "k7LFORate");       setupTextEditorFromParam(k8LFORateEditor, "k8LFORate");
    setupTextEditorFromParam(k9LFORateEditor, "k9LFORate");       setupTextEditorFromParam(k10LFORateEditor, "k10LFORate");

    // LFO Amount row
    setupTextEditorFromParam(k1LFOAmountEditor, "k1LFOAmount");     setupTextEditorFromParam(k2LFOAmountEditor, "k2LFOAmount");
    setupTextEditorFromParam(k3LFOAmountEditor, "k3LFOAmount");     setupTextEditorFromParam(k4LFOAmountEditor, "k4LFOAmount");
    setupTextEditorFromParam(k5LFOAmountEditor, "k5LFOAmount");     setupTextEditorFromParam(k6LFOAmountEditor, "k6LFOAmount");
    setupTextEditorFromParam(k7LFOAmountEditor, "k7LFOAmount");     setupTextEditorFromParam(k8LFOAmountEditor, "k8LFOAmount");
    setupTextEditorFromParam(k9LFOAmountEditor, "k9LFOAmount");     setupTextEditorFromParam(k10LFOAmountEditor, "k10LFOAmount");

    // Spectral Multiplier row (NEW)
    setupTextEditorFromParam(f1SpectralMultiplierEditor, "input_f1");     setupTextEditorFromParam(f2SpectralMultiplierEditor, "input_f2");
    setupTextEditorFromParam(f3SpectralMultiplierEditor, "input_f3");     setupTextEditorFromParam(f4SpectralMultiplierEditor, "input_f4");
    setupTextEditorFromParam(f5SpectralMultiplierEditor, "input_f5");     setupTextEditorFromParam(f6SpectralMultiplierEditor, "input_f6");
    setupTextEditorFromParam(f7SpectralMultiplierEditor, "input_f7");     setupTextEditorFromParam(f8SpectralMultiplierEditor, "input_f8");
    setupTextEditorFromParam(f9SpectralMultiplierEditor, "input_f9");     setupTextEditorFromParam(f10SpectralMultiplierEditor, "input_f10");

    // ======================== AMPLITUDE ENVELOPE COLUMN ========================
    setupTextEditorFromParam(ampEnvAttackTimeEditor, "ampEnvAttackTime");
    setupTextEditorFromParam(ampEnvDecayTimeEditor, "ampEnvDecayTime");
    setupTextEditorFromParam(ampEnvSustainLevelEditor, "ampEnvSustainLevel");
    setupTextEditorFromParam(ampEnvReleaseTimeEditor, "ampEnvReleaseTime");

    // ======================== VELOCITY SCALING CONTROLS ========================
    setupTextEditorFromParam(velToAmplitudeEditor, "velToAmplitude");
    setupTextEditorFromParam(velToBrillianceEditor, "velToBrilliance");
    setupTextEditorFromParam(velToAttackTimeEditor, "velToAttackTime");
    
    //========================= FUNKY PITCH MOD CONTROLS==========================
    setupTextEditorFromParam(vintageAmountEditor, "vintageAmount");
    setupTextEditorFromParam(pitchEnvDistanceEditor, "pitchEnvDistance");
    setupTextEditorFromParam(pitchEnvTimeEditor, "pitchEnvTime");

    velToAmplitudeLabel.setText("Vel¨Amplitude", juce::dontSendNotification);
    velToBrillianceLabel.setText("Vel¨Brilliance", juce::dontSendNotification);
    velToAttackTimeLabel.setText("Vel¨Attack", juce::dontSendNotification);
    vintageAmountLabel.setText("Vintage Amount", juce::dontSendNotification);
    addAndMakeVisible(velToAmplitudeLabel);
    addAndMakeVisible(velToBrillianceLabel);
    addAndMakeVisible(velToAttackTimeLabel);
    addAndMakeVisible(vintageAmountLabel);
    pitchEnvDistanceLabel.setText("Pitch Env Distance", juce::dontSendNotification);
    pitchEnvTimeLabel.setText("Pitch Env Time", juce::dontSendNotification);
    addAndMakeVisible(pitchEnvDistanceLabel);
    addAndMakeVisible(pitchEnvTimeLabel);

    // Add this in the constructor after your other setupTextEditor calls:
    //debugEnvLabel.setText("Envelope: Idle", juce::dontSendNotification);
    //debugEnvLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkblue);
    //debugEnvLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(debugEnvLabel);

    //debugAttackLabel.setText("Attack Time: 0.0", juce::dontSendNotification);
    //debugAttackLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkgreen);
    //debugAttackLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    //addAndMakeVisible(debugAttackLabel);
    
    // At the end of the constructor, add:
    startTimerHz(30); // This belongs in PluginEditor.cpp constructor

    // ======================== GUI PREVIEW BUTTON ========================
    addAndMakeVisible(guiPreviewButton);
    guiPreviewButton.onClick = [this]()
    {
        if (guiPreviewWindow == nullptr)
        {
            guiPreviewWindow = std::make_unique<juce::DocumentWindow>(
                "PLANET GUI Preview",
                juce::Colours::darkgrey,
                juce::DocumentWindow::allButtons);
            
            guiPreviewWindow->setContentOwned(new PLANETMainGui(audioProcessor.parameters), true);
            guiPreviewWindow->setResizable(true, true);
            guiPreviewWindow->centreWithSize(1400, 800);
            guiPreviewWindow->setVisible(true);
        }
        else
        {
            guiPreviewWindow->setVisible(!guiPreviewWindow->isVisible());
        }
    };

    // Version label
    versionLabel.setText("PLANET v0.1.1 - 01 Jan 2026", juce::dontSendNotification);
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(versionLabel);

    
    // Make window larger for the parameter grid (now 9 rows instead of 8)
    setSize(1900, 700);
}

PLANETtest4AudioProcessorEditor::~PLANETtest4AudioProcessorEditor()
{
}

//=====================================================TIMER CALLBACK============================================
void PLANETtest4AudioProcessorEditor::timerCallback()
{
    // Core functionality: Update brilliance slider from mod wheel
    float modWheelValue = audioProcessor.currentModWheelValue.load();
    if (modWheelValue != brillianceSlider.getValue())
    {
        brillianceSlider.setValue(modWheelValue, juce::sendNotification);
    }

    // Call debugging function (easy to disable by commenting out this line)
    debugTimerCallback();
}

//=================================================DEBUGGING TIMER CALLBACK=========================================

void PLANETtest4AudioProcessorEditor::debugTimerCallback()
{
    // All existing debug variable loading
    float attackTime = audioProcessor.debugCurrentAttackTime.load();
    float envLevel = audioProcessor.debugCurrentEnvLevel.load();
    float deltaTime = audioProcessor.debugDeltaTime.load();
    float frequency = audioProcessor.debugCurrentFrequency.load();
    float envTime = audioProcessor.debugEnvTime.load();
    float sampleRate = audioProcessor.debugSampleRate.load();
    int stage = audioProcessor.debugEnvStage.load();
    int updateCount = audioProcessor.debugEnvelopeUpdateCount.load();
    std::string stageNames[] = { "Idle", "Attack", "Decay", "Sustain", "Release" };
    std::string stageName = (stage >= 0 && stage < 5) ? stageNames[stage] : "Unknown";

    // MIDI info
    int lastNoteOn = audioProcessor.debugLastNoteOn.load();
    int lastNoteOff = audioProcessor.debugLastNoteOff.load();
    int activeVoices = audioProcessor.debugActiveVoices.load();
    float vel = audioProcessor.debugVelocity.load();
    float bril = audioProcessor.debugBrilliance.load();
    float velBril = audioProcessor.debugVelocityBrilliance.load();

    int lastCC = audioProcessor.debugLastCC.load();
    int lastCCValue = audioProcessor.debugLastCCValue.load();

    debugAttackLabel.setText("CC#" + juce::String(lastCC) +
        "=" + juce::String(lastCCValue) +
        " MW:" + juce::String(audioProcessor.currentModWheelValue.load(), 2),
        juce::dontSendNotification);
}
    
void PLANETtest4AudioProcessorEditor::paint(juce::Graphics& g)
{
    // Fill background
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.0f));
    g.drawFittedText("PLANET Phase Distortion Synth Prototype", getLocalBounds().removeFromTop(40),
        juce::Justification::centred, 1);

    g.setFont(juce::FontOptions(14.0f));
    g.drawFittedText("Phase Distortion Coefficients", 30, 50, 300, 20,
        juce::Justification::left, 1);

    g.drawFittedText("Modulation Matrix (Per K-Coefficient)", 30, 320, 300, 20,
        juce::Justification::left, 1);
}

//==================RESIZED FUNCTIONS========================================================

void PLANETtest4AudioProcessorEditor::resized()
{
    // Position Brilliance slider in top right corner
    brillianceSlider.setBounds(getWidth() - 140, 10, 120, 120);

    // Position Envelop Exponential Control slider
    exponentialControlSlider.setBounds(getWidth() - 100, getHeight() - 100, 80, 80);

    // ======================== K1-K10 DRAWBARS ========================
    for (int i = 0; i < 10; ++i)
    {
        int x = 30 + i * 130;  // More spacing for text grid below
        int y = 70;
        int w = 80;
        int h = 220;

        switch (i) {
        case 0: k1Slider.setBounds(x, y, w, h); break;
        case 1: k2Slider.setBounds(x, y, w, h); break;
        case 2: k3Slider.setBounds(x, y, w, h); break;
        case 3: k4Slider.setBounds(x, y, w, h); break;
        case 4: k5Slider.setBounds(x, y, w, h); break;
        case 5: k6Slider.setBounds(x, y, w, h); break;
        case 6: k7Slider.setBounds(x, y, w, h); break;
        case 7: k8Slider.setBounds(x, y, w, h); break;
        case 8: k9Slider.setBounds(x, y, w, h); break;
        case 9: k10Slider.setBounds(x, y, w, h); break;
        }
    }

    // K-coefficient labels below sliders
    for (int i = 0; i < 10; ++i)
    {
        int x = 30 + i * 130;
        int y = 310;
        int w = 80;
        int h = 20;

        switch (i) {
        case 0: k1Label.setBounds(x, y, w, h); break;
        case 1: k2Label.setBounds(x, y, w, h); break;
        case 2: k3Label.setBounds(x, y, w, h); break;
        case 3: k4Label.setBounds(x, y, w, h); break;
        case 4: k5Label.setBounds(x, y, w, h); break;
        case 5: k6Label.setBounds(x, y, w, h); break;
        case 6: k7Label.setBounds(x, y, w, h); break;
        case 7: k8Label.setBounds(x, y, w, h); break;
        case 8: k9Label.setBounds(x, y, w, h); break;
        case 9: k10Label.setBounds(x, y, w, h); break;
        }
        // Add to the end of resized():
        debugEnvLabel.setBounds(10, 550, 200, 25);
        debugAttackLabel.setBounds(220, 550, 200, 25);

        vibratoRateSlider.setBounds(getWidth() - 300, 140, 80, 80);
        vibratoDepthSlider.setBounds(getWidth() - 200, 140, 80, 80);
        vibratoFadeInSlider.setBounds(getWidth() - 100, 140, 80, 80);

        // ======================== EFFECTS CONTROLS ========================
        detuneAmountSlider.setBounds(getWidth() - 400, 240, 80, 80);
        detuneMixSlider.setBounds(getWidth() - 300, 240, 80, 80);
        reverbLengthSlider.setBounds(getWidth() - 200, 240, 80, 80);
        reverbMixSlider.setBounds(getWidth() - 100, 240, 80, 80);
        
        // New reverb controls in a second row
        reverbDampingSlider.setBounds(getWidth() - 300, 340, 80, 80);
        reverbWidthSlider.setBounds(getWidth() - 200, 340, 80, 80);



    }

    // ======================== PARAMETER GRID (9 rows ~ 10 columns) ========================
    const int gridStartY = 350;
    const int rowHeight = 25;
    const int colWidth = 80;
    const int labelWidth = 120;

    // Row labels (left side)
    attackTimeRowLabel.setBounds(10, gridStartY + 0 * rowHeight, labelWidth, rowHeight);
    decayTimeRowLabel.setBounds(10, gridStartY + 1 * rowHeight, labelWidth, rowHeight);
    sustainLevelRowLabel.setBounds(10, gridStartY + 2 * rowHeight, labelWidth, rowHeight);
    releaseTimeRowLabel.setBounds(10, gridStartY + 3 * rowHeight, labelWidth, rowHeight);
    envelopeAmountRowLabel.setBounds(10, gridStartY + 4 * rowHeight, labelWidth, rowHeight);
    lfoShapeRowLabel.setBounds(10, gridStartY + 5 * rowHeight, labelWidth, rowHeight);
    lfoRateRowLabel.setBounds(10, gridStartY + 6 * rowHeight, labelWidth, rowHeight);
    lfoAmountRowLabel.setBounds(10, gridStartY + 7 * rowHeight, labelWidth, rowHeight);
    spectralMultiplierRowLabel.setBounds(10, gridStartY + 8 * rowHeight, labelWidth, rowHeight);  // NEW

    // Attack Time row
    std::array<juce::Label*, 10> attackTimeEditors = { &k1AttackTimeEditor, &k2AttackTimeEditor, &k3AttackTimeEditor, &k4AttackTimeEditor, &k5AttackTimeEditor,
                                                       &k6AttackTimeEditor, &k7AttackTimeEditor, &k8AttackTimeEditor, &k9AttackTimeEditor, &k10AttackTimeEditor };

    // Decay Time row
    std::array<juce::Label*, 10> decayTimeEditors = { &k1DecayTimeEditor, &k2DecayTimeEditor, &k3DecayTimeEditor, &k4DecayTimeEditor, &k5DecayTimeEditor,
                                                      &k6DecayTimeEditor, &k7DecayTimeEditor, &k8DecayTimeEditor, &k9DecayTimeEditor, &k10DecayTimeEditor };

    // Sustain Level row
    std::array<juce::Label*, 10> sustainLevelEditors = { &k1SustainLevelEditor, &k2SustainLevelEditor, &k3SustainLevelEditor, &k4SustainLevelEditor, &k5SustainLevelEditor,
                                                         &k6SustainLevelEditor, &k7SustainLevelEditor, &k8SustainLevelEditor, &k9SustainLevelEditor, &k10SustainLevelEditor };

    // Release Time row
    std::array<juce::Label*, 10> releaseTimeEditors = { &k1ReleaseTimeEditor, &k2ReleaseTimeEditor, &k3ReleaseTimeEditor, &k4ReleaseTimeEditor, &k5ReleaseTimeEditor,
                                                        &k6ReleaseTimeEditor, &k7ReleaseTimeEditor, &k8ReleaseTimeEditor, &k9ReleaseTimeEditor, &k10ReleaseTimeEditor };

    // Envelope Amount row
    std::array<juce::Label*, 10> envelopeAmountEditors = { &k1EnvelopeAmountEditor, &k2EnvelopeAmountEditor, &k3EnvelopeAmountEditor, &k4EnvelopeAmountEditor, &k5EnvelopeAmountEditor,
                                                           &k6EnvelopeAmountEditor, &k7EnvelopeAmountEditor, &k8EnvelopeAmountEditor, &k9EnvelopeAmountEditor, &k10EnvelopeAmountEditor };

    // LFO Shape row
    std::array<juce::Label*, 10> lfoShapeEditors = { &k1LFOShapeEditor, &k2LFOShapeEditor, &k3LFOShapeEditor, &k4LFOShapeEditor, &k5LFOShapeEditor,
                                                     &k6LFOShapeEditor, &k7LFOShapeEditor, &k8LFOShapeEditor, &k9LFOShapeEditor, &k10LFOShapeEditor };

    // LFO Rate row
    std::array<juce::Label*, 10> lfoRateEditors = { &k1LFORateEditor, &k2LFORateEditor, &k3LFORateEditor, &k4LFORateEditor, &k5LFORateEditor,
                                                    &k6LFORateEditor, &k7LFORateEditor, &k8LFORateEditor, &k9LFORateEditor, &k10LFORateEditor };

    // LFO Amount row
    std::array<juce::Label*, 10> lfoAmountEditors = { &k1LFOAmountEditor, &k2LFOAmountEditor, &k3LFOAmountEditor, &k4LFOAmountEditor, &k5LFOAmountEditor,
                                                      &k6LFOAmountEditor, &k7LFOAmountEditor, &k8LFOAmountEditor, &k9LFOAmountEditor, &k10LFOAmountEditor };

    // Spectral Multiplier row (NEW)
    std::array<juce::Label*, 10> spectralMultiplierEditors = { &f1SpectralMultiplierEditor, &f2SpectralMultiplierEditor, &f3SpectralMultiplierEditor, &f4SpectralMultiplierEditor, &f5SpectralMultiplierEditor,
                                                               &f6SpectralMultiplierEditor, &f7SpectralMultiplierEditor, &f8SpectralMultiplierEditor, &f9SpectralMultiplierEditor, &f10SpectralMultiplierEditor };

    // Position all grid elements
    for (int col = 0; col < 10; ++col)
    {
        int x = 140 + col * 130;  // Align with K-sliders above

        attackTimeEditors[col]->setBounds(x, gridStartY + 0 * rowHeight, colWidth, rowHeight);
        decayTimeEditors[col]->setBounds(x, gridStartY + 1 * rowHeight, colWidth, rowHeight);
        sustainLevelEditors[col]->setBounds(x, gridStartY + 2 * rowHeight, colWidth, rowHeight);
        releaseTimeEditors[col]->setBounds(x, gridStartY + 3 * rowHeight, colWidth, rowHeight);
        envelopeAmountEditors[col]->setBounds(x, gridStartY + 4 * rowHeight, colWidth, rowHeight);
        lfoShapeEditors[col]->setBounds(x, gridStartY + 5 * rowHeight, colWidth, rowHeight);
        lfoRateEditors[col]->setBounds(x, gridStartY + 6 * rowHeight, colWidth, rowHeight);
        lfoAmountEditors[col]->setBounds(x, gridStartY + 7 * rowHeight, colWidth, rowHeight);
        spectralMultiplierEditors[col]->setBounds(x, gridStartY + 8 * rowHeight, colWidth, rowHeight);  // NEW
    }

    // ======================== AMPLITUDE ENVELOPE COLUMN (11th column) ========================
    int ampEnvX = 140 + 10 * 130;  // Position after K10 column
    ampEnvAttackTimeEditor.setBounds(ampEnvX, gridStartY + 0 * rowHeight, colWidth, rowHeight);
    ampEnvDecayTimeEditor.setBounds(ampEnvX, gridStartY + 1 * rowHeight, colWidth, rowHeight);
    ampEnvSustainLevelEditor.setBounds(ampEnvX, gridStartY + 2 * rowHeight, colWidth, rowHeight);
    ampEnvReleaseTimeEditor.setBounds(ampEnvX, gridStartY + 3 * rowHeight, colWidth, rowHeight);
    // Note: rows 4-7 are empty for amp envelope (no LFO parameters)
    // ======================== VELOCITY SCALING CONTROLS (Below main grid) ========================
    int velControlsStartY = gridStartY + 9 * rowHeight + 40;  // Below the parameter grid with some spacing
    int velControlWidth = 80;
    int velControlHeight = 25;

    velToAmplitudeEditor.setBounds(140, velControlsStartY, velControlWidth, velControlHeight);
    velToBrillianceEditor.setBounds(140 + 130, velControlsStartY, velControlWidth, velControlHeight);
    velToAttackTimeEditor.setBounds(140 + 260, velControlsStartY, velControlWidth, velControlHeight);
    vintageAmountEditor.setBounds(140 + 390, velControlsStartY, velControlWidth, velControlHeight);

    // Position labels above the editors
    velToAmplitudeLabel.setBounds(140, velControlsStartY - 20, velControlWidth, 20);
    velToBrillianceLabel.setBounds(140 + 130, velControlsStartY - 20, velControlWidth, 20);
    velToAttackTimeLabel.setBounds(140 + 260, velControlsStartY - 20, velControlWidth, 20);
    vintageAmountLabel.setBounds(140 + 390, velControlsStartY - 20, velControlWidth, 20);
    pitchEnvDistanceEditor.setBounds(140 + 520, velControlsStartY, velControlWidth, velControlHeight);
    pitchEnvTimeEditor.setBounds(140 + 650, velControlsStartY, velControlWidth, velControlHeight);
    pitchEnvDistanceLabel.setBounds(140 + 520, velControlsStartY - 20, velControlWidth, 20);
    pitchEnvTimeLabel.setBounds(140 + 650, velControlsStartY - 20, velControlWidth, 20);

    // Version label - bottom right corner
    versionLabel.setBounds(getWidth() - 220, getHeight() - 25, 210, 20);

    // GUI Preview button - bottom right corner
    guiPreviewButton.setBounds(getWidth() - 100, getHeight() - 50, 80, 30);

}
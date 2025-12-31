/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PLANETMainGui.h"

//==============================================================================
/**
*/
class PLANETtest4AudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    PLANETtest4AudioProcessorEditor(PLANETtest4AudioProcessor&);
    ~PLANETtest4AudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PLANETtest4AudioProcessor& audioProcessor;

    juce::Slider vibratoRateSlider, vibratoDepthSlider, vibratoFadeInSlider;
    juce::Label vibratoRateLabel, vibratoDepthLabel, vibratoFadeInLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vibratoRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vibratoDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vibratoFadeInAttachment;

    // ======================== EFFECTS CONTROLS ========================
    juce::Slider detuneAmountSlider, detuneMixSlider;
    juce::Slider reverbLengthSlider, reverbMixSlider;
    juce::Slider reverbDampingSlider, reverbWidthSlider;  // NEW

    juce::Label detuneAmountLabel, detuneMixLabel;
    juce::Label reverbLengthLabel, reverbMixLabel;
    juce::Label reverbDampingLabel, reverbWidthLabel;      // NEW

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> detuneAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> detuneMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbLengthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbDampingAttachment;  // NEW
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbWidthAttachment;    // NEW



    //=========================Debug Tools========================================
    juce::Label debugEnvLabel;
    juce::Label debugAttackLabel;
    // Add to private section:
    void timerCallback() override;
    void debugTimerCallback();  // Separate debugging function

    // Original morph slider (keep for comparison)
    juce::Slider brillianceSlider;
    juce::Label brillianceLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> brillianceAttachment;

    // ======================== K-COEFFICIENT SLIDERS ONLY ========================
    juce::Slider k1Slider, k2Slider, k3Slider, k4Slider, k5Slider;
    juce::Slider k6Slider, k7Slider, k8Slider, k9Slider, k10Slider;

    // Labels for the K sliders
    juce::Label k1Label, k2Label, k3Label, k4Label, k5Label;
    juce::Label k6Label, k7Label, k8Label, k9Label, k10Label;

    // Attachments for the K sliders
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> k1Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> k2Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> k3Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> k4Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> k5Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> k6Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> k7Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> k8Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> k9Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> k10Attachment;

    // ======================== TEXT INPUT GRID (8 rows ~ 10 columns = 80 fields) ========================

// Row Labels for the parameter grid
    juce::Label attackTimeRowLabel, decayTimeRowLabel, sustainLevelRowLabel, releaseTimeRowLabel;
    juce::Label envelopeAmountRowLabel, lfoShapeRowLabel, lfoRateRowLabel, lfoAmountRowLabel;
    juce::Label spectralMultiplierRowLabel;  

    // Attack Time (10 editable text fields)
    juce::Label k1AttackTimeEditor, k2AttackTimeEditor, k3AttackTimeEditor, k4AttackTimeEditor, k5AttackTimeEditor;
    juce::Label k6AttackTimeEditor, k7AttackTimeEditor, k8AttackTimeEditor, k9AttackTimeEditor, k10AttackTimeEditor;

    // Decay Time (10 editable text fields)
    juce::Label k1DecayTimeEditor, k2DecayTimeEditor, k3DecayTimeEditor, k4DecayTimeEditor, k5DecayTimeEditor;
    juce::Label k6DecayTimeEditor, k7DecayTimeEditor, k8DecayTimeEditor, k9DecayTimeEditor, k10DecayTimeEditor;

    // Sustain Level (10 editable text fields)
    juce::Label k1SustainLevelEditor, k2SustainLevelEditor, k3SustainLevelEditor, k4SustainLevelEditor, k5SustainLevelEditor;
    juce::Label k6SustainLevelEditor, k7SustainLevelEditor, k8SustainLevelEditor, k9SustainLevelEditor, k10SustainLevelEditor;

    // Release Time (10 editable text fields)
    juce::Label k1ReleaseTimeEditor, k2ReleaseTimeEditor, k3ReleaseTimeEditor, k4ReleaseTimeEditor, k5ReleaseTimeEditor;
    juce::Label k6ReleaseTimeEditor, k7ReleaseTimeEditor, k8ReleaseTimeEditor, k9ReleaseTimeEditor, k10ReleaseTimeEditor;

    // Envelope Amount (10 editable text fields)
    juce::Label k1EnvelopeAmountEditor, k2EnvelopeAmountEditor, k3EnvelopeAmountEditor, k4EnvelopeAmountEditor, k5EnvelopeAmountEditor;
    juce::Label k6EnvelopeAmountEditor, k7EnvelopeAmountEditor, k8EnvelopeAmountEditor, k9EnvelopeAmountEditor, k10EnvelopeAmountEditor;

    // LFO Shape (10 editable text fields - values 1, 2, or 3)
    juce::Label k1LFOShapeEditor, k2LFOShapeEditor, k3LFOShapeEditor, k4LFOShapeEditor, k5LFOShapeEditor;
    juce::Label k6LFOShapeEditor, k7LFOShapeEditor, k8LFOShapeEditor, k9LFOShapeEditor, k10LFOShapeEditor;

    // LFO Rate (10 editable text fields - REPLACE existing sliders)
    juce::Label k1LFORateEditor, k2LFORateEditor, k3LFORateEditor, k4LFORateEditor, k5LFORateEditor;
    juce::Label k6LFORateEditor, k7LFORateEditor, k8LFORateEditor, k9LFORateEditor, k10LFORateEditor;

    // LFO Amount (10 editable text fields - REPLACE existing sliders)
    juce::Label k1LFOAmountEditor, k2LFOAmountEditor, k3LFOAmountEditor, k4LFOAmountEditor, k5LFOAmountEditor;
    juce::Label k6LFOAmountEditor, k7LFOAmountEditor, k8LFOAmountEditor, k9LFOAmountEditor, k10LFOAmountEditor;

    // Spectral Multipliers (10 editable text fields - NEW)
    juce::Label f1SpectralMultiplierEditor, f2SpectralMultiplierEditor, f3SpectralMultiplierEditor, f4SpectralMultiplierEditor, f5SpectralMultiplierEditor;
    juce::Label f6SpectralMultiplierEditor, f7SpectralMultiplierEditor, f8SpectralMultiplierEditor, f9SpectralMultiplierEditor, f10SpectralMultiplierEditor;

    // Envelope Exponential Control
    juce::Slider exponentialControlSlider;
    juce::Label exponentialControlLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> exponentialControlAttachment;

    // ======================== VELOCITY SCALING CONTROLS ========================
    juce::Label velToAmplitudeEditor, velToBrillianceEditor, velToAttackTimeEditor, vintageAmountEditor;
    juce::Label velToAmplitudeLabel, velToBrillianceLabel, velToAttackTimeLabel, vintageAmountLabel;
    juce::Label pitchEnvDistanceEditor, pitchEnvTimeEditor;
    juce::Label pitchEnvDistanceLabel, pitchEnvTimeLabel;

    // ======================== AMPLITUDE ENVELOPE COLUMN (4 editable text fields) ========================
    juce::Label ampEnvAttackTimeEditor, ampEnvDecayTimeEditor, ampEnvSustainLevelEditor, ampEnvReleaseTimeEditor;

    // ======================== GUI PREVIEW BUTTON ========================
    juce::TextButton guiPreviewButton { "New GUI" };
    std::unique_ptr<juce::DocumentWindow> guiPreviewWindow;

    // Version display
    juce::Label versionLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PLANETtest4AudioProcessorEditor)
};
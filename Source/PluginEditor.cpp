/*
  ==============================================================================
    PLANET Plugin Editor - Clean User Interface
    Dev GUI archived in PluginEditor_DevMode.cpp.archive
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

PLANETtest4AudioProcessorEditor::PLANETtest4AudioProcessorEditor(PLANETtest4AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Create the main GUI
    mainGui = std::make_unique<PLANETMainGui>(audioProcessor.parameters,
        &audioProcessor,
        &audioProcessor.currentModWheelValue,
        &audioProcessor.waveformSnapshot,
        &audioProcessor.waveformSnapshotLength,
        &audioProcessor.waveformSnapshotReady,
        &audioProcessor.waveformSnapshotRequest,
        &audioProcessor.waveformActive);

    addAndMakeVisible(*mainGui);

    // Version label
    versionLabel.setText("PLANET v0.4.0 - 14 Jan 2026", juce::dontSendNotification);
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::grey.withAlpha(0.6f));
    versionLabel.setJustificationType(juce::Justification::centredRight);
    versionLabel.setFont(juce::Font(11.0f));
    addAndMakeVisible(versionLabel);

    setSize(1400, 800);
    setResizable(true, true);
}

PLANETtest4AudioProcessorEditor::~PLANETtest4AudioProcessorEditor()
{
}

void PLANETtest4AudioProcessorEditor::paint(juce::Graphics& g)
{
    // Main GUI handles its own painting
}

void PLANETtest4AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    mainGui->setBounds(bounds);

    // Version label in bottom-right of patch bar area
    versionLabel.setBounds(bounds.getWidth() - 180, bounds.getHeight() - 35, 170, 20);
}
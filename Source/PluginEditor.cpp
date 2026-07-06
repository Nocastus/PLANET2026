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
        &audioProcessor.rawModWheelValue,
        &audioProcessor.modWheelEngaged,
        &audioProcessor.waveformSnapshot,
        &audioProcessor.waveformSnapshotLength,
        &audioProcessor.waveformSnapshotReady,
        &audioProcessor.waveformSnapshotRequest,
        &audioProcessor.waveformActive,
        &audioProcessor.displayBPM,
        &audioProcessor.transportPlaying,
        &audioProcessor.effectiveBrillianceDisplay,
        &audioProcessor.effectiveCarrierMorphDisplay,
        &audioProcessor.outputPeak);

    addAndMakeVisible(*mainGui);

    // Wire the LIFE voicing dev panel to the processor's tunable constants
    mainGui->setLifeVoicingParams(&audioProcessor.lifeVoicing);

    // (The version string is drawn by PLANETMainGui in the patch bar - no separate label here.)

    setSize(1500, 850);
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
}
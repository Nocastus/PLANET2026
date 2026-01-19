/*
  ==============================================================================
    PLANET Plugin Editor - Clean User Interface
    Dev GUI archived in PluginEditor_DevMode.cpp.archive
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PLANETMainGui.h"

class PLANETtest4AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    PLANETtest4AudioProcessorEditor(PLANETtest4AudioProcessor&);
    ~PLANETtest4AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    PLANETMainGui* getMainGui() { return mainGui.get(); }

private:
    PLANETtest4AudioProcessor& audioProcessor;

    std::unique_ptr<PLANETMainGui> mainGui;

    // Version display
    juce::Label versionLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PLANETtest4AudioProcessorEditor)
};
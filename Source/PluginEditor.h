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

    // The GUI is laid out at exactly this size and then scaled as a whole (see resized()).
    // Every layout constant in PLANETMainGui is in these coordinates - nothing re-flows.
    static constexpr int   baseWidth  = 1500;
    static constexpr int   baseHeight = 850;
    static constexpr float minScale   = 0.5f;   // 750 x 425 - the floor where text is still readable
    static constexpr float maxScale   = 1.0f;   // never bigger than the designed size

private:
    // Largest whole-GUI scale that leaves the window fully on the primary display.
    static float scaleToFitScreen();

    PLANETtest4AudioProcessor& audioProcessor;

    std::unique_ptr<PLANETMainGui> mainGui;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PLANETtest4AudioProcessorEditor)
};
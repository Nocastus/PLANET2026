/*
  ==============================================================================
    PLANETMainGui.h - User-Friendly GUI for PLANET Synthesizer
    Mockup Phase - No parameter binding yet
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PLANETMainGui : public juce::Component
{
public:
    PLANETMainGui();
    ~PLANETMainGui() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // Colour scheme
    juce::Colour backgroundLight { 0xff2a2a2a };   // Per-harmonic zone
    juce::Colour backgroundGlobal { 0xff1a1a2e };  // Global controls zone
    juce::Colour accentColour { 0xff4a9eff };      // Highlights

    // Layout proportions
    static constexpr float leftWidthRatio = 0.67f;   // Left 2/3
    static constexpr float rightWidthRatio = 0.33f;  // Right 1/3
    static constexpr int patchBarHeight = 40;
    static constexpr int drawbarSectionHeight = 200;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PLANETMainGui)
};

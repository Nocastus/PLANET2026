/*
  ==============================================================================
    PLANETMainGui.cpp - User-Friendly GUI for PLANET Synthesizer
    Mockup Phase - No parameter binding yet
  ==============================================================================
*/

#include "PLANETMainGui.h"

PLANETMainGui::PLANETMainGui()
{
    setSize(1200, 700);
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
    
    // Harmonic section (per-harmonic - light background)
    g.fillRect(0, drawbarSectionHeight, leftWidth, harmonicHeight);
    
    // Amplitude section (global - tinted background)
    g.setColour(backgroundGlobal);
    g.fillRect(0, drawbarSectionHeight + harmonicHeight, leftWidth, ampHeight);

    // ======================== RIGHT SIDE ========================
    
    // Entire right column is global (tinted background)
    g.setColour(backgroundGlobal);
    g.fillRect(leftWidth, 0, rightWidth, mainHeight);

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
    
    // Right side labels
    int rightX = leftWidth;
    int sectionHeight = mainHeight / 5;
    g.drawText("VIBRATO", rightX, 0, rightWidth, sectionHeight, 
               juce::Justification::centred);
    g.drawText("PITCH", rightX, sectionHeight, rightWidth, sectionHeight, 
               juce::Justification::centred);
    g.drawText("BRILLIANCE / ENV LAW", rightX, sectionHeight * 2, rightWidth, sectionHeight, 
               juce::Justification::centred);
    g.drawText("EFFECTS", rightX, sectionHeight * 3, rightWidth, sectionHeight * 2, 
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
    
    // Horizontal dividers on right
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine(sectionHeight * i, (float)leftWidth, (float)bounds.getWidth());
    
    // Patch bar divider
    g.drawHorizontalLine(mainHeight, 0, (float)bounds.getWidth());
}

void PLANETMainGui::resized()
{
    // Component positioning will go here as we add controls
}

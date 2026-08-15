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

    // Whole-GUI zoom. The layout is fixed at baseWidth x baseHeight and the window shows it
    // through a uniform scale, so a smaller window is the same GUI shrunk, never a squashed
    // or clipped one. Dragging the corner re-zooms (aspect ratio is locked); on open we pick
    // the largest zoom that fits the screen, so laptops get a GUI that fits without any fiddling.
    setResizable(true, true);

    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio((double) baseWidth / (double) baseHeight);
        constrainer->setSizeLimits(juce::roundToInt(baseWidth  * minScale),
                                   juce::roundToInt(baseHeight * minScale),
                                   juce::roundToInt(baseWidth  * maxScale),
                                   juce::roundToInt(baseHeight * maxScale));
    }

    const float scale = scaleToFitScreen();
    setSize(juce::roundToInt(baseWidth * scale), juce::roundToInt(baseHeight * scale));
}

float PLANETtest4AudioProcessorEditor::scaleToFitScreen()
{
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        // userArea already excludes the taskbar; the margins leave room for the host's own
        // plugin-window chrome (title bar, frame, and Cubase's toolbar strip above the editor).
        const auto area = display->userArea;
        const float fitW = (float) (area.getWidth()  - 40) / (float) baseWidth;
        const float fitH = (float) (area.getHeight() - 120) / (float) baseHeight;

        return juce::jlimit(minScale, maxScale, juce::jmin(fitW, fitH));
    }

    return maxScale;
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
    // The GUI keeps its designed bounds and the transform does the shrinking. JUCE maps mouse
    // events through the transform, so hit-testing on the drawbar switches and drag zones
    // stays correct at any zoom.
    const float scale = (float) getWidth() / (float) baseWidth;

    mainGui->setTransform(juce::AffineTransform::scale(scale));
    mainGui->setBounds(0, 0, baseWidth, baseHeight);
}
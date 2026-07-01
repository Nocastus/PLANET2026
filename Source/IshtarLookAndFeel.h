/*
  ==============================================================================
    IshtarLookAndFeel.h
    Custom rotary knob with Star of Ishtar design and orbiting indicator
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

// Shared "tempo-synced LFO" accent (amber). Free-running = white. Used by both the drawbar
// ring (DrawbarLookAndFeel) and the LFO-speed knob's centre dot, so the two stay in step.
static constexpr juce::uint32 kLfoSyncColour = 0xffffb04a;

class IshtarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    //==========================================================================
    // DESIGN CONSTANTS - Adjust these to tweak the appearance
    //==========================================================================

    // Star geometry
    static constexpr float INNER_CIRCLE_RATIO = 0.4f;      // Central circle radius as fraction of the orbit
    static constexpr float RAY_TIP_RATIO = 0.78f;          // Cardinal (N/S/E/W) ray-tip radius as fraction of the orbit
    static constexpr float RAY_TIP_RATIO_DIAG = 0.58f;     // Diagonal ray-tip radius — shorter than cardinals to break the uniform-spoke "settings cog" look
    static constexpr float RAY_BASE_WIDTH_DEG = 20.0f;     // 20 for reticule, 45 for classic

    // Line weights (at 80px size, scales proportionally)
    static constexpr float INNER_STROKE = 4.0f;
    static constexpr float RAY_STROKE = 4.0f;

    // Opacity
    static constexpr float STAR_OPACITY = 1.0f;

    // Comet-tail value indicator — ISHTAR's signature. A trailing arc swept from the
    // start of travel to the current value, fading transparent -> accent toward the head
    // (where the dot sits). This REPLACES PLANET2026's full orbit ring, so the two builds
    // are tellable at a glance: comet-tails = ISHTAR, full orbit rings = frozen PLANET2026.
    static constexpr float TAIL_STROKE       = 2.6f;       // Tail thickness at 80px
    static constexpr float TAIL_HEAD_OPACITY = 0.95f;      // Opacity at the head (by the dot)
    static constexpr int   TAIL_SEGMENTS     = 72;         // Subdivisions across the full ARC_SPAN_DEG

    // Orbiting indicator
    static constexpr float INDICATOR_SIZE = 4.0f;
    static constexpr float GLOW_RADIUS = 13.0f;
    static constexpr float GLOW_OPACITY = 0.75f;
    static constexpr float ORBIT_OFFSET = -1.0f;           // Negative = inside orbit circle

    // Knob rotation range
    static constexpr float START_ANGLE_DEG = 135.0f;       // Where 0% sits (degrees from top)
    static constexpr float ARC_SPAN_DEG = 270.0f;          // Total rotation range

    // Colours
    juce::Colour starColour{ 0xff6ab0ff };                // Pale blue accent
    juce::Colour indicatorColour{ 0xffffffff };           // White

    //==========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    //==========================================================================
    IshtarLookAndFeel() = default;
    ~IshtarLookAndFeel() override = default;

    //==========================================================================
    // ROTARY SLIDER OVERRIDE
    //==========================================================================
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPosProportional, float rotaryStartAngle,
        float rotaryEndAngle, juce::Slider& slider) override;

private:
    void drawIshtarStar(juce::Graphics& g, float centreX, float centreY,
        float outerRadius, float scale);

    void drawCometTail(juce::Graphics& g, float centreX, float centreY,
        float orbitRadius, float originAngle, float headAngle, float scale);

    void drawOrbitingIndicator(juce::Graphics& g, float centreX, float centreY,
        float orbitRadius, float angle, float scale);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IshtarLookAndFeel)
};
/*
  ==============================================================================
    IshtarLookAndFeel.cpp
    Custom rotary knob with Star of Ishtar design and orbiting indicator
  ==============================================================================
*/

#include "IshtarLookAndFeel.h"

void IshtarLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosProportional, float /*rotaryStartAngle*/,
    float /*rotaryEndAngle*/, juce::Slider& slider)
{
    // Calculate dimensions
    float size = (float)juce::jmin(width, height);
    float centreX = x + width * 0.5f;
    float centreY = y + height * 0.5f;
    float outerRadius = (size * 0.5f) - INDICATOR_SIZE - 2.0f;
    float scale = size / 80.0f;  // Scale factor relative to 80px reference size

    // Draw the star
    drawIshtarStar(g, centreX, centreY, outerRadius, scale);

    // LFO-rate "ping" dot filling the star's central hole. Only the LFO-speed knob sets
    // "lfoActive" (via slider properties from the GUI), so every other star knob is untouched.
    // Absent when the LFO is off; pulses white when free-running, amber when tempo-synced.
    if ((bool) slider.getProperties().getWithDefault("lfoActive", false))
    {
        const float brightness  = (float) slider.getProperties().getWithDefault("lfoPulse", 1.0f);
        const bool  synced      = (bool)  slider.getProperties().getWithDefault("lfoSynced", false);
        const float innerRadius = outerRadius * INNER_CIRCLE_RATIO;
        const float dotRadius   = innerRadius - (INNER_STROKE * scale) * 0.5f;  // fill the hole up to the ring
        if (dotRadius > 0.0f)
        {
            g.setColour((synced ? juce::Colour(kLfoSyncColour) : juce::Colours::white).withAlpha(brightness));
            g.fillEllipse(centreX - dotRadius, centreY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
        }
    }

    // Calculate indicator angle
    float startAngleRad = juce::degreesToRadians(START_ANGLE_DEG);
    float arcSpanRad = juce::degreesToRadians(ARC_SPAN_DEG);
    float indicatorAngle = startAngleRad + (sliderPosProportional * arcSpanRad);

    // Tail origin: unipolar knobs trail from the start of travel; bipolar knobs (range
    // straddles zero, e.g. Pitch Distance, Vel to Drawbar, drawbar LFO Depth) trail from
    // the value-0 position (centre-top) and grow in whichever direction the value moves.
    float originProportional = 0.0f;
    if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        originProportional = (float)slider.valueToProportionOfLength(0.0);
    float originAngle = startAngleRad + (originProportional * arcSpanRad);

    // Draw the comet-tail value arc (origin -> current value), then the dot at its head
    float indicatorOrbitRadius = outerRadius + (ORBIT_OFFSET * scale);
    drawCometTail(g, centreX, centreY, indicatorOrbitRadius, originAngle, indicatorAngle, scale);
    drawOrbitingIndicator(g, centreX, centreY, indicatorOrbitRadius, indicatorAngle, scale);
}

void IshtarLookAndFeel::drawIshtarStar(juce::Graphics& g, float centreX, float centreY,
    float outerRadius, float scale)
{
    // The orbit ring sits at outerRadius. The central circle and the ray tips are now
    // sized independently of each other (both as fractions of the orbit): the circle is
    // restored to its full size, while the rays are kept short so they clear the ring.
    float innerRadius  = outerRadius * INNER_CIRCLE_RATIO;

    // Scale stroke widths
    float innerStroke = INNER_STROKE * scale;
    float rayStroke = RAY_STROKE * scale;

    // (No full orbit ring on ISHTAR — the comet-tail value arc, drawn in drawCometTail,
    //  now traces the value along outerRadius. The full ring is PLANET2026's tell.)

    // Draw inner circle
    g.setColour(starColour.withAlpha(STAR_OPACITY));
    g.drawEllipse(centreX - innerRadius, centreY - innerRadius,
        innerRadius * 2.0f, innerRadius * 2.0f, innerStroke);

    // Draw 8 triangular rays
    float rayBaseRad = juce::degreesToRadians(RAY_BASE_WIDTH_DEG);
    float halfBase = rayBaseRad * 0.5f;

    for (int i = 0; i < 8; ++i)
    {
        float angle = i * juce::MathConstants<float>::pi / 4.0f;  // 0, 45, 90... degrees

        // Cardinals (i even: N/S/E/W) keep the full length; diagonals (i odd) are shorter so
        // the eight spokes aren't uniform — pulls the star away from a "settings cog" read.
        float rayTipRadius = outerRadius * ((i % 2 == 0) ? RAY_TIP_RATIO : RAY_TIP_RATIO_DIAG);

        // Apex on the ray-tip radius, inside the orbit ring
        float apexX = centreX + rayTipRadius * std::sin(angle);
        float apexY = centreY - rayTipRadius * std::cos(angle);

        // Base points on inner circle
        float baseLeftX = centreX + innerRadius * std::sin(angle - halfBase);
        float baseLeftY = centreY - innerRadius * std::cos(angle - halfBase);
        float baseRightX = centreX + innerRadius * std::sin(angle + halfBase);
        float baseRightY = centreY - innerRadius * std::cos(angle + halfBase);

        // Draw the ray (two lines from base to apex)
        juce::Path ray;
        ray.startNewSubPath(baseLeftX, baseLeftY);
        ray.lineTo(apexX, apexY);
        ray.lineTo(baseRightX, baseRightY);

        // Curved join rounds the apex so the stroke doesn't throw a mitered spike past
        // the tip (that overhang was the old star-vs-orbit overlap). Rounded ends soften
        // the ray bases too.
        g.setColour(starColour.withAlpha(STAR_OPACITY));
        g.strokePath(ray, juce::PathStrokeType(rayStroke,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void IshtarLookAndFeel::drawCometTail(juce::Graphics& g, float centreX, float centreY,
    float orbitRadius, float originAngle, float headAngle, float scale)
{
    // Sweep from the origin (0% for unipolar, value-0/centre for bipolar) to the current
    // value. headAngle uses the same cos/sin convention as the orbiting dot, so the tail
    // traces exactly the dot's path. sweep may be negative (head left of a bipolar origin).
    const float sweep = headAngle - originAngle;
    if (std::abs(sweep) <= 1.0e-4f)
        return;                                            // nothing to trail at the origin

    const float tailStroke = TAIL_STROKE * scale;
    const float fullSpan   = juce::degreesToRadians(ARC_SPAN_DEG);

    // Keep segment density constant regardless of value, so a short tail is as smooth as a long one.
    const int   segments = juce::jmax(2, (int)std::ceil(TAIL_SEGMENTS * (std::abs(sweep) / fullSpan)));
    const float segAngle = sweep / (float)segments;        // signed: follows the sweep direction

    // Curved/rounded so consecutive segments blend into a continuous arc with no faceting.
    const juce::PathStrokeType stroke(tailStroke,
        juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    for (int i = 0; i < segments; ++i)
    {
        const float a0 = originAngle + segAngle * (float)i;
        const float a1 = a0 + segAngle;

        // Fade transparent at the tail (i=0) -> TAIL_HEAD_OPACITY at the head (the dot).
        const float t     = (float)(i + 1) / (float)segments;
        const float alpha = t * TAIL_HEAD_OPACITY;

        juce::Path seg;
        seg.startNewSubPath(centreX + orbitRadius * std::cos(a0),
                            centreY + orbitRadius * std::sin(a0));
        seg.lineTo(centreX + orbitRadius * std::cos(a1),
                   centreY + orbitRadius * std::sin(a1));

        g.setColour(starColour.withAlpha(alpha));
        g.strokePath(seg, stroke);
    }
}

void IshtarLookAndFeel::drawOrbitingIndicator(juce::Graphics& g, float centreX, float centreY,
    float orbitRadius, float angle, float scale)
{
    // Calculate indicator position
    float indicatorX = centreX + orbitRadius * std::cos(angle);
    float indicatorY = centreY + orbitRadius * std::sin(angle);

    float indicatorSize = INDICATOR_SIZE * scale;
    float glowRadius = GLOW_RADIUS * scale;

    // Draw glow
    if (glowRadius > 0.0f && GLOW_OPACITY > 0.0f)
    {
        juce::ColourGradient gradient(
            indicatorColour.withAlpha(GLOW_OPACITY),
            indicatorX, indicatorY,
            indicatorColour.withAlpha(0.0f),
            indicatorX + glowRadius, indicatorY,
            true);  // Radial gradient

        g.setGradientFill(gradient);
        g.fillEllipse(indicatorX - glowRadius, indicatorY - glowRadius,
            glowRadius * 2.0f, glowRadius * 2.0f);
    }

    // Draw indicator dot
    g.setColour(indicatorColour);
    g.fillEllipse(indicatorX - indicatorSize, indicatorY - indicatorSize,
        indicatorSize * 2.0f, indicatorSize * 2.0f);
}
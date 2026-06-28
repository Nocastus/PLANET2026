/*
  ==============================================================================
    IshtarLookAndFeel.cpp
    Custom rotary knob with Star of Ishtar design and orbiting indicator
  ==============================================================================
*/

#include "IshtarLookAndFeel.h"

void IshtarLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosProportional, float /*rotaryStartAngle*/,
    float /*rotaryEndAngle*/, juce::Slider& /*slider*/)
{
    // Calculate dimensions
    float size = (float)juce::jmin(width, height);
    float centreX = x + width * 0.5f;
    float centreY = y + height * 0.5f;
    float outerRadius = (size * 0.5f) - INDICATOR_SIZE - 2.0f;
    float scale = size / 80.0f;  // Scale factor relative to 80px reference size

    // Draw the star
    drawIshtarStar(g, centreX, centreY, outerRadius, scale);

    // Calculate indicator angle
    float startAngleRad = juce::degreesToRadians(START_ANGLE_DEG);
    float arcSpanRad = juce::degreesToRadians(ARC_SPAN_DEG);
    float indicatorAngle = startAngleRad + (sliderPosProportional * arcSpanRad);

    // Draw the orbiting indicator
    float indicatorOrbitRadius = outerRadius + (ORBIT_OFFSET * scale);
    drawOrbitingIndicator(g, centreX, centreY, indicatorOrbitRadius, indicatorAngle, scale);
}

void IshtarLookAndFeel::drawIshtarStar(juce::Graphics& g, float centreX, float centreY,
    float outerRadius, float scale)
{
    // The orbit ring sits at outerRadius. The central circle and the ray tips are now
    // sized independently of each other (both as fractions of the orbit): the circle is
    // restored to its full size, while the rays are kept short so they clear the ring.
    float innerRadius  = outerRadius * INNER_CIRCLE_RATIO;
    float rayTipRadius = outerRadius * RAY_TIP_RATIO;

    // Scale stroke widths
    float orbitStroke = ORBIT_STROKE * scale;
    float innerStroke = INNER_STROKE * scale;
    float rayStroke = RAY_STROKE * scale;

    // Draw orbit circle (outer) — stays at outerRadius, outside the star points
    g.setColour(starColour.withAlpha(ORBIT_OPACITY));
    g.drawEllipse(centreX - outerRadius, centreY - outerRadius,
        outerRadius * 2.0f, outerRadius * 2.0f, orbitStroke);

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
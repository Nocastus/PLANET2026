/*
  ==============================================================================
    PLANETMainGui.h - User-Friendly GUI for PLANET Synthesizer
    With parameter binding to audio engine
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "WaveformDisplay.h"
#include <array>
#include "IshtarLookAndFeel.h"
#include "PLANETDataStructures.h"


//==============================================================================
// Custom LookAndFeel for drawbar sliders with LFO and VelToHarmonic visual feedback
//==============================================================================
class DrawbarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearVertical)
        {
            auto trackWidth = juce::jmin(6.0f, (float)width * 0.25f);
            auto trackLeft = x + (width - trackWidth) * 0.5f;

            // Draw track
            g.setColour(slider.findColour(juce::Slider::backgroundColourId));
            g.fillRect(juce::Rectangle<float>(trackLeft, (float)y, trackWidth, (float)height));

            // Calculate thumb position
            auto sliderPosProportional = (float)slider.valueToProportionOfLength(slider.getValue());
            auto thumbCenterX = x + width * 0.5f;
            auto thumbCenterY = y + height * (1.0f - sliderPosProportional);
            float thumbRadius = 7.0f;

            // Check modulation states
            bool hasLFO = slider.getProperties()["hasActiveLFO"];
            bool hasVelHarm = slider.getProperties()["hasActiveVelHarm"];

            // LFO-rate pulse brightness [LFO_PULSE_FLOOR..1], set each timer tick by the GUI.
            // Default 1.0 so the ring looks normal if the property is ever unset.
            float lfoPulse = (float) slider.getProperties().getWithDefault("lfoPulse", 1.0f);

            // Ring colour tells free-running from tempo-synced at a glance: white = free, amber = synced.
            bool lfoSynced = slider.getProperties().getWithDefault("lfoSynced", false);
            juce::Colour ringColour = (lfoSynced ? juce::Colour(kLfoSyncColour)
                                                 : juce::Colours::white).withAlpha(lfoPulse);

            if (hasVelHarm)
            {
                // Draw Ishtar star instead of circle (scaled up for visibility)
                float starRadius = thumbRadius * 1.8f;
                drawMiniIshtarStar(g, thumbCenterX, thumbCenterY, starRadius,
                    slider.findColour(juce::Slider::thumbColourId), hasLFO, ringColour);
            }
            else
            {
                // Draw standard circle thumb
                g.setColour(slider.findColour(juce::Slider::thumbColourId));
                g.fillEllipse(thumbCenterX - thumbRadius, thumbCenterY - thumbRadius,
                    thumbRadius * 2, thumbRadius * 2);

                // If LFO is active, draw the ring, its brightness pulsing at the LFO rate.
                if (hasLFO)
                {
                    float outlineRadius = 10.0f;  // Original thumb size, now used just for LFO ring
                    g.setColour(ringColour);
                    g.drawEllipse(thumbCenterX - outlineRadius, thumbCenterY - outlineRadius,
                        outlineRadius * 2, outlineRadius * 2, 3.0f);
                }
            }
        }
        else
        {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                minSliderPos, maxSliderPos, style, slider);
        }
    }

private:
    void drawMiniIshtarStar(juce::Graphics& g, float cx, float cy, float radius,
        juce::Colour fillColour, bool hasLFO, juce::Colour ringColour = juce::Colours::white)
    {
        float outerRadius = radius;           // Ray tips extend to here
        float orbitRadius = radius * 0.6f;    // Orbit ring sits inside the rays
        float innerRadius = radius * 0.4f;

        // Draw outer circle (like orbit) - the LFO ring, pulsing/coloured at the LFO rate
        if (hasLFO)
        {
            g.setColour(ringColour);
            g.drawEllipse(cx - orbitRadius, cy - orbitRadius,
                orbitRadius * 2, orbitRadius * 2, 3.0f);
        }

        // Draw inner filled circle
        g.setColour(fillColour);
        g.fillEllipse(cx - innerRadius, cy - innerRadius,
            innerRadius * 2, innerRadius * 2);

        // Draw 8 rays
        float rayStroke = 1.5f;
        g.setColour(fillColour);

        for (int i = 0; i < 8; ++i)
        {
            float angle = i * juce::MathConstants<float>::pi / 4.0f;

            // Ray from inner circle edge to outer circle edge
            float innerX = cx + innerRadius * std::sin(angle);
            float innerY = cy - innerRadius * std::cos(angle);
            float outerX = cx + outerRadius * std::sin(angle);
            float outerY = cy - outerRadius * std::cos(angle);

            g.drawLine(innerX, innerY, outerX, outerY, rayStroke);
        }
    }
};

//==============================================================================
// ComboBox that never holds keyboard focus, so DAW transport keys (e.g. keypad
// Enter in Cubase) reach the host instead of being swallowed by the combo.
//
// setWantsKeyboardFocus(false) / setMouseClickGrabsKeyboardFocus(false) are NOT
// enough: after the popup closes, ComboBox's popup-finished callback calls
// getAccessibilityHandler()->grabFocus() (see juce_ComboBox.cpp), which puts
// focus back on the combo regardless of those flags. Once focused, ComboBox::
// keyPressed consumes Return by re-opening the popup. We bounce focus away the
// instant it is gained (covers the accessibility grab and every other path), so
// the combo never gets to consume keys. These combos are non-editable and
// mouse-operated, so they never legitimately need focus.
//==============================================================================
class FocuslessComboBox : public juce::ComboBox
{
public:
    FocuslessComboBox()
    {
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
    }

    void focusGained(FocusChangeType) override
    {
        // Don't recurse while the give-away is in flight.
        if (givingAway)
            return;

        const juce::ScopedValueSetter<bool> guard(givingAway, true);
        giveAwayKeyboardFocus();
    }

    // Belt-and-suspenders: if focus ever lands here anyway, don't consume the
    // keys ComboBox normally eats (Return/arrows) — let them propagate to host.
    bool keyPressed(const juce::KeyPress&) override { return false; }
    bool keyStateChanged(bool) override            { return false; }

private:
    bool givingAway = false;
};

//==============================================================================
// A Slider whose double-click TOGGLES rather than only resets: the first
// double-click remembers the current value and returns to the configured
// double-click return value (the standard JUCE behaviour); a double-click on a
// control already AT its return value restores the remembered one. Turns
// "reset to null" into a reversible A/B gesture - hear the patch without this
// control's contribution, double-click again to get it back. Applies wherever
// setDoubleClickReturnValue(true, ...) is set; otherwise behaves as a plain
// Slider. The restore survives across other edits (it is "the value this
// control last held before a double-click reset", not a general undo).
//==============================================================================
class ToggleResetSlider : public juce::Slider
{
public:
    using juce::Slider::Slider;

    // Fine-adjust: hold Ctrl or Shift as the drag STARTS for ~8x finer resolution (the modifier is
    // sampled here at mouse-down). A plain drag restores normal sensitivity. Applies to every knob and
    // the Colour/Master sliders (all ToggleResetSliders). JUCE's default full-scale drag is 250px.
    void mouseDown(const juce::MouseEvent& e) override
    {
        const bool fine = e.mods.isCtrlDown() || e.mods.isShiftDown();
        setMouseDragSensitivity(fine ? 2000 : 250);
        juce::Slider::mouseDown(e);
    }

    // Same fine-adjust modifier on the scroll wheel: a plain scroll keeps JUCE's normal step, Ctrl (or
    // Shift) + scroll nudges by a small fraction of the range per notch (floored to the control's own
    // interval so stepped controls like Life still move a whole unit). Tune the 0.0025 by feel.
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const bool fine = e.mods.isCtrlDown() || e.mods.isShiftDown();
        if (!fine || !isEnabled())
        {
            juce::Slider::mouseWheelMove(e, wheel);   // normal wheel behaviour
            return;
        }

        double amount = (wheel.deltaY != 0.0f) ? (double) wheel.deltaY : (double) wheel.deltaX;
        if (wheel.isReversed) amount = -amount;
        if (amount == 0.0) return;

        double step = (getMaximum() - getMinimum()) * 0.0025;
        if (getInterval() > 0.0) step = juce::jmax(step, getInterval());
        setValue(getValue() + (amount > 0.0 ? step : -step), juce::sendNotificationSync);
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (!isDoubleClickReturnEnabled())
        {
            juce::Slider::mouseDoubleClick(e);
            return;
        }

        const double resetValue = getDoubleClickReturnValue();
        // Tolerance absorbs float->double round-trips through the parameter
        // attachment (e.g. an 0.8 default stored as float), scaled for
        // large-valued controls like Punch Frequency.
        const double tolerance = 1.0e-4 * juce::jmax(1.0, std::abs(resetValue));

        if (std::abs(getValue() - resetValue) > tolerance)
        {
            rememberedValue = getValue();
            hasRemembered = true;
            setValue(resetValue, juce::sendNotificationSync);
        }
        else if (hasRemembered)
        {
            setValue(rememberedValue, juce::sendNotificationSync);
        }
    }

private:
    double rememberedValue = 0.0;
    bool hasRemembered = false;
};

//==============================================================================
// Main GUI Component
//==============================================================================
//==============================================================================
// Credits / About overlay - a full-window panel shown when the ISHTAR wordmark is
// clicked. Draws the embedded background art (Creditsbackground_png) "cover"-scaled
// with the star anchored top-left, then the credits text over the clean navy field.
// Dismissed by a click anywhere or the Esc key.
//==============================================================================
class IshtarCreditsOverlay : public juce::Component
{
public:
    IshtarCreditsOverlay();

    void setFonts(const juce::Font& regular, const juce::Font& semiBold);
    void setVersionText(const juce::String& v);
    void showOverlay();   // make visible, bring to front, take keyboard focus (for Esc)

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    juce::Image  background;
    juce::Font   regularFont, semiBoldFont;
    juce::String versionText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IshtarCreditsOverlay)
};

//==============================================================================
class PLANETMainGui : public juce::Component,
                       public juce::AudioProcessorValueTreeState::Listener,
                       public juce::Timer
{
public:
    PLANETMainGui(juce::AudioProcessorValueTreeState& apvts,
        juce::AudioProcessor* processor = nullptr,
        std::atomic<float>* rawModWheelPtr = nullptr,
        std::atomic<bool>* modWheelEngagedPtr = nullptr,
        std::array<float, 2048>* waveformSnapshotPtr = nullptr,
        std::atomic<int>* snapshotLengthPtr = nullptr,
        std::atomic<bool>* snapshotReadyPtr = nullptr,
        std::atomic<bool>* snapshotRequestPtr = nullptr,
        std::atomic<bool>* waveformActivePtr = nullptr,
        std::atomic<double>* bpmPtr = nullptr,
        std::atomic<bool>* transportPlayingPtr = nullptr,
        std::atomic<float>* effectiveBrilliancePtr = nullptr,
        std::atomic<float>* effectiveCarrierMorphPtr = nullptr,
        std::atomic<float>* outputPeakPtr = nullptr);
    ~PLANETMainGui() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;   // draws the selected-drawbar outline on top of the sliders
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void updateAdsrDisplay();
    void timerCallback() override;
    void updateDrawbarColors();
    
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void bindToSelectedDrawbar();
    void updateDrawbarDensityLabel();   // "Density" <-> "Wander" per the selected drawbar's Noise state
    void refreshAllGUIValues();  // Refresh all GUI elements from current parameter values
    void updatePatchNameDisplay(const juce::String& name);
    void updatePatchCommentDisplay(const juce::String& comment);
    void setLifeVoicingParams(LifeVoicingParams* p);   // wires the dev panel to the processor

    // Flag to suppress GUI updates during bulk parameter loading
    bool suppressParameterUpdates = false;

    // Envelope drag state
    enum class DragTarget { None, HarmonicAttack, HarmonicDecaySustain, HarmonicRelease,
                            AmpAttack, AmpDecaySustain, AmpRelease };
    DragTarget currentDragTarget = DragTarget::None;
    
    // Envelope graph bounds
    juce::Rectangle<int> harmonicEnvBounds;
    juce::Rectangle<int> ampEnvBounds;
    
    // Helper methods for envelope interaction
    juce::Point<float> getEnvelopePoint(int pointIndex, const juce::Rectangle<int>& bounds,
                                         float attack, float decay, float sustain, float release);
    void updateAdsrFromDrag(const juce::MouseEvent& event);
    
    // Envelope drawing helper - consolidates duplicate code
    void drawEnvelopeCurve(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                           float attack, float decay, float sustain, float release,
                           float curveAmount, juce::Colour strokeColour, juce::Colour handleOutlineColour);

private:
    // Colour scheme
    juce::Colour backgroundLight { 0xff3a3a3a };
    juce::Colour backgroundGlobal { 0xff2a2a4a };
    juce::Colour accentColour { 0xff6ab0ff };
    juce::Colour globalAccent { 0xff8a93a3 };   // Steel grey — for global elements (amp envelope, global section accents); neutral against Palette-03 hues. Tune by eye.

    // Drawbar colours (resistor code inspired)
    std::array<juce::Colour, 10> drawbarColours {
        juce::Colour(0xffff9886),   // 1 - coral
        juce::Colour(0xffae6100),   // 2 - amber
        juce::Colour(0xffd7b946),   // 3 - gold
        juce::Colour(0xff638519),   // 4 - olive
        juce::Colour(0xff64d599),   // 5 - mint
        juce::Colour(0xff008f89),   // 6 - teal
        juce::Colour(0xff33cdf8),   // 7 - cyan
        juce::Colour(0xff3779c5),   // 8 - blue
        juce::Colour(0xffb3adff),   // 9 - lavender
        juce::Colour(0xff985baa)    // 10 - purple
    };

    // Layout proportions
    static constexpr float leftWidthRatio = 0.67f;
    static constexpr float rightWidthRatio = 0.33f;
    static constexpr int patchBarHeight = 40;
    static constexpr int drawbarSectionHeight = 220;

    // Reference to APVTS
    juce::AudioProcessorValueTreeState& apvts;
    juce::AudioProcessor* audioProcessor = nullptr;

    // Drawbar components
    std::array<ToggleResetSlider, 10> drawbarSliders;
    std::array<juce::Label, 10> fValueLabels;
    std::array<juce::Rectangle<int>, 10> drawbarColumnBounds;  // per-column bounds (set in resized()), used to outline the selected drawbar
    int selectedDrawbar = 0;

    // ---- F7/F8 per-drawbar routing switches (console channel-strip paradigm) ----
    // Two circles in a routing strip to the RIGHT of each fader (like a mixer's channel routing
    // buttons): top "P" = route to phase-distortion path; bottom "A" = route direct to output
    // (additive partial). Both on = feeds both; both off = muted. Bounds set in resized(), drawn in
    // paintOverChildren(), clicked in mouseDown(). Live param values are read from cached atomics.
    // First-pass visual state (circle colours + muted-column wash) — to be dialled in by eye.
    std::array<juce::Rectangle<int>, 10> pmSwitchBounds;
    std::array<juce::Rectangle<int>, 10> outSwitchBounds;
    std::array<std::atomic<float>*, 10> toPMParamPtr {};
    std::array<std::atomic<float>*, 10> toOutParamPtr {};
    void toggleRoutingParam(const juce::String& paramID);

    // ---- F10 per-drawbar single-trigger ("Perc") switch ----
    // A third switch circle above the routing pair, labelled "Perc": off = Multi (retrigger every
    // note, the default), on = Single (fire the drawbar's envelope only on the first note of a
    // phrase - Hammond percussion). Shown/clickable ONLY when the drawbar's envelope is active
    // (|EnvelopeAmount| > 0, the same condition that turns its fader thumb red), since single-trigger
    // is meaningless without an envelope. Bounds set in resized(), drawn in paintOverChildren(),
    // clicked in mouseDown(); toggled via toggleRoutingParam() like the routing switches.
    std::array<juce::Rectangle<int>, 10> percSwitchBounds;
    std::array<std::atomic<float>*, 10> trigSingleParamPtr {};

    // ---- Per-drawbar Noise switch (experimental) ----
    // Fourth switch circle, between Shape and Perc: on = the drawbar is hijacked into a
    // band-pass noise source centred on its harmonic F (breath/chiff) - K/ADSR/LFO/routing
    // stay live, and the bar's Density knob becomes the band width.
    // Always visible (unlike Perc). Same bounds/paint/click plumbing as the routing switches.
    std::array<juce::Rectangle<int>, 10> noiseSwitchBounds;
    std::array<std::atomic<float>*, 10> noiseParamPtr {};
    bool drawbarEnvelopeActive(int drawbarIndex) const;  // |EnvelopeAmount| > 0 for drawbar i
    // Per-drawbar envelope-active state from the last updateDrawbarColors() pass. The Perc switch is
    // drawn in paintOverChildren (parent), which the timer doesn't otherwise repaint - so when this
    // flips we repaint the column strip to make the switch appear/disappear live.
    std::array<bool, 10> prevEnvelopeActive {};
    DrawbarLookAndFeel drawbarLookAndFeel;  // Custom LookAndFeel for LFO visual feedback

    // ---- LFO-rate "ping" pulse indicators (item #5) ----
    // Per-drawbar GUI-side phase [0,1); advanced in the timer at each drawbar's effective LFO
    // rate (free = Hz param; sync = syncDivisionToHz(div, bpm)). Phase -> brightness drives the
    // drawbar's LFO ring (indicator 1) and the selected drawbar's LFO-speed-knob inner circle
    // (indicator 2). "Ping" = hard onset at phase 0, exponential decay across the cycle.
    std::atomic<double>* dawBpm = nullptr;
    std::atomic<bool>*   transportPlaying = nullptr;
    std::array<double, 10> lfoPulsePhase {};
    double lfoPulseLastMs = 0.0;                 // hi-res timestamp of last pulse advance (0 = uninit)
    void updateLfoPulses();                      // advance phases + push brightness into slider properties
    static constexpr float LFO_PULSE_FLOOR      = 0.30f;   // drawbar-ring brightness between pings (kept visible on slow rates)
    static constexpr float LFO_PULSE_KNOB_FLOOR = 0.15f;   // LFO-speed-knob circle floor — lower than the ring so its ping is easy to see
    static constexpr float LFO_PULSE_DECAY      = 3.0f;    // exp decay rate across one cycle (lower = slower falloff / longer ping tail)
    static constexpr float LFO_PULSE_MAX_HZ     = 12.0f;   // cap visual rate so very fast LFOs flutter, not strobe

    // ---- F1: copy envelope / mod params between drawbars by dragging a control's background ----
    // Drag the harmonic-envelope graph background onto another drawbar to copy its ADSR + depth;
    // drag the LFO/velocity zone background to copy the LFO + velocity params. Source = selected
    // drawbar (that's what these controls edit). No modifier keys, no extra permanent widgets.
    enum class CopyDragKind { None, Envelope, Mod };
    CopyDragKind copyDrag = CopyDragKind::None;
    int copyDragSource = -1;                          // drawbar copied FROM (the selected one)
    int copyDragHoverTarget = -1;                     // drawbar column under the pointer (-1 = none)
    juce::Rectangle<int> modZoneBounds;               // background-drag source region for the mod copy
    int  drawbarColumnAt(juce::Point<int> p) const;   // drawbar column under a point, or -1
    void copyEnvelopeParamsBetweenDrawbars(int from, int to);
    void copyModParamsBetweenDrawbars(int from, int to);

    IshtarLookAndFeel ishtarLookAndFeel;          // Global star knobs — steel-grey accent
    IshtarLookAndFeel drawbarIshtarLookAndFeel;   // Per-drawbar star knobs — tracks selected drawbar's colour

    // Custom fonts
    juce::Font amarnaRegular;
    juce::Font amarnaSemiBold;

    // SliderAttachments for K1-K10 drawbars
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 10> drawbarAttachments;

    // Harmonic ADSR display
    std::array<juce::Label, 4> adsrLabels;
    std::array<juce::Label, 4> adsrValueEditors;
    float adsrValues[10][4] = {};
    
    // Current harmonic parameter IDs
    juce::String currentHarmonicParamIDs[4];

    // Harmonic LFO controls
    FocuslessComboBox lfoShapeCombo;
    FocuslessComboBox lfoSyncCombo;
    ToggleResetSlider lfoSpeedKnob;
    ToggleResetSlider lfoDepthKnob;
    juce::Label lfoShapeLabel, lfoSyncLabel, lfoSpeedLabel, lfoDepthLabel;
    juce::Label selectedFDisplay;
    juce::Label lfoSpeedValue, lfoDepthValue;
    bool currentSyncMode = false;  // Track whether speed knob is in sync mode
    void updateLfoSyncMode();      // Switch speed knob between Hz and sync division modes


    // Envelope depth control
    ToggleResetSlider envDepthKnob;
    juce::Label envDepthLabel;
    juce::Label envDepthValue;

    // Velocity to Drawbar control (context-sensitive)
    ToggleResetSlider velToDrawbarKnob;
    juce::Label velToDrawbarLabel;
    juce::Label velToDrawbarValue;

    // Per-drawbar Density control (context-sensitive, experimental)
    ToggleResetSlider drawbarDensityKnob;
    juce::Label drawbarDensityLabel;
    juce::Label drawbarDensityValue;

    // Amplitude ADSR display
    std::array<juce::Label, 4> ampAdsrLabels;
    std::array<juce::Label, 4> ampAdsrValueEditors;
    float ampAdsrValues[4] = { 0.1f, 0.3f, 0.7f, 0.5f };

    // Velocity to Amplitude control
    ToggleResetSlider velAmpSlider;
    juce::Label velAmpLabel;
    juce::Label velAmpValue;

    // Amplitude / Character zone knobs (2x2 grid: Vel->Attk, Env Curve / Vintage, Life)
    ToggleResetSlider velAttackKnob, envCurveKnob, vintageKnob, lifeKnob;
    juce::Label velAttackLabel, envCurveLabel, vintageLabel, lifeLabel;
    juce::Label velAttackValue, envCurveValue, vintageValue, lifeValue;

    // LIFE seed: editable readout (type to recall) + Star-of-Ishtar "re-cast" button.
    juce::Label seedLabel, seedValue;
    juce::ShapeButton rerollButton { "reroll", juce::Colour(0xff6ab0ff),
                                     juce::Colour(0xffaad4ff), juce::Colour(0xffffffff) };
    // Bolt anchors (set in resized) + flash level (0..1, pulsed on re-cast, decayed in timer).
    juce::Rectangle<int> lifeKnobBounds, seedModuleBounds;
    float boltFlash = 0.0f;
    void rerollSeed();

    // ---- LIFE voicing dev panel (temporary tuning scaffold) ----
    // Shift-click the seed star to toggle. Functional-ugly by design: orange border
    // so it can't be mistaken for shipping GUI. Sliders write straight to the
    // processor's LifeVoicingParams atomics; Snapshot dumps them to a text file.
    struct VoicingPanel : juce::Component {
        void paint(juce::Graphics& g) override {
            g.fillAll(juce::Colour(0xf0141420));
            g.setColour(juce::Colours::orange);
            g.drawRect(getLocalBounds(), 2);
            g.drawText("LIFE VOICING (dev)", 10, 6, getWidth() - 20, 16, juce::Justification::left);
        }
    };
    VoicingPanel voicingPanel;
    static constexpr int NUM_VOICING_SLIDERS = 5;
    std::array<juce::Slider, NUM_VOICING_SLIDERS> voicingSliders;
    std::array<juce::Label, NUM_VOICING_SLIDERS> voicingSliderLabels;
    juce::TextButton voicingSnapshotButton { "Snapshot" };
    juce::TextButton voicingCloseButton { "x" };   // dismiss the dev panel (top-right corner)
    juce::Label voicingSavedLabel;
    LifeVoicingParams* lifeVoicingParams = nullptr;
    void toggleVoicingPanel();
    void saveVoicingSnapshot();

    // ======================== RIGHT COLUMN CONTROLS ========================
    
    // Vibrato section
    ToggleResetSlider vibratoRateKnob, vibratoDepthKnob, vibratoFadeKnob;
    juce::Label vibratoRateLabel, vibratoDepthLabel, vibratoFadeLabel;
    
    // Pitch section (+ Portamento, F2a: Porta knob and a Rate/Time mode toggle)
    ToggleResetSlider pitchDistKnob, pitchTimeKnob, portamentoTimeKnob;
    juce::Label pitchDistLabel, pitchTimeLabel, portamentoLabel;
    
    // Colour section (Brilliance + Density/carrier-morph)
    ToggleResetSlider brillianceSlider;
    juce::Label brillianceMainLabel;
    ToggleResetSlider densitySlider;                   // F5 "Density": carrier sine->soft-saw morph
    juce::Label brillianceSubLabel, densitySubLabel;   // small labels under each slider
    juce::Label densityValue;
    juce::Rectangle<int> densitySliderBounds;

    // Per-slider "Mod wheel" buttons in the reserved gap at the right of each Colour slider.
    // Each cycles Off -> Normal -> Inverse (a saved choice param). Custom-drawn (paint) + hit-tested
    // (mouseDown), like the drawbar routing circles.
    // Two-zone button: left "MW" half toggles Off<->On (at the remembered polarity); right half is a
    // big up/down triangle that flips Normal<->Inverse. So disconnecting (-> Off) is one clean click on
    // the MW half and never passes through a polarity flip. lastPolarity remembers 1/2 across Off.
    juce::Rectangle<int> brillianceMWButtonBounds, densityMWButtonBounds;
    // Portamento Rate/Time toggle (F2a): custom-painted to match the MW buttons above it - dark/grey
    // = off = "Time" (the subtler default), accent fill = on = "Rate". Drawn in paint(), hit-tested
    // in mouseDown(), toggled via toggleRoutingParam() like the routing switches.
    juce::Rectangle<int> portamentoModeButtonBounds;
    // Vibrato VEL gate button (same visual language / column as the Rate/Time + MW buttons). A single
    // click toggles the gate on/off; the permanent velThresholdValue field below it (a plain editable
    // label like Transpose/Stack) sets the 1-127 velocity threshold.
    juce::Rectangle<int> velSwitchButtonBounds;
    juce::Label velThresholdValue;                     // editable 1-127 threshold, below the VEL button
    void updateVelThresholdLook();                     // dim the field when the gate is off
    std::atomic<float>* brillianceMWParam = nullptr;   // 0 Off / 1 Normal / 2 Inverse
    std::atomic<float>* densityMWParam = nullptr;
    std::atomic<float>* portamentoModeParam = nullptr; // 0 = Time (off) / 1 = Rate (on)
    std::atomic<float>* vibratoVelSwitchParam = nullptr;    // 0 = off / 1 = velocity-gated
    std::atomic<float>* vibratoVelThresholdParam = nullptr; // 1-127
    int brillLastPolarity = 1;
    int densLastPolarity = 1;
    void handleMWButtonClick(const juce::String& paramID, juce::Rectangle<int> bounds,
                             juce::Point<int> pos, int& lastPolarity);

    // Published effective (post mod-wheel / latch) values from the processor, so the diff indicators
    // draw exactly what's heard. Also cached for change-detection in the timer.
    std::atomic<float>* effectiveBrillianceValue = nullptr;
    std::atomic<float>* effectiveCarrierMorphValue = nullptr;

    // Output-saturation meter state (F12). outputPeakValue is the processor's running peak (GUI
    // exchanges it to 0 each frame). meterDisplayPeak is a GUI-side ballistic (fast attack, slow
    // release) so amber/green track smoothly; redHoldFramesLeft latches red for a few seconds.
    std::atomic<float>* outputPeakValue = nullptr;
    float meterDisplayPeak = 0.0f;
    int   redHoldFramesLeft = 0;
    float cachedEffectiveCarrierMorph = 0.0f;

    // Mod wheel tracking
    std::atomic<float>* rawModWheelValue = nullptr;
    std::atomic<bool>* modWheelEngaged = nullptr;
    juce::Rectangle<int> brillianceSliderBounds;
    float cachedEffectiveBrilliance = 0.5f;

    // Waveform display component
    WaveformDisplay waveformDisplay;
    
    // Effects section
    ToggleResetSlider detuneAmountSlider, detuneMixSlider;
    juce::Label detuneAmountLabel, detuneMixLabel;
    ToggleResetSlider warmthSlider;
    juce::Label warmthLabel;
    ToggleResetSlider punchSlider, punchFrequencySlider;
    juce::Label punchLabel, punchFrequencyLabel;
    juce::Label detuneAmountValue, detuneMixValue, warmthValue, punchValue, punchFrequencyValue;
    juce::Label brillianceValue;
    juce::Label vibratoRateValue, vibratoDepthValue, vibratoFadeValue;
    juce::Label pitchDistValue, pitchTimeValue, portamentoValue;

    // ======================== PATCH MANAGEMENT UI ========================
    juce::TextButton loadPatchButton;
    juce::TextButton savePatchButton;
    juce::Label currentPatchLabel;
    juce::String currentPatchName;
    juce::Label patchCommentLabel;

    // Master controls in patch bar
    ToggleResetSlider masterVolumeSlider;
    juce::Label masterVolumeLabel;
    juce::Label transposeLabel;
    juce::Label transposeValue;

    // Unison controls in the patch bar (F6): Stack = editable numeric 1-4 (like Trans); Detune =
    // horizontal slider next to Vol. Placed here so Stack & Vol sit adjacent (a stack change usually
    // wants a volume tweak). Plus a static version label at the far right.
    juce::Label unisonVoicesLabel;    // "Stack"
    juce::Label unisonVoicesValue;    // editable 1-4
    ToggleResetSlider unisonDetuneSlider;
    juce::Label unisonDetuneLabel;    // "Detune"
    juce::Label versionLabel;

    // Credits / About overlay + the clickable ISHTAR wordmark region that opens it.
    juce::Rectangle<int> ishtarWordmarkBounds;
    IshtarCreditsOverlay creditsOverlay;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterVolumeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> unisonDetuneAttachment;

    void loadPatchButtonClicked();
    void savePatchButtonClicked();
   

    // ======================== SLIDER ATTACHMENTS ========================
    
    // Right column attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vibratoRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vibratoDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vibratoFadeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchDistAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchTimeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> portamentoTimeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> brillianceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> carrierMorphAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> detuneAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> detuneMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> warmthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> punchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> punchFrequencyAttachment;
   
   
    
    // Amplitude section attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velAmpAttachment;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velAttackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envCurveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vintageAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lifeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envDepthAttachment;
    
    // Context-sensitive attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoSpeedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoShapeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoSyncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoSyncDivAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velToDrawbarAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> drawbarDensityAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PLANETMainGui)
};

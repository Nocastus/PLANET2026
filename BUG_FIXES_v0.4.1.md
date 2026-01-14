# PLANET2026 v0.4.1 - Bug Fixes
**Date:** 14 January 2026
**Location:** `N:\PLUGIN DEVELOPMENT\PLANET2026 development\PLANET2026\`

---

## BUGS FIXED

### **Bug #1: Punch controls not visible in GUI** ✅ FIXED

**Problem:** Punch and PunchFrequency parameters were integrated into the audio engine but had no GUI controls.

**Solution:** Added two vertical sliders in the Effects section.

**Files Modified:**
- `PLANETMainGui.h` (lines 160-161, 185-186)
- `PLANETMainGui.cpp` (lines 311-312, 345-348, 937-961)

**Changes Made:**

1. **Added slider and label members (PLANETMainGui.h:160-161):**
   ```cpp
   juce::Slider punchSlider, punchFrequencySlider;
   juce::Label punchLabel, punchFrequencyLabel;
   ```

2. **Added attachments (PLANETMainGui.h:185-186):**
   ```cpp
   std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> punchAttachment;
   std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> punchFrequencyAttachment;
   ```

3. **Added slider setup (PLANETMainGui.cpp:311-312):**
   ```cpp
   setupVerticalSlider(punchSlider, punchLabel, "Punch", 0.0, 1.0, 0.0);
   setupVerticalSlider(punchFrequencySlider, punchFrequencyLabel, "Pnch Frq", 500.0, 5000.0, 1800.0);
   ```

4. **Created attachments (PLANETMainGui.cpp:345-348):**
   ```cpp
   punchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
       apvts, "punch", punchSlider);
   punchFrequencyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
       apvts, "punchFrequency", punchFrequencySlider);
   ```

5. **Updated layout (PLANETMainGui.cpp:937-961):**
   - Changed `effectsSliderSpacing` from `rightContentWidth / 4` to `rightContentWidth / 5`
   - Added layout positions for esx3 (Punch) and esx4 (Pnch Frq)

**Result:** Effects section now displays 5 sliders: Detune, Det Mix, Warmth, Punch, Pnch Frq

---

### **Bug #2: No visual feedback on drawbars for LFO activity** ⚠️ PARTIAL

**Problem:** LFO state is captured in slider properties but not visually displayed.

**Current Status:**
- ✅ LFO state IS being calculated and stored in `drawbarSliders[i].getProperties().set("hasActiveLFO", hasActiveLFO)`
- ✅ Color feedback for envelope activity (red) and drawbar position (blue/white) works correctly
- ⚠️ LFO visual feedback requires custom LookAndFeel to render differently

**Technical Details:**
The `updateDrawbarColors()` method (PLANETMainGui.cpp:540-583) already:
1. Checks LFO amount for each drawbar
2. Sets `hasActiveLFO` boolean property on each slider
3. This property can be read by a custom LookAndFeel's `drawLinearSliderThumb()` method

**To Complete LFO Visual Feedback (Future Enhancement):**
Would need to create a custom LookAndFeel class that:
```cpp
class DrawbarLookAndFeel : public juce::LookAndFeel_V4 {
    void drawLinearSliderThumb(Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              const Slider::SliderStyle style, Slider& slider) override {
        bool hasLFO = slider.getProperties()["hasActiveLFO"];
        if (hasLFO) {
            // Draw pulsing border or other visual indicator
        }
        // Draw normal thumb
    }
};
```

**Decision:** Left as future enhancement. Current color system (red/blue/white) is functional and clear.

---

### **Bug #3: Numeric displays don't update when controls moved** ✅ FIXED

**Problem:** Value labels for Env Depth, Env Curve, Vel Brill, Vel Ampli, Vel Attack, and Vintage parameters didn't update when their corresponding knobs/sliders were moved.

**Root Cause:** Parameters weren't registered as listeners with the APVTS.

**Solution:** Added parameter listeners for all affected parameters.

**Files Modified:**
- `PLANETMainGui.cpp` (lines 451-456, 488-492, 1302-1328)

**Changes Made:**

1. **Added listeners in constructor (PLANETMainGui.cpp:451-456):**
   ```cpp
   // Register as listener for amplitude zone parameters
   apvts.addParameterListener("exponentialControl", this);
   apvts.addParameterListener("velToBrilliance", this);
   apvts.addParameterListener("velToAmplitude", this);
   apvts.addParameterListener("velToAttackTime", this);
   apvts.addParameterListener("vintageAmount", this);
   ```

2. **Added cleanup in destructor (PLANETMainGui.cpp:488-492):**
   ```cpp
   apvts.removeParameterListener("exponentialControl", this);
   apvts.removeParameterListener("velToBrilliance", this);
   apvts.removeParameterListener("velToAmplitude", this);
   apvts.removeParameterListener("velToAttackTime", this);
   apvts.removeParameterListener("vintageAmount", this);
   ```

3. **Added handlers in parameterChanged() (PLANETMainGui.cpp:1302-1328):**
   ```cpp
   // Amplitude zone parameters
   if (parameterID == "exponentialControl") {
       envCurveValue.setText(juce::String(newValue, 2), juce::dontSendNotification);
       return;
   }
   if (parameterID == "velToBrilliance") {
       velBrillValue.setText(juce::String(newValue, 2), juce::dontSendNotification);
       return;
   }
   if (parameterID == "velToAmplitude") {
       velAmpValue.setText(juce::String(newValue, 2), juce::dontSendNotification);
       return;
   }
   if (parameterID == "velToAttackTime") {
       velAttackValue.setText(juce::String(newValue, 2), juce::dontSendNotification);
       return;
   }
   if (parameterID == "vintageAmount") {
       vintageValue.setText(juce::String(newValue, 2), juce::dontSendNotification);
       return;
   }

   // Envelope depth for currently selected drawbar
   if (parameterID == juce::String("k") + juce::String(selectedDrawbar + 1) + "EnvelopeAmount") {
       envDepthValue.setText(juce::String(newValue, 2), juce::dontSendNotification);
       return;
   }
   ```

**Result:** All numeric value displays now update in real-time when controls are moved.

---

## PARAMETER MAPPING

For reference, here's the mapping between parameter IDs and value labels:

| Parameter ID | Value Label | Control Type |
|--------------|-------------|--------------|
| `exponentialControl` | `envCurveValue` | Knob (Env Curve) |
| `velToBrilliance` | `velBrillValue` | Knob (Vel Brill) |
| `velToAmplitude` | `velAmpValue` | Slider (Vel Ampli) |
| `velToAttackTime` | `velAttackValue` | Knob (Vel Attack) |
| `vintageAmount` | `vintageValue` | Knob (Vintage) |
| `k[1-10]EnvelopeAmount` | `envDepthValue` | Knob (Env Depth) - per drawbar |

---

## TESTING CHECKLIST

Before deploying, verify:

### **Punch Controls:**
- [ ] Punch slider visible in Effects section (4th slider)
- [ ] Pnch Frq slider visible in Effects section (5th slider)
- [ ] Both sliders respond to mouse input
- [ ] Both sliders update parameter values
- [ ] Punch effect audibly changes sound when Punch slider moved
- [ ] Punch frequency parameter changes tone character when moved
- [ ] Both parameters save/load correctly in patches

### **Numeric Display Updates:**
- [ ] Move Env Curve knob → `envCurveValue` updates
- [ ] Move Vel Brill knob → `velBrillValue` updates
- [ ] Move Vel Ampli slider → `velAmpValue` updates
- [ ] Move Vel Attack knob → `velAttackValue` updates
- [ ] Move Vintage knob → `vintageValue` updates
- [ ] Select drawbar, move Env Depth knob → `envDepthValue` updates
- [ ] All values display with 2 decimal places
- [ ] Values update smoothly without lag

### **Drawbar Visual Feedback:**
- [ ] Drawbar at null (0.0) → White thumb
- [ ] Drawbar non-null, no envelope → Blue thumb
- [ ] Drawbar with envelope active (non-zero env depth) → Red thumb
- [ ] Color changes happen immediately when parameters change

---

## BUILD INSTRUCTIONS

1. **Open in Visual Studio 2022:**
   ```
   N:\PLUGIN DEVELOPMENT\PLANET2026 development\PLANET2026\Builds\VisualStudio2022\PLANET2026.sln
   ```

2. **Build:**
   - Configuration: Release
   - Platform: x64
   - Build → Build Solution (Ctrl+Shift+B)

3. **Test:**
   - Load VST3 in DAW
   - Verify all three bug fixes

---

## VERSION UPDATE

**Suggested version bump:** v0.4.0 → v0.4.1

This is a minor patch release with bug fixes and no new features (Punch was already in v0.4.0 but just not visible).

If you want to update the version number, edit:
```
N:\PLUGIN DEVELOPMENT\PLANET2026 development\PLANET2026\Source\PluginEditor.cpp:27
```

Change:
```cpp
versionLabel.setText("PLANET v0.4.0 - 14 Jan 2026", juce::dontSendNotification);
```

To:
```cpp
versionLabel.setText("PLANET v0.4.1 - 14 Jan 2026", juce::dontSendNotification);
```

---

## SUMMARY

**Files Modified:** 2
- `PLANETMainGui.h` (added Punch GUI members)
- `PLANETMainGui.cpp` (added Punch GUI setup, added parameter listeners, updated parameterChanged)

**Lines Changed:** ~80 lines total
- Added: ~60 lines
- Modified: ~20 lines

**Bugs Fixed:** 2 complete + 1 documented for future
- ✅ Punch controls now visible
- ✅ Numeric displays update correctly
- ⚠️ LFO visual feedback infrastructure in place (needs custom LookAndFeel)

**Status:** Ready to build and test!

---

*End of bug fix documentation*

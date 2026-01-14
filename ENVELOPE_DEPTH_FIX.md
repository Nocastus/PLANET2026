# Envelope Depth Display Fix
**Date:** 14 January 2026

## PROBLEM
Envelope Depth numeric display wasn't updating when the Env Depth knob was moved.

## ROOT CAUSE
The envelope depth parameters (k1EnvelopeAmount through k10EnvelopeAmount) weren't registered as parameter listeners, so the parameterChanged() callback was never triggered for these parameters.

## SOLUTION
Added parameter listeners for all 10 envelope depth parameters.

## FILES MODIFIED

### PLANETMainGui.cpp

**Added listeners in constructor (lines 459-464):**
```cpp
// Register as listener for envelope depth parameters (K1-K10)
for (int i = 1; i <= 10; ++i)
{
    juce::String paramID = "k" + juce::String(i) + "EnvelopeAmount";
    apvts.addParameterListener(paramID, this);
}
```

**Added cleanup in destructor (lines 509-514):**
```cpp
// Remove listeners for envelope depth parameters (K1-K10)
for (int i = 1; i <= 10; ++i)
{
    juce::String paramID = "k" + juce::String(i) + "EnvelopeAmount";
    apvts.removeParameterListener(paramID, this);
}
```

**Handler already existed in parameterChanged() (line 1347):**
```cpp
// Envelope depth for currently selected drawbar
if (parameterID == juce::String("k") + juce::String(selectedDrawbar + 1) + "EnvelopeAmount") {
    envDepthValue.setText(juce::String(newValue, 2), juce::dontSendNotification);
    return;
}
```

## HOW IT WORKS

1. When you select a drawbar (e.g., K3), `selectedDrawbar` is set to 2 (0-indexed)
2. When you move the Env Depth knob, it changes parameter "k3EnvelopeAmount"
3. parameterChanged() is called with parameterID = "k3EnvelopeAmount"
4. The check compares: "k3EnvelopeAmount" == "k" + String(2+1) + "EnvelopeAmount" → match!
5. Updates envDepthValue.setText() with the new value

## TESTING

Test that envelope depth display updates:
- [ ] Select K1, move Env Depth knob → display updates
- [ ] Select K5, move Env Depth knob → display updates
- [ ] Select K10, move Env Depth knob → display updates
- [ ] Switch between drawbars → display shows correct value for selected drawbar

## STATUS
✅ **FIXED** - Ready to build and test

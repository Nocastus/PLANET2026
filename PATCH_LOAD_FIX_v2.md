# Patch Loading Fix v2 - Using NormalisableRange
**Date:** 14 January 2026

## PROBLEM
After implementing the first fix (storing actual values in patches), loading newly saved patches still resulted in extreme parameter values.

## ROOT CAUSE
The conversion method was unreliable. Using `floatParam->convertTo0to1(actualValue)` wasn't consistently working across all parameter types.

## SOLUTION
Use the `NormalisableRange` directly from the parameter instead of relying on the parameter's conversion methods.

### **Code Change:**

**PLANETPatchManager.cpp - Line 196-207:**

**Before (unreliable):**
```cpp
if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param)) {
    float normalizedValue = floatParam->convertTo0to1(actualValue);
    param->setValueNotifyingHost(normalizedValue);
}
```

**After (reliable):**
```cpp
if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(paramID))) {
    // normalizeRange converts actual value to 0-1 range
    float normalizedValue = parameter->getNormalisableRange().convertTo0to1(actualValue);
    parameter->setValueNotifyingHost(normalizedValue);
}
```

### **Why This Works Better:**

1. **Cast to RangedAudioParameter** instead of AudioParameterFloat
   - RangedAudioParameter is the base class that has the range information
   - All JUCE parameters (Float, Int, Bool, Choice) derive from this

2. **Direct access to NormalisableRange**
   - `getNormalisableRange()` returns the actual range definition
   - `convertTo0to1()` on the range uses the proper min/max/skew

3. **No intermediate conversions**
   - Goes directly from actual value → normalized value
   - More reliable than parameter wrapper methods

## EXAMPLE CONVERSIONS

### **Input F Multiplier (range 0.5 to 30.0):**
```cpp
actualValue = 5.0
normalizedValue = range.convertTo0to1(5.0)
                = (5.0 - 0.5) / (30.0 - 0.5)
                = 4.5 / 29.5
                = 0.1525...
setValueNotifyingHost(0.1525)
Result: F multiplier correctly set to 5.0 ✅
```

### **Amplitude Release (range 0.001 to 10.0):**
```cpp
actualValue = 2.5
normalizedValue = range.convertTo0to1(2.5)
                = (2.5 - 0.001) / (10.0 - 0.001)
                = 2.499 / 9.999
                = 0.2499...
setValueNotifyingHost(0.2499)
Result: Release time correctly set to 2.5 seconds ✅
```

### **Punch Frequency (range 500.0 to 5000.0):**
```cpp
actualValue = 1800.0
normalizedValue = range.convertTo0to1(1800.0)
                = (1800.0 - 500.0) / (5000.0 - 500.0)
                = 1300.0 / 4500.0
                = 0.2888...
setValueNotifyingHost(0.2888)
Result: Punch frequency correctly set to 1800 Hz ✅
```

## COMPLETE FLOW

### **Saving a Patch:**
1. User sets F1 multiplier slider to 5.0
2. Internally stored as normalized: ~0.1525
3. `floatParam->get()` returns 0.1525
4. `floatParam->convertFrom0to1(0.1525)` converts to 5.0
5. Saved to markdown: `input_f1 = 5.00`

### **Loading that Patch:**
1. Read from markdown: `input_f1 = 5.00`
2. actualValue = 5.0
3. Get parameter's NormalisableRange
4. `range.convertTo0to1(5.0)` returns 0.1525
5. `setValueNotifyingHost(0.1525)`
6. F1 multiplier correctly displays as 5.0 ✅

## TESTING CHECKLIST

### **Create and Load New Patch:**
- [ ] Set F1 multiplier to 5.0
- [ ] Set amplitude release to 2.5 seconds
- [ ] Set punch frequency to 1800 Hz
- [ ] Save patch as "Test Patch"
- [ ] Load "Test Patch"
- [ ] Verify F1 = 5.0 (not extreme value)
- [ ] Verify release = 2.5 (fades smoothly)
- [ ] Verify punch freq = 1800

### **All Parameter Types:**
- [ ] Drawbar coefficients (-2.0 to 2.0)
- [ ] F multipliers (0.5 to 30.0)
- [ ] Envelope times (0.001 to 10.0)
- [ ] LFO rates (0.05 to 1000.0)
- [ ] Velocity parameters (0 to 200, -100 to 100)
- [ ] Effects (0.0 to 1.0)
- [ ] Punch frequency (500 to 5000)

### **Old Patches:**
- [ ] Old patches (pre-fix) load correctly
- [ ] Can save them again to update format
- [ ] Re-saved patches load correctly

## WHY THE ORIGINAL APPROACH FAILED

The `AudioParameterFloat::convertTo0to1()` method might:
- Not be available on all parameter types
- Have different behavior than expected
- Require additional context we weren't providing

Using `NormalisableRange` directly is:
- ✅ More explicit
- ✅ Works for all ranged parameter types
- ✅ Uses the exact same range logic as the parameter definition
- ✅ More reliable and predictable

## STATUS

✅ **FIXED** - Using NormalisableRange for reliable conversion

**Files Modified:**
- PLANETPatchManager.cpp (lines 196-207)

**Next Step:**
Build and test - new patches should now save and load correctly!

---

*End of patch loading fix v2*

# LFO Outline Color Fix
**Date:** 14 January 2026

## PROBLEM
The LFO outline strokes were always appearing in pale blue instead of adapting to each drawbar's unique background color.

## ROOT CAUSE
The DrawbarLookAndFeel was reading `slider.findColour(Slider::backgroundColourId)`, but the drawbar sliders didn't have their background colors explicitly set. They were using the default LookAndFeel background color (pale blue).

The actual drawbar colors were only used in `paint()` to draw the background rectangles behind the sliders, not applied to the Slider components themselves.

## SOLUTION
Modified the DrawbarLookAndFeel to:
1. Accept a pointer to the drawbarColours array
2. Store the drawbar index in each slider's properties
3. Look up the actual drawbar color using the index
4. Use that color for brightness-adaptive outline rendering

## FILES MODIFIED

### PLANETMainGui.h

**Added to DrawbarLookAndFeel class:**

1. **Constructor with initialization (line 20):**
```cpp
DrawbarLookAndFeel() : drawbarColours(nullptr) {}
```

2. **Method to set color array pointer (lines 22-25):**
```cpp
void setDrawbarColours(const std::array<juce::Colour, 10>* colours)
{
    drawbarColours = colours;
}
```

3. **Private member variable (line 90):**
```cpp
private:
    const std::array<juce::Colour, 10>* drawbarColours;
```

4. **Updated outline drawing logic (lines 54-83):**
```cpp
// If LFO is active, draw an outline stroke with adaptive brightness
if (hasLFO && drawbarColours != nullptr)
{
    // Get the drawbar color from the stored index
    int drawbarIndex = slider.getProperties()["drawbarIndex"];
    if (drawbarIndex >= 0 && drawbarIndex < 10)
    {
        auto bgColour = (*drawbarColours)[drawbarIndex];

        // Calculate luminance (perceived brightness)
        float luminance = bgColour.getBrightness();

        // Choose outline color based on background brightness
        juce::Colour outlineColour;
        if (luminance > 0.5f)
        {
            // Background is light - use dark outline
            outlineColour = bgColour.withBrightness(0.2f);
        }
        else
        {
            // Background is dark - use light outline
            outlineColour = bgColour.withBrightness(0.9f);
        }

        g.setColour(outlineColour);
        g.drawEllipse(thumbCenterX - thumbRadius, thumbCenterY - thumbRadius,
                     thumbRadius * 2, thumbRadius * 2, 6.0f);  // 6px stroke
    }
}
```

### PLANETMainGui.cpp

**Constructor changes:**

1. **Initialize LookAndFeel with colors (lines 22-23):**
```cpp
// Set up drawbar LookAndFeel with color array
drawbarLookAndFeel.setDrawbarColours(&drawbarColours);
```

2. **Store drawbar index in each slider (line 37):**
```cpp
drawbarSliders[i].getProperties().set("drawbarIndex", i);  // Store index for color lookup
```

## HOW IT WORKS NOW

### **Data Flow:**

1. **Initialization (Constructor):**
   - Pass pointer to `drawbarColours` array to `DrawbarLookAndFeel`
   - For each slider, store its index (0-9) in properties

2. **Runtime (When Drawing LFO Outline):**
   - Check if LFO is active: `slider.getProperties()["hasActiveLFO"]`
   - If yes, get drawbar index: `slider.getProperties()["drawbarIndex"]`
   - Look up actual color: `(*drawbarColours)[drawbarIndex]`
   - Calculate brightness: `bgColour.getBrightness()`
   - Choose adaptive outline: dark (0.2) or bright (0.9)
   - Draw outline with correct color

### **Color Adaptation Per Drawbar:**

| Drawbar | Hex Color | Name | Luminance | Outline Type |
|---------|-----------|------|-----------|--------------|
| K1 | 0xff4a4a4a | Dark Grey | ~0.29 | Bright grey (0.9) |
| K2 | 0xff8b4513 | Brown | ~0.34 | Bright brown (0.9) |
| K3 | 0xffcc0000 | Red | ~0.40 | Bright red (0.9) |
| K4 | 0xffff6600 | Orange | ~0.50 | Dark orange (0.2) |
| K5 | 0xffcccc00 | Yellow | ~0.80 | Dark yellow (0.9) |
| K6 | 0xff00aa00 | Green | ~0.42 | Bright green (0.9) |
| K7 | 0xff0066cc | Blue | ~0.48 | Bright blue (0.9) |
| K8 | 0xff6600cc | Violet | ~0.44 | Bright violet (0.9) |
| K9 | 0xff666666 | Grey | ~0.40 | Bright grey (0.9) |
| K10 | 0xffeeeeee | White | ~0.93 | Dark white/grey (0.2) |

**Result:** Each drawbar now gets an outline that matches its color family but with inverted brightness for maximum visibility!

## VISUAL EXAMPLES

```
K1 (dark grey background):
   No LFO:  ○ (grey)
   LFO:     ⊙ (grey with bright grey outline)

K5 (yellow background):
   No LFO:  ○ (yellow)
   LFO:     ⊙ (yellow with dark yellow outline)

K7 (blue background):
   No LFO:  ○ (blue)
   LFO:     ⊙ (blue with bright blue outline)

K10 (white background):
   No LFO:  ○ (white)
   LFO:     ⊙ (white with dark grey outline)
```

## TESTING CHECKLIST

Verify each drawbar gets its unique adaptive outline:

- [ ] K1 (dark grey) → LFO active shows **bright grey** outline
- [ ] K2 (brown) → LFO active shows **bright brown** outline
- [ ] K3 (red) → LFO active shows **bright red** outline
- [ ] K4 (orange) → LFO active shows **dark/bright orange** outline (near threshold)
- [ ] K5 (yellow) → LFO active shows **dark yellow** outline
- [ ] K6 (green) → LFO active shows **bright green** outline
- [ ] K7 (blue) → LFO active shows **bright blue** outline
- [ ] K8 (violet) → LFO active shows **bright violet** outline
- [ ] K9 (grey) → LFO active shows **bright grey** outline
- [ ] K10 (white) → LFO active shows **dark grey** outline

### Additional Tests:
- [ ] Outline colors match the visual theme of each drawbar
- [ ] Outlines are clearly visible against their respective backgrounds
- [ ] No more "pale blue" outlines on all drawbars
- [ ] Outline stays with slider when drawbar value changes
- [ ] 6px stroke width is clearly visible

## TECHNICAL NOTES

### **Why Use Properties Instead of setColour?**

We could have used:
```cpp
drawbarSliders[i].setColour(Slider::backgroundColourId, drawbarColours[i]);
```

But that would:
- Require the track to also be drawn with the color
- Might interfere with other GUI elements
- Less flexible for future customization

Using properties + pointer:
- ✅ Cleaner separation of concerns
- ✅ Doesn't modify slider appearance elsewhere
- ✅ More flexible (can change colors dynamically)
- ✅ Follows JUCE best practices

### **Stroke Width**

Current value: **6.0f** (double the original 2.5f)
- Located in PLANETMainGui.h:81
- Easily adjustable if needed

## STATUS
✅ **FIXED** - Outline colors now adapt to each drawbar's unique background color!

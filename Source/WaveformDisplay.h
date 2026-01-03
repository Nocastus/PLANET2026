/*
  ==============================================================================
    WaveformDisplay.h - Real-time Waveform Visualization Component
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>

class WaveformDisplay : public juce::Component,
    public juce::Timer
{
public:
    static constexpr int SNAPSHOT_SIZE = 2048;

    WaveformDisplay();
    ~WaveformDisplay() override;

    void paint(juce::Graphics&) override;
    void timerCallback() override;

    void setDataSource(std::array<float, SNAPSHOT_SIZE>* snapshot,
        std::atomic<int>* snapshotLength,
        std::atomic<bool>* snapshotReady,
        std::atomic<bool>* snapshotRequest,
        std::atomic<bool>* active);

private:
    std::array<float, SNAPSHOT_SIZE>* waveformSnapshot = nullptr;
    std::atomic<int>* waveformSnapshotLength = nullptr;
    std::atomic<bool>* waveformSnapshotReady = nullptr;
    std::atomic<bool>* waveformSnapshotRequest = nullptr;
    std::atomic<bool>* waveformActive = nullptr;

    float peakAmplitude = 1.0f;

    juce::Colour backgroundColour{ 0xff000000 };
    juce::Colour waveColour{ 0xff6ab0ff };
    juce::Colour centerLineColour{ 0x4d6ab0ff };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
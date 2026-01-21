/*
  ==============================================================================
    WaveformDisplay.cpp - Real-time Waveform Visualization Component
  ==============================================================================
*/

#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay()
{
    startTimerHz(30);
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
}

void WaveformDisplay::setDataSource(std::array<float, SNAPSHOT_SIZE>* snapshot,
    std::atomic<int>* snapshotLength,
    std::atomic<bool>* snapshotReady,
    std::atomic<bool>* snapshotRequest,
    std::atomic<bool>* active)
{
    waveformSnapshot = snapshot;
    waveformSnapshotLength = snapshotLength;
    waveformSnapshotReady = snapshotReady;
    waveformSnapshotRequest = snapshotRequest;
    waveformActive = active;
}

void WaveformDisplay::timerCallback()
{
    // Request a new snapshot for next frame
    if (waveformSnapshotRequest != nullptr && waveformActive != nullptr && waveformActive->load())
    {
        waveformSnapshotRequest->store(true);
    }

    repaint();
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(backgroundColour);
    g.fillRoundedRectangle(bounds, 5.0f);

    // Center line
    float centerY = bounds.getCentreY();
    g.setColour(centerLineColour);
    g.drawHorizontalLine((int)centerY, bounds.getX() + 5, bounds.getRight() - 5);

    // Draw waveform if snapshot is ready
    if (waveformSnapshot != nullptr &&
        waveformSnapshotReady != nullptr &&
        waveformSnapshotReady->load() &&
        waveformActive != nullptr &&
        waveformActive->load())
    {
        int samplesToShow = waveformSnapshotLength->load();
        if (samplesToShow < 10) return;

        // Find peak amplitude for normalization
        float maxAmp = 0.0001f;
        for (int i = 0; i < samplesToShow; ++i)
        {
            float absVal = std::abs((*waveformSnapshot)[i]);
            if (absVal > maxAmp) maxAmp = absVal;
        }

        // Smooth the peak amplitude
        peakAmplitude = peakAmplitude * 0.8f + maxAmp * 0.2f;

        // Calculate scale to fill ~80% of display height
        float yScale = (bounds.getHeight() - 20) * 0.4f / peakAmplitude;
        float xScale = (bounds.getWidth() - 10) / (float)samplesToShow;

        juce::Path wavePath;

        for (int i = 0; i < samplesToShow; ++i)
        {
            float sample = (*waveformSnapshot)[i];
            float x = bounds.getX() + 5 + i * xScale;
            float y = centerY - sample * yScale;

            if (i == 0)
                wavePath.startNewSubPath(x, y);
            else
                wavePath.lineTo(x, y);
        }

        g.setColour(waveColour);
        g.strokePath(wavePath, juce::PathStrokeType(1.5f));
    }
    else
    {
        peakAmplitude = 1.0f;
    }


}
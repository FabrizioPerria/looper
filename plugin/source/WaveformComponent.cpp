#include "WaveformComponent.h"
#include "LinearRenderer.h"

WaveformComponent::WaveformComponent()
{
    renderer = std::make_unique<LinearRenderer>();
    startTimerHz (30);
}

WaveformComponent::~WaveformComponent()
{
    stopTimer();
    cancelPendingUpdate();
    backgroundProcessor.removeAllJobs (true, 5000);
}

void WaveformComponent::timerCallback()
{
    PERFETTO_FUNCTION();
    if (! bridge) return;

    // Lightweight position update - happens every frame
    size_t length, readPos;
    bool recording, playing;
    bridge->getPlaybackState (length, readPos, recording, playing);

    // Only repaint if position changed significantly or state changed
    bool stateChanged = (recording != lastRecording || playing != lastPlaying);
    bool posChanged = (readPos != lastReadPos);

    if (stateChanged || posChanged)
    {
        lastReadPos = readPos;
        lastRecording = recording;
        lastPlaying = playing;
        repaint();
    }

    // Check for waveform updates (less frequent, triggered by version change)
    if (bridge->getState().stateVersion.load (std::memory_order_relaxed) != lastProcessedVersion)
    {
        // Trigger async cache update
        triggerAsyncUpdate();
    }
}

void WaveformComponent::paint (juce::Graphics& g)
{
    PERFETTO_FUNCTION();
    g.fillAll (juce::Colours::black);

    if (! bridge)
    {
        g.setColour (juce::Colours::white);
        g.drawText ("No audio bridge", getLocalBounds(), juce::Justification::centred);
        return;
    }

    size_t length, readPos;
    bool recording, playing;
    bridge->getPlaybackState (length, readPos, recording, playing);

    if (length == 0)
    {
        g.setColour (juce::Colours::white);
        g.drawText ("Empty loop", getLocalBounds(), juce::Justification::centred);
        return;
    }

    // Draw from cache
    if (! cache.isEmpty() && cache.getWidth() > 0)
    {
        paintFromCache (g, (int) readPos, (int) length, recording);
    }
    else
    {
        // Loading state
        g.setColour (juce::Colours::grey);
        g.drawText ("Loading waveform...", getLocalBounds(), juce::Justification::centred);
    }
}

void WaveformComponent::paintFromCache (juce::Graphics& g, int readPos, int length, bool recording)
{
    PERFETTO_FUNCTION();
    int width = cache.getWidth();
    int height = getHeight();

    if (length <= 0 || width <= 0) return;

    int samplesPerPixel = std::max (1, length / width);
    int readPixel = std::min (readPos / samplesPerPixel, width - 1);

    // Draw waveform from cache
    for (int x = 0; x < width; ++x)
    {
        float min, max;
        if (cache.getMinMax (x, min, max, 0))
        {
            renderer->drawWaveformColumn (g, x, min, max, readPixel, height, recording);
        }
    }

    renderer->drawCRTEffects (g, readPixel, width, height);
}

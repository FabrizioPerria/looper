#include "WaveformComponent.h"

WaveformComponent::WaveformComponent()
{
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
            drawWaveformColumn (g, x, min, max, readPixel, height, recording);
        }
    }

    drawCRTEffects (g, readPixel, width, height);
}

void WaveformComponent::drawWaveformColumn (juce::Graphics& g, int x, float min, float max, int readPixel, int height, bool recording)
{
    PERFETTO_FUNCTION();
    float midY = height / 2.0f;
    float y1 = midY - (max * midY * 0.9f);
    float y2 = midY - (min * midY * 0.9f);

    juce::Colour waveColour = getWaveformColour (x, readPixel, recording);
    g.setColour (waveColour);
    g.drawLine ((float) x, y1, (float) x, y2, 1.5f);
}

juce::Colour WaveformComponent::getWaveformColour (int x, int readPixel, bool recording)
{
    PERFETTO_FUNCTION();
    int distance = std::abs (x - readPixel);

    if (distance < 2)
    {
        // Bright red at playhead
        return juce::Colour (255, 50, 50);
    }
    else if (distance < 10)
    {
        // Red glow fade to green
        float fade = (10 - distance) / 10.0f;
        return juce::Colour::fromFloatRGBA (0.5f + 0.5f * fade,   // R
                                            0.8f * (1.0f - fade), // G
                                            0.2f * (1.0f - fade), // B
                                            1.0f);
    }
    else
    {
        // Phosphor green
        return juce::Colour (0, 200, 50);
    }
}

void WaveformComponent::drawCRTEffects (juce::Graphics& g, int readPixel, int width, int height)
{
    PERFETTO_FUNCTION();
    float midY = height / 2.0f;

    // CRT scanlines
    g.setColour (juce::Colours::black.withAlpha (0.15f));
    for (int y = 0; y < height; y += 2)
    {
        g.drawHorizontalLine (y, 0.0f, (float) width);
    }

    // Playhead vertical line with glow
    for (int i = 0; i < 15; ++i)
    {
        float alpha = (15 - i) / 15.0f * 0.4f;
        g.setColour (juce::Colour (255, 50, 50).withAlpha (alpha));
        if (readPixel - i >= 0) g.drawVerticalLine (readPixel - i, 0.0f, (float) height);
        if (readPixel + i < width) g.drawVerticalLine (readPixel + i, 0.0f, (float) height);
    }

    // Center line
    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawHorizontalLine ((int) midY, 0.0f, (float) width);

    // Vignette effect (darker edges like old CRT)
    juce::ColourGradient
        vignette (juce::Colours::transparentBlack, width / 2.0f, height / 2.0f, juce::Colours::black.withAlpha (0.3f), 0.0f, 0.0f, true);
    g.setGradientFill (vignette);
    g.fillRect (0, 0, width, height);
}

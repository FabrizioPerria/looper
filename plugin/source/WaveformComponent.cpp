#include "WaveformComponent.h"

WaveformComponent::WaveformComponent()
{
    startTimerHz (30);
}

WaveformComponent::~WaveformComponent()
{
    stopTimer();
}

void WaveformComponent::timerCallback()
{
    if (! loopTrack || ! loopTrack->isPrepared())
    {
        repaint();
        return;
    }

    int length = loopTrack->getLength();
    int width = getWidth();

    if (length == 0 || width == 0)
    {
        repaint();
        return;
    }

    // Copy the buffer to avoid threading issues
    const auto& srcBuffer = loopTrack->getAudioBuffer();
    juce::AudioBuffer<float> bufferCopy (srcBuffer.getNumChannels(), length);

    for (int ch = 0; ch < srcBuffer.getNumChannels(); ++ch)
    {
        bufferCopy.copyFrom (ch, 0, srcBuffer, ch, 0, length);
    }

    // Update cache on background thread pool
    pool.addJob (
        [this, bufferCopy = std::move (bufferCopy), length, width]() mutable
        {
            cache.updateFromBuffer (bufferCopy, length, width);

            // Trigger repaint on message thread
            juce::MessageManager::callAsync ([this]() { repaint(); });
        });
}

void WaveformComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (! loopTrack || ! loopTrack->isPrepared())
    {
        g.setColour (juce::Colours::white);
        g.drawText ("No audio", getLocalBounds(), juce::Justification::centred);
        return;
    }

    int length = (int) loopTrack->getLength();
    if (length == 0)
    {
        g.setColour (juce::Colours::white);
        g.drawText ("Empty loop", getLocalBounds(), juce::Justification::centred);
        return;
    }

    auto readPos = loopTrack->getCurrentReadPosition();
    // Draw from cache if available
    if (! cache.isEmpty() && cache.getWidth() > 0)
    {
        paintFromCache (g, readPos);
    }
    else
    {
        // Fallback: draw directly (will be replaced by cache soon)
        paintDirect (g, readPos);
    }
}

void WaveformComponent::paintFromCache (juce::Graphics& g, size_t readPos)
{
    int width = cache.getWidth();
    int height = getHeight();
    float midY = height / 2.0f;

    int length = (int) loopTrack->getLength();
    int samplesPerPixel = std::max (1, length / width);
    int readPixel = readPos / samplesPerPixel;

    // Draw waveform with color based on playhead position
    for (int x = 0; x < width; ++x)
    {
        float min, max;
        if (cache.getMinMax (x, min, max, 0))
        {
            float y1 = midY - (max * midY * 0.9f);
            float y2 = midY - (min * midY * 0.9f);

            // Color based on distance from playhead
            juce::Colour waveColour;
            int distance = std::abs (x - readPixel);

            if (distance < 2)
            {
                // Bright red at playhead
                waveColour = juce::Colour (255, 50, 50);
            }
            else if (distance < 10)
            {
                // Red glow fade
                float fade = (10 - distance) / 10.0f;
                waveColour = juce::Colour::fromFloatRGBA (0.5f + 0.5f * fade,   // R
                                                          0.8f * (1.0f - fade), // G
                                                          0.2f * (1.0f - fade), // B
                                                          1.0f);
            }
            else
            {
                // Phosphor green
                waveColour = juce::Colour (0, 200, 50);
            }

            g.setColour (waveColour);
            g.drawLine ((float) x, y1, (float) x, y2, 1.5f);
        }
    }

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
    g.fillRect (getLocalBounds());
}

void WaveformComponent::paintDirect (juce::Graphics& g, size_t readPos)
{
    const auto& buffer = loopTrack->getAudioBuffer();
    int length = (int) loopTrack->getLength();
    int width = getWidth();
    int height = getHeight();
    float midY = height / 2.0f;
    int samplesPerPixel = std::max (1, length / width);
    int readPixel = readPos / samplesPerPixel;

    for (int x = 0; x < width; ++x)
    {
        int startSample = x * samplesPerPixel;
        int endSample = std::min (startSample + samplesPerPixel, length);

        float min = 0.0f, max = 0.0f;
        const float* data = buffer.getReadPointer (0);

        for (int i = startSample; i < endSample; ++i)
        {
            min = std::min (min, data[i]);
            max = std::max (max, data[i]);
        }

        float y1 = midY - (max * midY * 0.9f);
        float y2 = midY - (min * midY * 0.9f);

        // Same coloring logic as paintFromCache
        juce::Colour waveColour;
        int distance = std::abs (x - readPixel);

        if (distance < 2)
        {
            waveColour = juce::Colour (255, 50, 50);
        }
        else if (distance < 10)
        {
            float fade = (10 - distance) / 10.0f;
            waveColour = juce::Colour::fromFloatRGBA (0.5f + 0.5f * fade, 0.8f * (1.0f - fade), 0.2f * (1.0f - fade), 1.0f);
        }
        else
        {
            waveColour = juce::Colour (0, 200, 50).withAlpha (0.5f);
        }

        g.setColour (waveColour);
        g.drawLine ((float) x, y1, (float) x, y2, 1.5f);
    }

    // Scanlines
    g.setColour (juce::Colours::black.withAlpha (0.15f));
    for (int y = 0; y < height; y += 2)
    {
        g.drawHorizontalLine (y, 0.0f, (float) width);
    }

    // Playhead glow
    for (int i = 0; i < 15; ++i)
    {
        float alpha = (15 - i) / 15.0f * 0.4f;
        g.setColour (juce::Colour (255, 50, 50).withAlpha (alpha));
        if (readPixel - i >= 0) g.drawVerticalLine (readPixel - i, 0.0f, (float) height);
        if (readPixel + i < width) g.drawVerticalLine (readPixel + i, 0.0f, (float) height);
    }

    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawHorizontalLine ((int) midY, 0.0f, (float) width);
}

#pragma once

#include "IRenderer.h"
#include "profiler/PerfettoProfiler.h"
#include "ui/components/WaveformCache.h"
#include <JuceHeader.h>

class LinearRenderer : public IRenderer
{
public:
    LinearRenderer()
    {
    }
    void render (juce::Graphics& g, const WaveformCache& cache, int readPos, int length, int width, int height, bool recording) override
    {
        PERFETTO_FUNCTION();

        if (length < 0 || width < 0) return;

        int cacheWidth = cache.getWidth();
        if (cacheWidth <= 0 || length == 0)
        {
            g.setColour (juce::Colours::green);
            g.drawHorizontalLine (height / 2, 0.0f, (float) width);
            return;
        }
        int samplesPerPixel = std::max (1, length / cacheWidth);
        int readPixel = std::min (readPos / samplesPerPixel, cacheWidth - 1);

        // Draw waveform
        for (int x = 0; x < cacheWidth; ++x)
        {
            float min, max;
            if (cache.getMinMax (x, min, max, 0))
            {
                drawWaveformColumn (g, x, min, max, readPixel, height, recording);
            }
        }

        drawEffects (g, readPixel, cacheWidth, height, recording);
    }

private:
    void drawWaveformColumn (juce::Graphics& g, int x, float min, float max, int readPixel, int height, bool recording)
    {
        PERFETTO_FUNCTION();
        float midY = height / 2.0f;
        float y1 = midY - (max * midY * 0.9f);
        float y2 = midY - (min * midY * 0.9f);

        juce::Colour waveColour = getWaveformColour (x, readPixel, recording);
        g.setColour (waveColour);
        g.drawLine ((float) x, y1, (float) x, y2, 1.5f);
    }

    void drawEffects (juce::Graphics& g, int readPixel, int width, int height, bool recording)
    {
        PERFETTO_FUNCTION();
        float midY = height / 2.0f;

        // CRT scanlines
        g.setColour (juce::Colours::black.withAlpha (0.15f));
        for (int y = 0; y < height; y += 2)
        {
            g.drawHorizontalLine (y, 0.0f, (float) width);
        }

        // Playhead glow
        juce::Colour playheadColour = recording ? juce::Colour (255, 50, 50) : juce::Colours::white;
        for (int i = 0; i < 15; ++i)
        {
            float alpha = (15 - i) / 15.0f * 0.4f;
            g.setColour (playheadColour.withAlpha (alpha));
            if (readPixel - i >= 0) g.drawVerticalLine (readPixel - i, 0.0f, (float) height);
            if (readPixel + i < width) g.drawVerticalLine (readPixel + i, 0.0f, (float) height);
        }

        // Center line
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawHorizontalLine ((int) midY, 0.0f, (float) width);

        // Vignette
        juce::ColourGradient vignette (juce::Colours::transparentBlack,
                                       width / 2.0f,
                                       height / 2.0f,
                                       juce::Colours::black.withAlpha (0.3f),
                                       0.0f,
                                       0.0f,
                                       true);
        g.setGradientFill (vignette);
        g.fillRect (0, 0, width, height);
    }

    juce::Colour getWaveformColour (int x, int readPixel, bool recording)
    {
        PERFETTO_FUNCTION();
        int distance = std::abs (x - readPixel);

        if (distance < 2)
        {
            return recording ? juce::Colour (255, 50, 50) : juce::Colours::white;
        }
        else if (distance < 10)
        {
            float fade = (10 - distance) / 10.0f;
            if (recording)
            {
                return juce::Colour::fromFloatRGBA (0.5f + 0.5f * fade, 0.8f * (1.0f - fade), 0.2f * (1.0f - fade), 1.0f);
            }
            return juce::Colour (0, 200, 50).withAlpha (0.5f + 0.5f * fade);
        }
        else
        {
            return juce::Colour (0, 200, 50);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LinearRenderer)
};

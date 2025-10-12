#pragma once

#include "IRenderer.h"
#include "PerfettoProfiler.h"
#include <JuceHeader.h>

class LinearRenderer : public IRenderer
{
public:
    LinearRenderer()
    {
    }

    void drawWaveformColumn (juce::Graphics& g, int x, float min, float max, int readPixel, int height, bool recording) override
    {
        PERFETTO_FUNCTION();
        float midY = height / 2.0f;
        float y1 = midY - (max * midY * 0.9f);
        float y2 = midY - (min * midY * 0.9f);

        juce::Colour waveColour = getWaveformColour (x, readPixel, recording);
        g.setColour (waveColour);
        g.drawLine ((float) x, y1, (float) x, y2, 1.5f);
    }

    void drawCRTEffects (juce::Graphics& g, int readPixel, int width, int height) override
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

private:
    juce::Colour getWaveformColour (int x, int readPixel, bool recording) override
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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LinearRenderer)
};

#pragma once

#include <JuceHeader.h>

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    virtual void drawWaveformColumn (juce::Graphics& g, int x, float min, float max, int readPixel, int height, bool recording) = 0;
    virtual void drawCRTEffects (juce::Graphics& g, int readPixel, int width, int height) = 0;

private:
    virtual juce::Colour getWaveformColour (int x, int readPixel, bool recording) = 0;
};

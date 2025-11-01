#pragma once

#include "ui/helpers/WaveformCache.h"
#include <JuceHeader.h>

class IRenderer
{
public:
    virtual ~IRenderer() = default;
    virtual void render (juce::Graphics& g,
                         const WaveformCache& cache,
                         int readPos,
                         int length,
                         int width,
                         int height,
                         bool recording,
                         bool isSubLoop) = 0;
};

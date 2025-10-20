#pragma once

#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

class WaveformCache
{
public:
    WaveformCache() = default;
    ~WaveformCache() = default;

    void clear()
    {
        juce::ScopedLock sl (lock);
        minMaxData.clear();
        width.store (0, std::memory_order_relaxed);
        numChannels.store (0, std::memory_order_relaxed);
    }

    void updateFromBuffer (const juce::AudioBuffer<float>& source, int sourceLength, int targetWidth)
    {
        PERFETTO_FUNCTION();
        if (targetWidth <= 0 || sourceLength <= 0) return;

        int samplesPerPixel = sourceLength / targetWidth;
        if (samplesPerPixel < 1) return;

        juce::ScopedLock sl (lock);
        scratchBuffer.resize ((size_t) source.getNumChannels());

        for (int ch = 0; ch < source.getNumChannels(); ++ch)
        {
            scratchBuffer[(size_t) ch].resize ((size_t) targetWidth);
            const float* data = source.getReadPointer (ch);

            for (int pixel = 0; pixel < targetWidth; ++pixel)
            {
                int start = pixel * samplesPerPixel;
                int end = std::min (start + samplesPerPixel, sourceLength);

                float min = 0.0f, max = 0.0f;
                for (int i = start; i < end; ++i)
                {
                    min = std::min (min, data[i]);
                    max = std::max (max, data[i]);
                }
                scratchBuffer[(size_t) ch][(size_t) pixel] = { min, max };
            }
        }

        minMaxData.swap (scratchBuffer);
        width = targetWidth;
        numChannels = source.getNumChannels();
    }

    bool getMinMax (int pixelIndex, float& min, float& max, int channel) const
    {
        PERFETTO_FUNCTION();
        juce::ScopedLock sl (lock);

        if (channel < 0 || channel >= numChannels) return false;
        if (pixelIndex < 0 || pixelIndex >= width) return false;
        if (minMaxData.empty() || minMaxData[(size_t) channel].empty()) return false;

        const auto& data = minMaxData[(size_t) channel][(size_t) pixelIndex];
        min = data.first;
        max = data.second;
        return true;
    }

    int getWidth() const
    {
        PERFETTO_FUNCTION();
        return width.load();
    }
    int getNumChannels() const
    {
        PERFETTO_FUNCTION();
        return numChannels.load();
    }
    bool isEmpty() const
    {
        PERFETTO_FUNCTION();
        return width.load() == 0;
    }

private:
    std::vector<std::vector<std::pair<float, float>>> scratchBuffer;
    std::vector<std::vector<std::pair<float, float>>> minMaxData; // [channel][pixel]
    std::atomic<int> width { 0 };
    std::atomic<int> numChannels { 0 };
    mutable juce::CriticalSection lock;

    void downsample (std::vector<std::pair<float, float>>& destination, const float* source, int sourceLength, int targetWidth)
    {
        PERFETTO_FUNCTION();
        destination.resize ((size_t) targetWidth);
        int samplesPerPixel = sourceLength / targetWidth;
        if (samplesPerPixel < 1) return;

        for (int pixel = 0; pixel < targetWidth; ++pixel)
        {
            int start = pixel * samplesPerPixel;
            int end = std::min (start + samplesPerPixel, sourceLength);

            float min = 0.0f, max = 0.0f;
            for (int i = start; i < end; ++i)
            {
                min = std::min (min, source[i]);
                max = std::max (max, source[i]);
            }
            destination[(size_t) pixel] = { min, max };
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformCache)
};

#pragma once

#include <JuceHeader.h>

class WaveformCache
{
public:
    WaveformCache() = default;
    ~WaveformCache() = default;

    void updateFromBuffer (const juce::AudioBuffer<float>& source, int sourceLength, int targetWidth)
    {
        if (targetWidth <= 0 || sourceLength <= 0) return;

        int samplesPerPixel = sourceLength / targetWidth;
        if (samplesPerPixel < 1) return;

        std::vector<std::vector<std::pair<float, float>>> newData;
        newData.resize (source.getNumChannels());

        for (int ch = 0; ch < source.getNumChannels(); ++ch)
        {
            newData[ch].resize (targetWidth);
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
                newData[ch][pixel] = { min, max };
            }
        }

        juce::ScopedLock sl (lock);
        minMaxData.swap (newData);
        width = targetWidth;
        numChannels = source.getNumChannels();
    }

    bool getMinMax (int pixelIndex, float& min, float& max, int channel) const
    {
        juce::ScopedLock sl (lock);

        if (channel < 0 || channel >= numChannels) return false;
        if (pixelIndex < 0 || pixelIndex >= width) return false;
        if (minMaxData.empty() || minMaxData[channel].empty()) return false;

        const auto& data = minMaxData[channel][pixelIndex];
        min = data.first;
        max = data.second;
        return true;
    }

    int getWidth() const
    {
        return width.load();
    }
    int getNumChannels() const
    {
        return numChannels.load();
    }
    bool isEmpty() const
    {
        return width.load() == 0;
    }

private:
    std::vector<std::vector<std::pair<float, float>>> minMaxData; // [channel][pixel]
    std::atomic<int> width { 0 };
    std::atomic<int> numChannels { 0 };
    mutable juce::CriticalSection lock;

    void downsample (std::vector<std::pair<float, float>>& destination, const float* source, int sourceLength, int targetWidth)
    {
        destination.resize (targetWidth);
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
            destination[pixel] = { min, max };
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformCache)
};

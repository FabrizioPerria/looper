#pragma once
#include <JuceHeader.h>

class LevelMeter
{
public:
    LevelMeter() = default;
    ~LevelMeter() = default;

    void reset()
    {
        for (size_t channel = 0; channel < peakLevels.size(); ++channel)
        {
            peakLevels[channel].store (0.0f);
            rmsLevels[channel].store (0.0f);
        }
    }

    void prepare (int numChannels)
    {
        if (peakLevels.size() != numChannels)
        {
            peakLevels = std::vector<std::atomic<float>> (numChannels);
            rmsLevels = std::vector<std::atomic<float>> (numChannels);
            reset();
        }
    }

    void processBuffer (const juce::AudioBuffer<float>& buffer)
    {
        for (size_t channel = 0; channel < (size_t) buffer.getNumChannels(); ++channel)
        {
            rmsLevels[channel].store (juce::jmax (peakLevels[channel].load() * decayFactor,
                                                  buffer.getRMSLevel ((int) channel, 0, buffer.getNumSamples())));
            peakLevels[channel].store (juce::jmax (peakLevels[channel].load() * decayFactor,
                                                   buffer.getMagnitude ((int) channel, 0, buffer.getNumSamples())));
        }
        std::cout << "Peak Levels: ";
        for (const auto& level : peakLevels)
        {
            std::cout << level.load() << " ";
        }

        std::cout << std::endl << "RMS Levels: ";

        for (const auto& level : rmsLevels)
        {
            std::cout << level.load() << " ";
        }
        std::cout << std::endl;
    }

    // Call this from the UI thread to get current peak level for a channel
    float getPeakLevel (int channel) const { return peakLevels[(size_t) channel].load(); }

    // Call this from the UI thread to get current RMS level for a channel
    float getRMSLevel (int channel) const { return rmsLevels[(size_t) channel].load(); }

private:
    static constexpr float decayFactor = 0.95f; // Decay factor for smoothing

    std::vector<std::atomic<float>> peakLevels;
    std::vector<std::atomic<float>> rmsLevels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

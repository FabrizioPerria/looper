#pragma once
#include <JuceHeader.h>

class ChannelContext
{
public:
    ChannelContext() {}

    void clear()
    {
        peakLevel.store (0.0f);
        rmsLevel.store (0.0f);
        clipCount.store (0);
    }

    void update (const ChannelContext* other)
    {
        peakLevel.store (juce::jmax (peakLevel.load(), other->peakLevel.load()));
        rmsLevel.store (juce::jmax (rmsLevel.load(), other->rmsLevel.load()));
        clipCount.store (clipCount.load() + other->clipCount.load());
    }

    void setPeakLevel (float level) { peakLevel.store (level); }
    void setRMSLevel (float level) { rmsLevel.store (level); }
    void incrementClipCount() { clipCount.fetch_add (1); }

    float getPeakLevel() const { return peakLevel.load(); }
    float getRMSLevel() const { return rmsLevel.load(); }
    int getClipCount() const { return clipCount.load(); }

private:
    std::atomic<float> peakLevel { 0.0f };
    std::atomic<float> rmsLevel { 0.0f };
    std::atomic<int> clipCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelContext)
};

class StereoMeterContext
{
public:
    StereoMeterContext() {}
    void clear()
    {
        leftChannel->clear();
        rightChannel->clear();
    }

    void update (const StereoMeterContext& other)
    {
        leftChannel->update (other.getLeftChannel());
        rightChannel->update (other.getRightChannel());
    }

    ChannelContext* getLeftChannel() const { return leftChannel.get(); }
    ChannelContext* getRightChannel() const { return rightChannel.get(); }

private:
    std::unique_ptr<ChannelContext> leftChannel = std::make_unique<ChannelContext>();
    std::unique_ptr<ChannelContext> rightChannel = std::make_unique<ChannelContext>();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoMeterContext)
};

class LevelMeter
{
public:
    LevelMeter() {}

    void clear() { meterContext->clear(); }

    void prepare (int numChannels) { clear(); }

    void processBuffer (const juce::AudioBuffer<float>& buffer)
    {
        auto leftChannel = meterContext->getLeftChannel();
        leftChannel->setRMSLevel (juce::jmax (leftChannel->getRMSLevel() * decayFactor, //
                                              buffer.getRMSLevel (0, 0, buffer.getNumSamples())));
        leftChannel->setPeakLevel (juce::jmax (leftChannel->getPeakLevel() * decayFactor,
                                               buffer.getMagnitude (0, 0, buffer.getNumSamples())));

        auto rightChannel = meterContext->getRightChannel();
        rightChannel->setRMSLevel (juce::jmax (rightChannel->getRMSLevel() * decayFactor,
                                               buffer.getRMSLevel (1, 0, buffer.getNumSamples())));
        rightChannel->setPeakLevel (juce::jmax (rightChannel->getPeakLevel() * decayFactor,
                                                buffer.getMagnitude (1, 0, buffer.getNumSamples())));
    }

    // Call this from the UI thread to get current peak level for a channel
    float getPeakLevel (int channel) const
    {
        return (channel == 0) ? meterContext->getLeftChannel()->getPeakLevel() //
                              : meterContext->getRightChannel()->getPeakLevel();
    }

    // Call this from the UI thread to get current RMS level for a channel
    float getRMSLevel (int channel) const
    {
        return (channel == 0) ? meterContext->getLeftChannel()->getRMSLevel() //
                              : meterContext->getRightChannel()->getRMSLevel();
    }

    StereoMeterContext& getMeterContext() const { return *meterContext; }

private:
    static constexpr float decayFactor = 0.95f; // Decay factor for smoothing

    std::unique_ptr<StereoMeterContext> meterContext = std::make_unique<StereoMeterContext>();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

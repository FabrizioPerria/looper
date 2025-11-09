#pragma once
#include "engine/Constants.h"
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
        peakLevel.store (other->peakLevel.load());
        rmsLevel.store (other->rmsLevel.load());
        clipCount.store (other->clipCount.load());
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

    void prepare (int /**/) { clear(); }

    void processBuffer (const juce::AudioBuffer<float>& buffer)
    {
        auto leftChannel = meterContext->getLeftChannel();
        float leftDecayCurrentRMS = leftChannel->getRMSLevel() * DECAY_FACTOR;
        float leftDecayNextRMS = buffer.getRMSLevel (LEFT_CHANNEL, 0, buffer.getNumSamples());

        float leftDecayCurrentPeak = leftChannel->getPeakLevel() * DECAY_FACTOR;
        float leftDecayNextPeak = buffer.getMagnitude (LEFT_CHANNEL, 0, buffer.getNumSamples());
        leftChannel->setRMSLevel (juce::jmax (leftDecayCurrentRMS, leftDecayNextRMS));
        leftChannel->setPeakLevel (juce::jmax (leftDecayCurrentPeak, leftDecayNextPeak));

        auto rightChannel = meterContext->getRightChannel();
        float rightDecayCurrentRMS = rightChannel->getRMSLevel() * DECAY_FACTOR;
        float rightDecayNextRMS = buffer.getRMSLevel (RIGHT_CHANNEL, 0, buffer.getNumSamples());
        float rightDecayCurrentPeak = rightChannel->getPeakLevel() * DECAY_FACTOR;
        float rightDecayNextPeak = buffer.getMagnitude (RIGHT_CHANNEL, 0, buffer.getNumSamples());
        rightChannel->setRMSLevel (juce::jmax (rightDecayCurrentRMS, rightDecayNextRMS));
        rightChannel->setPeakLevel (juce::jmax (rightDecayCurrentPeak, rightDecayNextPeak));
    }

    // Call this from the UI thread to get current peak level for a channel
    float getPeakLevel (int channel) const
    {
        return (channel == LEFT_CHANNEL) ? meterContext->getLeftChannel()->getPeakLevel() //
                                         : meterContext->getRightChannel()->getPeakLevel();
    }

    // Call this from the UI thread to get current RMS level for a channel
    float getRMSLevel (int channel) const
    {
        return (channel == LEFT_CHANNEL) ? meterContext->getLeftChannel()->getRMSLevel() //
                                         : meterContext->getRightChannel()->getRMSLevel();
    }

    StereoMeterContext& getMeterContext() const { return *meterContext; }

private:
    std::unique_ptr<StereoMeterContext> meterContext = std::make_unique<StereoMeterContext>();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

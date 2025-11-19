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

class MeterContext
{
public:
    MeterContext (int numChannels)
    {
        for (int i = 0; i < numChannels; ++i)
            this->channels.push_back (std::make_unique<ChannelContext>());
    }
    void clear()
    {
        for (auto& channel : channels)
            channel->clear();
    }

    void update (const MeterContext& other)
    {
        for (int channel = 0; channel < channels.size(); ++channel)
        {
            auto* channelCtx = other.getChannel (channel);
            auto* currentChannelCtx = channels[(size_t) channel].get();
            currentChannelCtx->update (channelCtx);
        }
    }

    ChannelContext* getChannel (int channel) const { return channels[(size_t) channel].get(); }

private:
    std::vector<std::unique_ptr<ChannelContext>> channels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterContext)
};

class LevelMeter
{
public:
    LevelMeter() {}

    void clear() { meterContext->clear(); }

    void prepare (int numChannels) { meterContext = std::make_unique<MeterContext> (numChannels); }

    void processBuffer (const juce::AudioBuffer<float>& buffer)
    {
        for (auto i = 0; i < buffer.getNumChannels(); ++i)
        {
            auto* leftChannel = meterContext->getChannel (i);
            float leftDecayCurrentRMS = leftChannel->getRMSLevel() * DECAY_FACTOR;
            float leftDecayNextRMS = buffer.getRMSLevel (i, 0, buffer.getNumSamples());

            // float leftDecayCurrentPeak = leftChannel->getPeakLevel() * DECAY_FACTOR;
            // float leftDecayNextPeak = buffer.getMagnitude (i, 0, buffer.getNumSamples());
            // leftChannel->setRMSLevel (juce::jmax (leftDecayCurrentRMS, leftDecayNextRMS));
            // leftChannel->setPeakLevel (juce::jmax (leftDecayCurrentPeak, leftDecayNextPeak));
        }
    }

    // Call this from the UI thread to get current peak level for a channel
    float getPeakLevel (int channel) const { return meterContext->getChannel (channel)->getPeakLevel(); }

    // Call this from the UI thread to get current RMS level for a channel
    float getRMSLevel (int channel) const { return meterContext->getChannel (channel)->getRMSLevel(); }

    MeterContext& getMeterContext() const { return *meterContext; }

private:
    std::unique_ptr<MeterContext> meterContext;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

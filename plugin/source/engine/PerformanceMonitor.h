#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <deque>

class PerformanceMonitor
{
public:
    PerformanceMonitor() = default;

    void prepareToPlay (double sampleRate, int blockSize)
    {
        this->sampleRate = sampleRate;
        this->blockSize = blockSize;
        expectedBlockTimeMs = (blockSize / sampleRate) * 1000.0;

        reset();
    }

    void reset()
    {
        cpuLoad.store (0.0f, std::memory_order_relaxed);
        peakCpuLoad.store (0.0f, std::memory_order_relaxed);
        averageBlockTimeMs.store (0.0f, std::memory_order_relaxed);
        peakBlockTimeMs.store (0.0f, std::memory_order_relaxed);
        xrunCount.store (0, std::memory_order_relaxed);
        totalBlocksProcessed.store (0, std::memory_order_relaxed);
    }

    // Call at start of audio callback
    void startBlock() { blockStartTime = juce::Time::getHighResolutionTicks(); }

    // Call at end of audio callback
    void endBlock()
    {
        auto endTime = juce::Time::getHighResolutionTicks();
        auto elapsedTicks = endTime - blockStartTime;
        double blockTimeMs = juce::Time::highResolutionTicksToSeconds (elapsedTicks) * 1000.0;

        // Update block time statistics
        updateBlockTimeStats (blockTimeMs);

        // Calculate CPU load as percentage of available time
        float load = (float) (blockTimeMs / expectedBlockTimeMs);
        cpuLoad.store (load, std::memory_order_relaxed);

        // Track peak
        float currentPeak = peakCpuLoad.load (std::memory_order_relaxed);
        if (load > currentPeak)
        {
            peakCpuLoad.store (load, std::memory_order_relaxed);
        }

        // Detect buffer overrun (processing took longer than available time)
        if (blockTimeMs > expectedBlockTimeMs)
        {
            xrunCount.fetch_add (1, std::memory_order_relaxed);
        }

        totalBlocksProcessed.fetch_add (1, std::memory_order_relaxed);
    }

    // Getters for UI thread
    float getCpuLoad() const { return cpuLoad.load (std::memory_order_relaxed); }
    float getPeakCpuLoad() const { return peakCpuLoad.load (std::memory_order_relaxed); }
    float getAverageBlockTimeMs() const { return averageBlockTimeMs.load (std::memory_order_relaxed); }
    float getPeakBlockTimeMs() const { return peakBlockTimeMs.load (std::memory_order_relaxed); }
    int getXrunCount() const { return xrunCount.load (std::memory_order_relaxed); }
    int getTotalBlocksProcessed() const { return totalBlocksProcessed.load (std::memory_order_relaxed); }
    double getExpectedBlockTimeMs() const { return expectedBlockTimeMs; }
    int getBlockSize() const { return blockSize; }
    double getSampleRate() const { return sampleRate; }

    void resetPeaks()
    {
        peakCpuLoad.store (0.0f, std::memory_order_relaxed);
        peakBlockTimeMs.store (0.0f, std::memory_order_relaxed);
    }

private:
    void updateBlockTimeStats (double blockTimeMs)
    {
        // Simple moving average
        const int windowSize = 100;

        blockTimeSamples.push_back (blockTimeMs);
        if (blockTimeSamples.size() > windowSize)
        {
            blockTimeSamples.pop_front();
        }

        // Calculate average
        double sum = 0.0;
        double peak = 0.0;
        for (auto time : blockTimeSamples)
        {
            sum += time;
            peak = std::max (peak, time);
        }

        averageBlockTimeMs.store ((float) (sum / blockTimeSamples.size()), std::memory_order_relaxed);
        peakBlockTimeMs.store ((float) peak, std::memory_order_relaxed);
    }

    std::atomic<float> cpuLoad { 0.0f };
    std::atomic<float> peakCpuLoad { 0.0f };
    std::atomic<float> averageBlockTimeMs { 0.0f };
    std::atomic<float> peakBlockTimeMs { 0.0f };
    std::atomic<int> xrunCount { 0 };
    std::atomic<int> totalBlocksProcessed { 0 };

    juce::int64 blockStartTime = 0;
    double expectedBlockTimeMs = 0.0;
    double sampleRate = 0.0;
    int blockSize = 0;

    std::deque<double> blockTimeSamples;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformanceMonitor)
};

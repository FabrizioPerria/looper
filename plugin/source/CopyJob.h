#pragma once

#include "juce_core/juce_core.h"
#include <JuceHeader.h>
#include <cstdint>

class CopyJobManager
{
public:
    CopyJobManager() = default;

    void prepare (juce::AudioBuffer<float>* destination,
                  const juce::AudioBuffer<float>* source,
                  const int totalNumSamples,
                  const int writePosBeforeWrap,
                  const int samplesBeforeWrap,
                  const int writePosAfterWrap,
                  const int samplesAfterWrap,
                  std::atomic<uint32>* copyState,
                  const bool overdub = false,
                  const float overdubOldGainValue = 1.0f,
                  const float overdubNewGainValue = 1.0f)
    {
        dest = destination;
        src = source;
        numSamples = totalNumSamples;
        writePositionBeforeWrap = writePosBeforeWrap;
        numSamplesBeforeWrap = samplesBeforeWrap;
        writePositionAfterWrap = writePosAfterWrap;
        numSamplesAfterWrap = samplesAfterWrap;
        state = copyState;
        shouldOverdub = overdub;
        overdubOldGain = overdubOldGainValue;
        overdubNewGain = overdubNewGainValue;
    }

protected:
    // try to start a snapshot: succeed if neither LOOP_BIT nor WANT_LOOP_BIT are set.
    // multiple input jobs can succeed concurrently thanks to CAS increments.
    bool tryBeginSnapshot()
    {
        uint32_t s = state->load (std::memory_order_acquire);
        while (true)
        {
            // If a loop is running or a loop wants to run, refuse to start a new snapshot
            if ((s & (LOOP_BIT | WANT_LOOP_BIT)) != 0) return false;

            uint32_t desired = s + SNAPSHOT_INC; // increment snapshot count
            if (state->compare_exchange_weak (s, desired, std::memory_order_acq_rel, std::memory_order_acquire)) return true;
            // s is updated with current state; retry
        }
    }

    // finish a snapshot (simple decrement)
    void endSnapshot()
    {
        state->fetch_sub (SNAPSHOT_INC, std::memory_order_acq_rel);
    }

    // Indicate intent to run loop: set WANT_LOOP_BIT so no new snapshots start.
    // Returns once WANT_LOOP_BIT is set.
    void setWantLoop()
    {
        uint32_t s = state->load (std::memory_order_acquire);
        while (true)
        {
            if ((s & WANT_LOOP_BIT) != 0) // already set
                return;
            uint32_t desired = s | WANT_LOOP_BIT;
            if (state->compare_exchange_weak (s, desired, std::memory_order_acq_rel, std::memory_order_acquire)) return;
        }
    }

    // Try to begin loop only if snapshot count == 0 and WANT_LOOP_BIT is set (optional),
    // or just snapshot count == 0. We'll use WANT_LOOP_BIT to prevent new snapshots.
    bool tryBeginLoop()
    {
        uint32_t s = state->load (std::memory_order_acquire);
        while (true)
        {
            // only start loop if no snapshots are active (snapshot bits 2..)
            if ((s & SNAPSHOT_MASK) != 0) return false;

            // set LOOP_BIT and clear WANT_LOOP_BIT atomically (so no further snapshots can start).
            uint32_t desired = (s | LOOP_BIT) & ~WANT_LOOP_BIT;
            if (state->compare_exchange_weak (s, desired, std::memory_order_acq_rel, std::memory_order_acquire)) return true;
            // otherwise retry with updated s
        }
    }

    void endLoop()
    {
        state->fetch_and (~LOOP_BIT, std::memory_order_release);
    }

    static constexpr uint32_t LOOP_BIT = 1ull << 0;
    static constexpr uint32_t WANT_LOOP_BIT = 1 << 1;
    static constexpr uint32_t SNAPSHOT_INC = 1 << 2;
    static constexpr uint32_t SNAPSHOT_MASK = ~((1u << 2) - 1); // all bits from bit2 up

    juce::AudioBuffer<float>* dest;
    const juce::AudioBuffer<float>* src;
    int numSamples;

    int writePositionBeforeWrap;
    int numSamplesBeforeWrap;
    int writePositionAfterWrap;
    int numSamplesAfterWrap;

    std::atomic<uint32_t>* state = nullptr;

    bool shouldOverdub = false;
    float overdubOldGain = 1.0f;
    float overdubNewGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CopyJobManager)
};

class CopyLoopJob : public CopyJobManager, public juce::ThreadPoolJob
{
public:
    CopyLoopJob() : ThreadPoolJob ("CopyLoopJob")
    {
    }

    JobStatus runJob() override
    {
        setWantLoop();

        while (! tryBeginLoop())
            std::this_thread::yield();

        for (int ch = 0; ch < dest->getNumChannels(); ++ch)
            juce::FloatVectorOperations::copy (dest->getWritePointer (ch), src->getReadPointer (ch), numSamples);

        endLoop();
        return jobHasFinished;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CopyLoopJob)
};

class CopyInputJob : public CopyJobManager, public juce::ThreadPoolJob
{
public:
    CopyInputJob() : ThreadPoolJob ("CopyInputJob")
    {
    }

    JobStatus runJob() override
    {
        while (! tryBeginSnapshot())
            std::this_thread::yield();

        for (int ch = 0; ch < dest->getNumChannels(); ++ch)
        {
            if (numSamplesBeforeWrap > 0)
                copyInputToLoopBuffer ((uint) ch, src->getReadPointer (ch), (uint) writePositionBeforeWrap, (uint) numSamplesBeforeWrap);

            if (numSamplesAfterWrap > 0 && shouldOverdub)
                copyInputToLoopBuffer ((uint) ch,
                                       src->getReadPointer (ch) + numSamplesBeforeWrap,
                                       (uint) writePositionAfterWrap,
                                       (uint) numSamplesAfterWrap);
        }

        endSnapshot();
        return jobHasFinished;
    }

private:
    void copyInputToLoopBuffer (const uint ch, const float* bufPtr, const uint offset, const uint numSamplesToCopy)
    {
        auto* audioBufferPtr = dest->getWritePointer ((int) ch) + offset;

        juce::FloatVectorOperations::multiply (audioBufferPtr, shouldOverdub ? overdubOldGain : 0, (int) numSamplesToCopy);
        juce::FloatVectorOperations::addWithMultiply (audioBufferPtr, bufPtr, overdubNewGain, (int) numSamplesToCopy);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CopyInputJob)
};

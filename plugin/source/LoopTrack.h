#pragma once

#include "CopyJob.h"
#include "LoopFifo.h"
#include "UndoBuffer.h"
#include "juce_core/juce_core.h"
#include <JuceHeader.h>
#include <cstdint>

class LoopTrack
{
public:
    LoopTrack()
    {
    }

    ~LoopTrack()
    {
        releaseResources();
    }

    void prepareToPlay (const double currentSampleRate,
                        const uint maxBlockSize,
                        const uint numChannels,
                        const uint maxSeconds = MAX_SECONDS_HARD_LIMIT,
                        const size_t maxUndoLayers = MAX_UNDO_LAYERS);
    void releaseResources();

    void processRecord (const juce::AudioBuffer<float>& input, const uint numSamples);
    void finalizeLayer();
    void processPlayback (juce::AudioBuffer<float>& output, const uint numSamples);

    void clear();
    void undo();
    void redo();

    const juce::AudioBuffer<float>& getAudioBuffer() const
    {
        while (backgroundPool.getNumJobs() > 0)
            juce::Thread::sleep (0);
        return *audioBuffer;
    }

    double getSampleRate() const
    {
        return sampleRate;
    }

    int getLength() const
    {
        return (int) length;
    }

    void setLength (const uint newLength)
    {
        length = newLength;
    }

    void setCrossFadeLength (const uint newLength)
    {
        crossFadeLength = newLength;
    }

    bool isPrepared() const
    {
        return alreadyPrepared;
    }

    void setOverdubGains (const double oldGain, const double newGain)
    {
        overdubNewGain = std::clamp (newGain, 0.0, 2.0);
        overdubOldGain = std::clamp (oldGain, 0.0, 2.0);
    }

    double getOverdubNewGain() const
    {
        return overdubNewGain;
    }

    double getOverdubOldGain() const
    {
        return overdubOldGain;
    }

    const UndoBuffer& getUndoBuffer() const
    {
        return undoBuffer;
    }

private:
    std::unique_ptr<juce::AudioBuffer<float>> audioBuffer = std::make_unique<juce::AudioBuffer<float>>();
    std::unique_ptr<juce::AudioBuffer<float>> tmpBuffer = std::make_unique<juce::AudioBuffer<float>>();

    juce::ThreadPool backgroundPool { MAX_POOL_SIZE };

    std::vector<std::unique_ptr<CopyInputJob>> copyBlockJobs;
    std::vector<std::unique_ptr<CopyLoopJob>> copyLoopJobs;
    std::vector<juce::AudioBuffer<float>*> blockSnapshots;

    std::unique_ptr<std::atomic<uint32_t>> state = std::make_unique<std::atomic<uint32_t>> (0);

    UndoBuffer undoBuffer;

    double sampleRate = 0.0;

    LoopFifo fifo;

    uint length = 0;
    uint provisionalLength = 0;
    uint crossFadeLength = 0;

    bool isRecording = false;
    bool alreadyPrepared = false;

    double overdubNewGain = 1.0;
    double overdubOldGain = 1.0;

    void processRecordChannel (const juce::AudioBuffer<float>& input, const uint numSamples, const uint ch);
    void updateLoopLength (const uint numSamples, const uint bufferSamples);
    void saveToUndoBuffer();
    void copyInputToLoopBuffer (const uint ch, const float* bufPtr, const uint offset, const uint numSamples);
    void advanceWritePos (const uint numSamples, const uint bufferSamples);
    void advanceReadPos (const uint numSamples, const uint bufferSamples);

    void processPlaybackChannel (juce::AudioBuffer<float>& output, const uint numSamples, const uint ch);

    bool shouldNotRecordInputBuffer (const juce::AudioBuffer<float> input, const uint numSamples) const
    {
        return numSamples == 0 || (uint) input.getNumSamples() < numSamples || ! isPrepared()
               || input.getNumChannels() != audioBuffer->getNumChannels();
    }

    bool shouldNotPlayback (const uint numSamples) const
    {
        return ! isPrepared() || length == 0 || numSamples == 0;
    }

    bool shouldOverdub() const
    {
        return length > 0;
    }

    static const uint MAX_SECONDS_HARD_LIMIT = 3600; // 1 hour max buffer size
    static const uint MAX_UNDO_LAYERS = 5;
    static const int MAX_POOL_SIZE = 10;

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

    static constexpr uint32_t LOOP_BIT = 1 << 0;
    static constexpr uint32_t WANT_LOOP_BIT = 1 << 1;
    static constexpr uint32_t SNAPSHOT_INC = 1 << 2;
    static constexpr uint32_t SNAPSHOT_MASK = ~((1u << 2) - 1); // all bits from bit2 up
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTrack)
};

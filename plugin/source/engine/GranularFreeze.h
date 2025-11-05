#pragma once
#include "BufferManager.h"
#include "Constants.h"
#include <JuceHeader.h>
#include <atomic>

class GranularFreeze
{
public:
    struct alignas (16) Grain
    {
        float position = 0.0f;
        float envPosition = 0.0f;
        float envIncrement = 0.0f;
        float increment = 1.0f;
        float pitchMod = 1.0f;
        float ampMod = 1.0f;
        bool isActive = false;
        char padding[7];
    };

    GranularFreeze() : snapshotThread ("Snapshot")
    {
        snapshotThread.startThread (juce::Thread::Priority::low);
        snapshotThread.owner = this;
    }

    ~GranularFreeze() { snapshotThread.stopThread (2000); }

    double getSampleRate() const { return sampleRate_; }

    std::array<Grain, MAX_GRAINS> getActiveGrains() const { return grains; }

    const juce::AudioBuffer<float>& getFrozenBuffer() const { return frozenBuffer; }
    int getBufferSize() const { return bufferSize_; }

    void prepareToPlay (double sampleRate, int numChannels)
    {
        const int bufferSize = static_cast<int> (sampleRate * FREEZE_BUFFER_DURATION_SECONDS);

        frozenBuffer.setSize (numChannels, bufferSize);
        frozenBuffer.clear();
        circularBuffer.prepareToPlay (numChannels, bufferSize);

        sampleRate_ = sampleRate;
        bufferSize_ = bufferSize;
        bufferSizeFloat_ = static_cast<float> (bufferSize);
        isFrozen.store (false);

        grainEnvIncrement = 1.0f / static_cast<float> (GRAIN_LENGTH);

        std::memset (grains.data(), 0, sizeof (Grain) * MAX_GRAINS);

        createWindowLookup();
        createModulationLookup();

        modPhaseInc = MOD_RATE / static_cast<float> (sampleRate);
    }

    void toggleActiveState()
    {
        if (! isFrozen.load())
        {
            needsSnapshot.store (true);
            while (needsSnapshot.load()) // Wait for snapshot
            {
                juce::Thread::sleep (1);
            }

            isFrozen.store (true);
            nextGrainTime = 0;
        }
        else
        {
            isFrozen.store (false);

            juce::Thread::launch ([this]() { std::memset (grains.data(), 0, sizeof (Grain) * MAX_GRAINS); });
        }
    }

    void releaseResources()
    {
        circularBuffer.releaseResources();
        frozenBuffer.setSize (0, 0);
        isFrozen.store (false);
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        const auto numSamples = buffer.getNumSamples();

        circularBuffer.writeToAudioBuffer ([] (float* dest, const float* src, int numSamples, bool)
                                           { juce::FloatVectorOperations::copy (dest, src, numSamples); },
                                           buffer,
                                           numSamples,
                                           true,
                                           false);

        if (! isFrozen.load()) return;

        float* leftChannel = buffer.getWritePointer (0);
        float* rightChannel = buffer.getWritePointer (1);
        const float* frozenL = frozenBuffer.getReadPointer (0);
        const float* frozenR = frozenBuffer.getReadPointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            // Update mod phase
            modPhase += modPhaseInc;
            modPhase -= static_cast<float> (modPhase >= 1.0f);

            // Grain triggering
            if (--nextGrainTime <= 0)
            {
                triggerGrain();
                nextGrainTime = GRAIN_SPACING;
            }

            float leftSum = 0.0f;
            float rightSum = 0.0f;
            int activeCount = 0;

            const int modIdx = static_cast<int> (modPhase * MOD_TABLE_SIZE) & MOD_TABLE_MASK;

            // Unrolled grain loop for better CPU pipelining
            for (int g = 0; g < MAX_GRAINS; ++g)
            {
                Grain& grain = grains[g];

                // Branch prediction optimization - most grains are active during freeze
                if (! grain.isActive) continue;

                activeCount++;

                // Modulation lookup
                const int modOffset = (modIdx + (g * MOD_TABLE_SIZE / MAX_GRAINS)) & MOD_TABLE_MASK;
                const float pitchMod = pitchModTable[modOffset];
                const float ampMod = ampModTable[modOffset];

                // Window envelope
                const int envIdx = static_cast<int> (grain.envPosition * WINDOW_SIZE_1);
                const float env = windowTable[envIdx];

                // Sample interpolation
                const int pos1 = static_cast<int> (grain.position);
                const int pos2 = (pos1 + 1) % bufferSize_;
                const float frac = grain.position - static_cast<float> (pos1);

                const float sampleL = frozenL[pos1] + frac * (frozenL[pos2] - frozenL[pos1]);
                const float sampleR = frozenR[pos1] + frac * (frozenR[pos2] - frozenR[pos1]);

                const float amp = env * ampMod;
                leftSum += sampleL * amp;
                rightSum += sampleR * amp;

                // Update grain state
                grain.position += grain.increment * pitchMod;
                grain.position -= bufferSizeFloat_ * static_cast<float> (grain.position >= bufferSizeFloat_);

                grain.envPosition += grain.envIncrement;
                grain.isActive = grain.envPosition < 1.0f;
            }

            // Normalization with fast inverse sqrt
            if (activeCount > 0)
            {
                const float scale = 0.25f * juce::approximatelyEqual (activeCount, 1)
                                        ? 1.0f
                                        : (1.0f / std::sqrt (static_cast<float> (activeCount)));
                leftSum *= scale;
                rightSum *= scale;
            }

            // Hard clipping (fastest)
            leftChannel[i] += juce::jlimit (-1.0f, 1.0f, leftSum);
            rightChannel[i] += juce::jlimit (-1.0f, 1.0f, rightSum);
        }
    }

    bool isEnabled() const { return isFrozen.load(); }

private:
    // Background snapshot thread
    class SnapshotThread : public juce::Thread
    {
    public:
        SnapshotThread (const juce::String& name) : juce::Thread (name) {}

        void run() override
        {
            while (! threadShouldExit())
            {
                if (owner && owner->needsSnapshot.load())
                {
                    const int bufferSize = owner->circularBuffer.getNumSamples();
                    const int numChannels = owner->circularBuffer.getNumChannels();

                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        juce::FloatVectorOperations::copy (owner->frozenBuffer.getWritePointer (ch),
                                                           owner->circularBuffer.getReadPointer (ch),
                                                           bufferSize);
                    }

                    owner->needsSnapshot.store (false);
                }

                wait (5);
            }
        }

        GranularFreeze* owner = nullptr;
    } snapshotThread;

    void createWindowLookup()
    {
        windowTable.resize (WINDOW_SIZE);
        for (int i = 0; i < WINDOW_SIZE; ++i)
        {
            const float x = static_cast<float> (i) * (1.0f / WINDOW_SIZE_1);
            windowTable[i] = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * x));
        }
    }

    void createModulationLookup()
    {
        pitchModTable.resize (MOD_TABLE_SIZE);
        ampModTable.resize (MOD_TABLE_SIZE);

        const float invTableSize = 1.0f / static_cast<float> (MOD_TABLE_SIZE);

        for (int i = 0; i < MOD_TABLE_SIZE; ++i)
        {
            const float phase = static_cast<float> (i) * invTableSize;
            const float modValue = std::sin (juce::MathConstants<float>::twoPi * phase);

            pitchModTable[i] = 1.0f + (modValue * PITCH_MOD_DEPTH);
            ampModTable[i] = juce::jlimit (0.7f, 1.0f, 1.0f + (modValue * AMP_MOD_DEPTH));
        }
    }

    void triggerGrain()
    {
        for (auto& grain : grains)
        {
            if (! grain.isActive)
            {
                grain.isActive = true;
                grain.envPosition = 0.0f;
                grain.envIncrement = grainEnvIncrement;
                grain.position = random.nextFloat() * bufferSizeFloat_;
                grain.increment = 1.0f;
                return;
            }
        }
    }

    BufferManager circularBuffer;
    juce::AudioBuffer<float> frozenBuffer;
    std::array<Grain, MAX_GRAINS> grains;
    std::vector<float> windowTable;
    std::vector<float> pitchModTable;
    std::vector<float> ampModTable;

    double sampleRate_ = 0.0;
    int bufferSize_ = 0;
    float bufferSizeFloat_ = 0.0f;
    int nextGrainTime = 0;
    float grainEnvIncrement = 0.0f;
    float modPhase = 0.0f;
    float modPhaseInc = 0.0f;

    std::atomic<bool> isFrozen { false };
    std::atomic<bool> needsSnapshot { false };
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranularFreeze)
};

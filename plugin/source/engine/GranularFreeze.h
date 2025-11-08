#pragma once
#include "BufferManager.h"
#include "Constants.h"
#include "juce_gui_extra/juce_gui_extra.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>
class WindowTable
{
public:
    WindowTable() { createWindow(); }

    float operator[] (int index) const { return table[(size_t) index]; }

    size_t size() const { return WINDOW_SIZE; }

private:
    void createWindow()
    {
        for (int i = 0; i < WINDOW_SIZE; ++i)
        {
            const float x = static_cast<float> (i) * (1.0f / (static_cast<float> (WINDOW_SIZE) - 1.0f));
            table[(size_t) i] = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * x));
        }
    }

    static constexpr int WINDOW_SIZE = 2048;
    float table[WINDOW_SIZE];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WindowTable)
};

class Modulator
{
public:
    struct Parameters
    {
        float rate = 0.04f;        // MOD_RATE - speed of variation
        float pitchDepth = 0.005f; // PITCH_MOD_DEPTH - amount of pitch variation
        float ampDepth = 0.01f;    // AMP_MOD_DEPTH - amount of amplitude variation
    };

    void setParameters (const Parameters& params)
    {
        modRate = params.rate;
        updateModulationTables (params.pitchDepth, params.ampDepth);
    }

    Modulator() { createModulation(); }

    void prepare (double sampleRate) { modPhaseInc = modRate / static_cast<float> (sampleRate); }

    void updatePhase()
    {
        modPhase += modPhaseInc;
        modPhase -= static_cast<float> (modPhase >= 1.0f);
    }

    void reset() { modPhase = 0.0f; }

    struct ModulationValues
    {
        float pitchMod = 1.0f;
        float ampMod = 1.0f;
    };

    ModulationValues getModulation (int grainIndex, int numGrains) const
    {
        // Calculate base modulation index
        const int modIdx = static_cast<int> (modPhase * MOD_TABLE_SIZE) & MOD_TABLE_MASK;

        // Offset by grain position in pool
        const int modOffset = (modIdx + (grainIndex * MOD_TABLE_SIZE / numGrains)) & MOD_TABLE_MASK;

        return { pitchModTable[modOffset], ampModTable[modOffset] };
    }

    static constexpr int MOD_TABLE_SIZE = 1024;
    static constexpr int MOD_TABLE_MASK = MOD_TABLE_SIZE - 1;

private:
    void createModulation()
    {
        pitchModTable.resize (MOD_TABLE_SIZE);
        ampModTable.resize (MOD_TABLE_SIZE);

        static constexpr float PITCH_MOD_DEPTH = 0.005f;
        static constexpr float AMP_MOD_DEPTH = 0.01f;
        updateModulationTables (PITCH_MOD_DEPTH, AMP_MOD_DEPTH);
    }

    void updateModulationTables (float pitchDepth, float ampDepth)
    {
        for (int i = 0; i < MOD_TABLE_SIZE; ++i)
        {
            const float phase = static_cast<float> (i) / static_cast<float> (MOD_TABLE_SIZE);
            const float modValue = std::sin (juce::MathConstants<float>::twoPi * phase);

            pitchModTable[(size_t) i] = 1.0f + (modValue * pitchDepth);
            ampModTable[(size_t) i] = juce::jlimit (0.7f, 1.0f, 1.0f + (modValue * ampDepth));
        }
    }

    std::vector<float> pitchModTable;
    std::vector<float> ampModTable;
    float modPhase = 0.0f;
    float modPhaseInc = 0.0f;

    float modRate = 0.04f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Modulator)
};

class Grain
{
public:
    struct Parameters
    {
        float duration = 16384.0f; // GRAIN_LENGTH - controls grain size
        float density = 512.0f;    // GRAIN_SPACING - controls overlap
    };
    void trigger (float startPosition, Parameters params)
    {
        position = startPosition;
        envPosition = 0.0f;
        envIncrement = 1.0f / params.duration;
        increment = 1.0f;
        isActive = true;
    }

    struct SamplePair
    {
        float left = 0.0f;
        float right = 0.0f;
    };
    SamplePair process (const juce::AudioBuffer<float>& frozenBuffer, const WindowTable& window, float pitchMod, float ampMod)
    {
        if (! isActive) return { 0.0f, 0.0f };

        // Get envelope
        const int envIdx = static_cast<int> (envPosition * (float) window.size());
        const float env = window[envIdx];

        // Get interpolated samples
        const int pos1 = static_cast<int> (position);
        const int pos2 = (pos1 + 1) % frozenBuffer.getNumSamples();
        const float frac = position - static_cast<float> (pos1);

        const float* frozenL = frozenBuffer.getReadPointer (0);
        const float* frozenR = frozenBuffer.getReadPointer (1);

        const float sampleL = frozenL[pos1] + frac * (frozenL[pos2] - frozenL[pos1]);
        const float sampleR = frozenR[pos1] + frac * (frozenR[pos2] - frozenR[pos1]);

        // Apply envelope and modulation
        const float amp = env * ampMod;

        // Update positions
        position += increment * pitchMod;
        position = std::fmod (position, frozenBuffer.getNumSamples());

        envPosition += envIncrement;
        isActive = envPosition < 1.0f;

        return { sampleL * amp, sampleR * amp };
    }

    bool isPlaying() const { return isActive; }

    float position = 0.0f;
    float envPosition = 0.0f;

private:
    float increment = 1.0f;
    float envIncrement = 0.0f;
    bool isActive = false;
};

class GranularFreeze
{
public:
    class CloudController
    {
    public:
        struct Parameters
        {
            Grain::Parameters grainParams;
            Modulator::Parameters modParams;
            int maxGrains = 64;
            float positionSpread = 1.0f; // How much of the buffer to use
            float amplitude = 0.25f;
        };

        void setParameters (const Parameters& params)
        {
            cloudParams = params;
            modulator.setParameters (params.modParams);
            nextGrainTime = static_cast<int> (params.grainParams.density);
        }
        void prepare (double sampleRate, float bufferDuration)
        {
            sampleRate_ = sampleRate;
            clearAllGrains();
            bufferSizeFloat_ = sampleRate_ * bufferDuration;
        }

        std::array<Grain, MAX_GRAINS> getActiveGrains() const { return grains; }

        void processBlock (juce::AudioBuffer<float>& output, const juce::AudioBuffer<float>& frozenBuffer)
        {
            for (int i = 0; i < output.getNumSamples(); ++i)
            {
                modulator.updatePhase(); // Update global modulation phase

                if (--nextGrainTime <= 0)
                {
                    startNewGrain();
                    nextGrainTime = static_cast<int> (cloudParams.grainParams.density);
                }

                float leftSum = 0.0f;
                float rightSum = 0.0f;
                int activeCount = 0;

                // Process all active grains with correct phase offset
                for (size_t g = 0; g < cloudParams.maxGrains; ++g)
                {
                    if (! grains[g].isPlaying()) continue;

                    // Get modulation with grain index offset
                    auto [pitchMod, ampMod] = modulator.getModulation (g, cloudParams.maxGrains);
                    auto [left, right] = grains[g].process (frozenBuffer, window, pitchMod, ampMod);

                    leftSum += left;
                    rightSum += right;
                    activeCount++;
                }

                // Apply normalization and add to output
                if (activeCount > 0)
                {
                    const float scale = cloudParams.amplitude
                                        * (activeCount <= 1 ? 1.0f : fastInverseSqrt (static_cast<float> (activeCount)));

                    output.addSample (0, i, output.getSample (0, i) + juce::jlimit (-1.0f, 1.0f, leftSum * scale));
                    output.addSample (1, i, output.getSample (1, i) + juce::jlimit (-1.0f, 1.0f, rightSum * scale));
                }
            }
        }

        void triggerFreeze() { nextGrainTime = 0; }

        void stopFreeze()
        {
            clearAllGrains();
            modulator.reset();
        }

    private:
        void triggerGrainIfNeeded()
        {
            if (--nextGrainTime <= 0)
            {
                startNewGrain();
                nextGrainTime = cloudParams.grainParams.density;
            }
        }

        void startNewGrain()
        {
            for (auto& grain : grains)
            {
                if (! grain.isPlaying())
                {
                    float startPosition = random.nextFloat() * (bufferSizeFloat_ * cloudParams.positionSpread);
                    grain.trigger (startPosition, cloudParams.grainParams);
                    return;
                }
            }
        }

        void clearAllGrains() { std::memset (grains.data(), 0, sizeof (Grain) * MAX_GRAINS); }

        static float fastInverseSqrt (float x)
        {
            union
            {
                float f;
                uint32_t i;
            } conv = { x };
            conv.i = 0x5f3759df - (conv.i >> 1);
            conv.f = conv.f * (1.5f - 0.5f * x * conv.f * conv.f);
            return conv.f;
        }
        std::array<Grain, MAX_GRAINS> grains;
        WindowTable window;
        Modulator modulator;

        double sampleRate_ = 44100.0;
        int nextGrainTime = 0;
        float bufferSizeFloat_ = 0.0f;
        juce::Random random;
        Parameters cloudParams;

        // static constexpr int MAX_GRAINS = 64;
        // static constexpr int GRAIN_SPACING = 512;
    };

    GranularFreeze() : snapshotThread ("Snapshot")
    {
        snapshotThread.startThread (juce::Thread::Priority::low);
        snapshotThread.owner = this;
    }

    ~GranularFreeze() { snapshotThread.stopThread (2000); }

    double getSampleRate() const { return sampleRate_; }

    std::array<Grain, MAX_GRAINS> getActiveGrains() const { return cloudController.getActiveGrains(); }

    const juce::AudioBuffer<float>& getFrozenBuffer() const { return frozenBuffer; }
    int getBufferSize() const { return bufferSize_; }

    void prepareToPlay (double sampleRate, int numChannels)
    {
        constexpr float FREEZE_BUFFER_DURATION_SECONDS = 0.5f;
        const int bufferSize = static_cast<int> (sampleRate * FREEZE_BUFFER_DURATION_SECONDS);

        frozenBuffer.setSize (numChannels, bufferSize);
        frozenBuffer.clear();
        circularBuffer.prepareToPlay (numChannels, bufferSize);

        sampleRate_ = sampleRate;
        bufferSize_ = bufferSize;
        isFrozen.store (false);

        cloudController.prepare (sampleRate, FREEZE_BUFFER_DURATION_SECONDS);
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
            cloudController.setParameters (getDefaultParameters());
            cloudController.triggerFreeze();
        }
        else
        {
            isFrozen.store (false);
            cloudController.stopFreeze();
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

        cloudController.processBlock (buffer, frozenBuffer);
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

    void setParameters (const CloudController::Parameters& params) { cloudController.setParameters (params); }

    // Example presets
    static CloudController::Parameters getDefaultParameters()
    {
        CloudController::Parameters params;
        // Default values as they are now
        return params;
    }

    static CloudController::Parameters getSmoothTexture()
    {
        CloudController::Parameters params;
        params.grainParams.duration = 32768.0f; // Longer grains
        params.grainParams.density = 128.0f;    // More overlap
        params.modParams.rate = 0.02f;          // Slower modulation
        params.modParams.pitchDepth = 0.002f;   // Less pitch variation
        return params;
    }

    static CloudController::Parameters getGranularTexture()
    {
        CloudController::Parameters params;
        params.grainParams.duration = 8192.0f; // Shorter grains
        params.grainParams.density = 1024.0f;  // Less overlap
        params.modParams.rate = 0.08f;         // Faster modulation
        params.modParams.pitchDepth = 0.01f;   // More pitch variation
        return params;
    }

    BufferManager circularBuffer;
    juce::AudioBuffer<float> frozenBuffer;

    CloudController cloudController;

    double sampleRate_ = 0.0;
    int bufferSize_ = 0;

    std::atomic<bool> isFrozen { false };
    std::atomic<bool> needsSnapshot { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranularFreeze)
};

#pragma once
#include "BufferManager.h"
#include "Constants.h"
#include "ui/components/FreezeParametersPopup.h"
#include <JuceHeader.h>
#include <array>

class WindowTable
{
public:
    WindowTable() { createWindow(); }

    float operator[] (int index) const { return table[(size_t) index]; }

    size_t size() const { return ENVELOPE_WINDOW_SIZE; }

private:
    void createWindow()
    {
        for (int i = 0; i < ENVELOPE_WINDOW_SIZE; ++i)
        {
            const float x = static_cast<float> (i) * (1.0f / (static_cast<float> (ENVELOPE_WINDOW_SIZE) - 1.0f));
            table[(size_t) i] = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * x));
        }
    }

    float table[ENVELOPE_WINDOW_SIZE];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WindowTable)
};

class Modulator
{
public:
    struct Parameters
    {
        float rate = MOD_RATE;
        float pitchDepth = PITCH_MOD_DEPTH;
        float ampDepth = AMP_MOD_DEPTH;
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
        const int modIdx = static_cast<int> (modPhase * MOD_TABLE_SIZE) & MOD_TABLE_MASK;
        const int modOffset = (modIdx + (grainIndex * MOD_TABLE_SIZE / numGrains)) & MOD_TABLE_MASK;

        return { pitchModTable[(size_t) modOffset], ampModTable[(size_t) modOffset] };
    }

private:
    void createModulation()
    {
        pitchModTable.resize (MOD_TABLE_SIZE);
        ampModTable.resize (MOD_TABLE_SIZE);

        updateModulationTables (PITCH_MOD_DEPTH, AMP_MOD_DEPTH);
    }

    void updateModulationTables (float pitchDepth, float ampDepth)
    {
        for (int i = 0; i < MOD_TABLE_SIZE; ++i)
        {
            const float phase = static_cast<float> (i) / static_cast<float> (MOD_TABLE_SIZE);
            const float modValue = std::sin (juce::MathConstants<float>::twoPi * phase);

            pitchModTable[(size_t) i] = 1.0f + (modValue * pitchDepth);
            ampModTable[(size_t) i] = juce::jlimit (MIN_AMP_MOD, MAX_AMP_MOD, 1.0f + (modValue * ampDepth));
        }
    }

    std::vector<float> pitchModTable;
    std::vector<float> ampModTable;
    float modPhase = 0.0f;
    float modPhaseInc = 0.0f;
    float modRate = MOD_RATE;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Modulator)
};

class Grain
{
public:
    struct Parameters
    {
        float duration = GRAIN_LENGTH;
        float density = GRAIN_SPACING;
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

    SamplePair processSingle (const juce::AudioBuffer<float>& frozenBuffer, const WindowTable& window, float pitchMod, float ampMod)
    {
        if (! isActive) return { 0.0f, 0.0f };

        // Clamp envelope index to valid range
        const int envIdx = std::min (static_cast<int> (envPosition * (float) window.size()), (int) window.size() - 1);
        const float env = window[envIdx];

        const int pos1 = static_cast<int> (position);
        const int pos2 = (pos1 + 1) % frozenBuffer.getNumSamples();
        const float frac = position - static_cast<float> (pos1);

        const float* frozenL = frozenBuffer.getReadPointer (LEFT_CHANNEL);
        const float* frozenR = frozenBuffer.getReadPointer (RIGHT_CHANNEL);

        const float sampleL = frozenL[pos1] + frac * (frozenL[pos2] - frozenL[pos1]);
        const float sampleR = frozenR[pos1] + frac * (frozenR[pos2] - frozenR[pos1]);

        const float amp = env * ampMod;

        position += increment * pitchMod;
        if (position >= frozenBuffer.getNumSamples())
            position = std::fmod (position, (float) frozenBuffer.getNumSamples());
        else if (position < 0.0f)
            position += frozenBuffer.getNumSamples();

        envPosition += envIncrement;
        isActive = envPosition < 1.0f;

        return { sampleL * amp, sampleR * amp };
    }

    void
        processBlock (const juce::AudioBuffer<float>& frozenBuffer, const WindowTable& window, float pitchMod, float ampMod, int numSamples)
    {
        leftOut.resize ((size_t) numSamples);
        rightOut.resize ((size_t) numSamples);

        for (int s = 0; s < numSamples; ++s)
        {
            auto [left, right] = processSingle (frozenBuffer, window, pitchMod, ampMod);
            leftOut[(size_t) s] = left;
            rightOut[(size_t) s] = right;
        }
    }

    const std::vector<float>& getLeftOut() const { return leftOut; }
    const std::vector<float>& getRightOut() const { return rightOut; }

    bool isPlaying() const { return isActive; }

    float position = 0.0f;
    float envPosition = 0.0f;

private:
    float increment = 1.0f;
    float envIncrement = 0.0f;
    bool isActive = false;
    std::vector<float> leftOut;
    std::vector<float> rightOut;
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
            int maxGrains = MAX_GRAINS;
            float positionSpread = 1.0f;
            float amplitude = DEFAULT_FREEZE_AMPLITUDE;
        };

        void setParameters (const Parameters& params)
        {
            cloudParams = params;
            modulator.setParameters (params.modParams);
            nextGrainTime = static_cast<int> (params.grainParams.density);
        }

        void setGranularParameters (const FreezeParameters& params)
        {
            cloudParams.grainParams.duration = params.grainLengthMs * 0.001f * sampleRate_;
            cloudParams.grainParams.density = params.grainSpacing;
            cloudParams.maxGrains = params.maxGrains;
            cloudParams.positionSpread = params.positionSpread;
            cloudParams.modParams.rate = params.modRate;
            cloudParams.modParams.pitchDepth = params.pitchModDepth;
            cloudParams.modParams.ampDepth = params.ampModDepth;
            // Store randomness for use in grain triggering
            grainDurationRandomFactor = params.grainRandomness;

            modulator.setParameters (cloudParams.modParams);
        }

        void setLevelParameters (float amplitude) { cloudParams.amplitude = amplitude; }
        float getLevelParameters() const { return cloudParams.amplitude; }

        void prepare (double sampleRate, float bufferDuration)
        {
            sampleRate_ = sampleRate;
            clearAllGrains();
            bufferSizeFloat_ = (float) sampleRate_ * bufferDuration;
            modulator.prepare (sampleRate);
        }

        std::array<Grain, MAX_GRAINS> getActiveGrains() const { return grains; }

        void processBlock (juce::AudioBuffer<float>& output,
                           const juce::AudioBuffer<float>& frozenBuf,
                           const juce::AudioBuffer<float>& tailBuf)
        {
            const int numSamples = output.getNumSamples();

            for (int i = 0; i < numSamples; ++i)
            {
                modulator.updatePhase();
                if (--nextGrainTime <= 0)
                {
                    startNewGrain();
                    nextGrainTime = static_cast<int> (cloudParams.grainParams.density);
                }
            }
            if (isTailing())
            {
                tailElapsedSamples += numSamples;
                if (tailElapsedSamples >= tailDurationSamples)
                {
                    cloudState = CloudState::IDLE;
                    clearAllGrains();
                }
            }

            // ===== GRAIN PROCESSING =====
            std::vector<float> blockLeftAccum ((size_t) numSamples, 0.0f);
            std::vector<float> blockRightAccum ((size_t) numSamples, 0.0f);
            int activeCount = 0;

            const int numGrains = cloudParams.maxGrains;

            float tailBufferBlend = 0.0f;
            if (isTailing())
            {
                tailBufferBlend = 1.0f - (static_cast<float> (tailElapsedSamples) / static_cast<float> (tailDurationSamples));
                tailBufferBlend = juce::jlimit (0.0f, 1.0f, tailBufferBlend);
            }

            for (int g = 0; g < numGrains; ++g)
            {
                if (grains[(size_t) g].isPlaying())
                {
                    auto [pitchMod, ampMod] = modulator.getModulation (g, numGrains);

                    // Pick buffer based on blend probability
                    const juce::AudioBuffer<float>& bufferToUse = (random.nextFloat() < tailBufferBlend) ? tailBuf : frozenBuf;

                    grains[(size_t) g].processBlock (bufferToUse, window, pitchMod, ampMod, numSamples);

                    juce::FloatVectorOperations::add (blockLeftAccum.data(), grains[(size_t) g].getLeftOut().data(), numSamples);
                    juce::FloatVectorOperations::add (blockRightAccum.data(), grains[(size_t) g].getRightOut().data(), numSamples);
                    activeCount++;
                }
            }

            // ===== TAIL FADE-OUT =====
            float tailGain = 1.0f;
            if (cloudState == CloudState::TAILING)
            {
                tailGain = 1.0f - (static_cast<float> (tailElapsedSamples) / static_cast<float> (tailDurationSamples));
                tailGain = juce::jlimit (0.0f, 1.0f, tailGain);
            }

            // ===== OUTPUT MIXING & NORMALIZATION =====
            if (activeCount > 0)
            {
                const float scale = cloudParams.amplitude * (activeCount <= 1 ? 1.0f : fastInverseSqrt (static_cast<float> (activeCount)))
                                    * tailGain;

                juce::FloatVectorOperations::multiply (blockLeftAccum.data(), scale, numSamples);
                juce::FloatVectorOperations::multiply (blockRightAccum.data(), scale, numSamples);

                juce::FloatVectorOperations::add (output.getWritePointer (LEFT_CHANNEL), blockLeftAccum.data(), numSamples);
                juce::FloatVectorOperations::add (output.getWritePointer (RIGHT_CHANNEL), blockRightAccum.data(), numSamples);
            }
        }

        void triggerFreeze()
        {
            cloudState = CloudState::FREEZING;
            nextGrainTime = 0;
        }

        void stopFreeze()
        {
            if (cloudState == CloudState::FREEZING)
            {
                cloudState = CloudState::TAILING;
                tailElapsedSamples = 0;
                tailDurationSamples = (int) (sampleRate_ * 5.0); // 5 second tail
            }
        }

        bool isIdle() const { return cloudState == CloudState::IDLE; }
        bool isFreezing() const { return cloudState == CloudState::FREEZING; }
        bool isTailing() const { return cloudState == CloudState::TAILING; }

    private:
        void startNewGrain()
        {
            for (auto& grain : grains)
            {
                if (! grain.isPlaying())
                {
                    float startPosition = random.nextFloat() * (bufferSizeFloat_ * cloudParams.positionSpread);

                    auto params = cloudParams.grainParams;
                    params.duration *= (1.0f - grainDurationRandomFactor) + random.nextFloat() * 2.0f * grainDurationRandomFactor;

                    grain.trigger (startPosition, params);
                    return;
                }
            }
        }

        void clearAllGrains() { std::memset ((void*) grains.data(), 0, sizeof (Grain) * MAX_GRAINS); }

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

        double sampleRate_;
        int nextGrainTime = 0;
        float bufferSizeFloat_ = 0.0f;
        juce::Random random;
        Parameters cloudParams;
        float grainDurationRandomFactor;

        enum class CloudState
        {
            IDLE,
            FREEZING,
            TAILING
        };
        CloudState cloudState = CloudState::IDLE;
        int tailElapsedSamples = 0;
        int tailDurationSamples = 0;
    };

    GranularFreeze() {}
    ~GranularFreeze() {}

    double getSampleRate() const { return sampleRate_; }

    std::array<Grain, MAX_GRAINS> getActiveGrains() const { return cloudController.getActiveGrains(); }

    const juce::AudioBuffer<float>& getFrozenBuffer() const { return frozenBuffer; }
    int getBufferSize() const { return bufferSize_; }

    void prepareToPlay (double sampleRate, int numChannels)
    {
        const int bufferSize = static_cast<int> (sampleRate * FREEZE_BUFFER_DURATION_SECONDS);

        frozenBuffer.setSize (numChannels, bufferSize);
        frozenBuffer.clear();
        tailBuffer.setSize (numChannels, bufferSize);
        tailBuffer.clear();

        circularBuffer.prepareToPlay (numChannels, bufferSize);

        sampleRate_ = sampleRate;
        bufferSize_ = bufferSize;

        cloudController.prepare (sampleRate, FREEZE_BUFFER_DURATION_SECONDS);
    }

    void toggleActiveState()
    {
        if (cloudController.isIdle() || cloudController.isTailing())
        {
            for (int ch = 0; ch < circularBuffer.getNumChannels(); ++ch)
            {
                juce::FloatVectorOperations::copy (frozenBuffer.getWritePointer (ch),
                                                   circularBuffer.getReadPointer (ch),
                                                   circularBuffer.getNumSamples());
            }
            cloudController.triggerFreeze();
        }
        else if (cloudController.isFreezing())
        {
            cloudController.stopFreeze();

            for (int ch = 0; ch < circularBuffer.getNumChannels(); ++ch)
            {
                juce::FloatVectorOperations::copy (tailBuffer.getWritePointer (ch),
                                                   frozenBuffer.getReadPointer (ch),
                                                   tailBuffer.getNumSamples());
            }
        }
    }

    void releaseResources()
    {
        circularBuffer.releaseResources();
        frozenBuffer.setSize (0, 0);
        tailBuffer.setSize (0, 0);
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        const auto numSamples = buffer.getNumSamples();

        circularBuffer.writeToAudioBuffer ([] (float* dest, const float* src, int numCurrentSamples, bool)
                                           { juce::FloatVectorOperations::copy (dest, src, numCurrentSamples); },
                                           buffer,
                                           numSamples,
                                           true,
                                           false);

        if (! cloudController.isIdle())
        {
            cloudController.processBlock (buffer, frozenBuffer, tailBuffer);
        }
    }

    bool isEnabled() const { return cloudController.isFreezing(); }

    void setLevel (float amplitude) { cloudController.setLevelParameters (amplitude); }
    float getLevel() const { return cloudController.getLevelParameters(); }

private:
    static CloudController::Parameters getDefaultParameters()
    {
        CloudController::Parameters params;
        return params;
    }

    BufferManager circularBuffer;
    juce::AudioBuffer<float> frozenBuffer;
    juce::AudioBuffer<float> tailBuffer;

    CloudController cloudController;

    double sampleRate_ = 0.0;
    int bufferSize_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranularFreeze)
};

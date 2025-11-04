#pragma once

#include "engine/BufferManager.h"
#include "engine/Constants.h"
#include "engine/LoopFifo.h"
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

// Grain is responsible for its complete behavior:
// - State management (position, envelope, lifecycle)
// - Audio generation (read from frozen buffer, apply envelope)
// - Output mixing (add to grain buffer)
class Grain
{
public:
    Grain() {}
    void reset()
    {
        isActive = false;
        grainLength = 0;
        grainPosition = 0;
        pitch = 1.0;
        amplitude = 1.0f;
        fifo.clear();
        frozenBufferLength = 0;
        cachedSampleRate = 0.0;
    }

    void start (int frozenBufferSize, double sampleRate)
    {
        reset();
        frozenBufferLength = frozenBufferSize;
        cachedSampleRate = sampleRate;

        fifo.prepareToPlay (frozenBufferSize);
        fifo.setMusicalLength (frozenBufferSize);

        // DON'T randomize phase here - grainLength is still 0!
        spawn(); // This will set grainLength and randomize phase
    }

    // Generate and mix one sample from this grain into the output buffer
    void generateAndMixSample (const juce::AudioBuffer<float>& frozenBuffer, juce::AudioBuffer<float>& outputBuffer, int sampleIndex)
    {
        if (! isActive) return;

        // Check if grain finished and needs to respawn
        if (isGrainFinished())
        {
            spawn();
        }

        // Calculate envelope (Hann window)
        const float envelope = calculateEnvelope();

        // Read interpolated sample from frozen buffer
        const double fractionalPos = fifo.getExactReadPos();

        // Mix into output buffer for all channels
        const int numChannels = juce::jmin (frozenBuffer.getNumChannels(), outputBuffer.getNumChannels());
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float sample = interpolateSample (frozenBuffer, ch, fractionalPos);
            outputBuffer.addSample (ch, sampleIndex, sample * envelope);
        }

        // Advance grain position
        fifo.finishedRead (1, pitch, false);
        grainPosition++;
    }

    bool getIsActive() const { return isActive; }

private:
    bool isActive = false;
    LoopFifo fifo; // Manages read position in frozen buffer
    int grainLength = 0;
    int grainPosition = 0;

    float pitch = 1.0f;
    float amplitude = 1.0f;
    int frozenBufferLength = 0;
    double cachedSampleRate = 0.0;

    static constexpr float GRAIN_DENSITY = 0.8f;
    static constexpr float GRAIN_DURATION_SECONDS = 0.1f;
    static constexpr float GRAIN_PITCH_SHIFT = 1.0f; // No pitch shift by default
    static constexpr float GRAIN_SPREAD = 0.3f;

    juce::Random random;

    void spawn()
    {
        isActive = true; // Always spawn when called
        grainPosition = 0;

        // Randomize start position and set in LoopFifo
        const int startPos = randomizeStartPosition();
        fifo.setReadPosition (startPos);

        // Set grain length based on duration
        grainLength = static_cast<int> (GRAIN_DURATION_SECONDS * cachedSampleRate);
        grainLength = juce::jlimit (static_cast<int> (cachedSampleRate * 0.01), static_cast<int> (cachedSampleRate * 0.5), grainLength);

        // Add variations for interest
        modulatePitch();
        modulateAmplitude();

        // NOW randomize phase - grainLength is set so this works correctly
        randomizePhase();
    }

    float calculateEnvelope() const
    {
        // Hann window envelope
        return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * grainPosition / static_cast<float> (grainLength)));
    }

    float interpolateSample (const juce::AudioBuffer<float>& buffer, int channel, double fractionalPos) const
    {
        const int pos1 = static_cast<int> (fractionalPos) % frozenBufferLength;
        const int pos2 = (pos1 + 1) % frozenBufferLength;
        const float frac = static_cast<float> (fractionalPos - std::floor (fractionalPos));

        const float sample1 = buffer.getSample (channel, pos1);
        const float sample2 = buffer.getSample (channel, pos2);
        return sample1 + frac * (sample2 - sample1);
    }

    int randomizeStartPosition()
    {
        const float spreadAmount = GRAIN_SPREAD * frozenBufferLength;
        const float centerPos = frozenBufferLength * 0.5f;
        float position = centerPos + (random.nextFloat() - 0.5f) * spreadAmount;

        // Clamp to valid range (LoopFifo will handle wrapping during playback)
        return juce::jlimit (0, frozenBufferLength - 1, static_cast<int> (position));
    }

    void modulatePitch()
    {
        const float pitchVariation = 0.1f * GRAIN_SPREAD;
        pitch = GRAIN_PITCH_SHIFT * (1.0 + (random.nextFloat() - 0.5f) * pitchVariation);
    }

    void modulateAmplitude()
    {
        const float amplitudeVariation = 0.2f;
        amplitude = 0.9f + random.nextFloat() * amplitudeVariation; // 0.9 to 1.1
    }

    void randomizePhase() { grainPosition = random.nextInt (grainLength > 0 ? grainLength : 1000); }

    bool isGrainFinished() const { return grainPosition >= grainLength; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Grain)
};

// GranularFreeze manages the circular buffer and coordinates grains
class GranularFreeze
{
public:
    GranularFreeze() = default;
    void prepareToPlay (double currentSampleRate, int samplesPerBlock, int numChannels)
    {
        PERFETTO_FUNCTION();
        sampleRate = currentSampleRate;

        const int captureBufferSize = static_cast<int> (sampleRate * FREEZE_BUFFER_DURATION_SECONDS);
        circularBuffer.prepareToPlay (numChannels, captureBufferSize);

        frozenBuffer.setSize (numChannels, captureBufferSize);
        frozenBuffer.clear();

        grainBuffer.setSize (numChannels, samplesPerBlock);
        grainBuffer.clear();

        for (auto& grain : grains)
        {
            grain.reset();
        }
    }

    void toggleActiveState()
    {
        PERFETTO_FUNCTION();
        if (! isFreezeActive.load())
        {
            isFreezeActive.store (true, std::memory_order_release);

            takeSnapshot();

            // Start all grains
            const int frozenSize = frozenBuffer.getNumSamples();
            for (auto& grain : grains)
                grain.start (frozenSize, sampleRate);
        }
        else
        {
            isFreezeActive.store (false, std::memory_order_release);
            fadeOutGrains();
        }
    }

    void releaseResources()
    {
        PERFETTO_FUNCTION();
        circularBuffer.releaseResources();
        frozenBuffer.setSize (0, 0);
        grainBuffer.setSize (0, 0);
        isFreezeActive.store (false, std::memory_order_relaxed);

        for (auto& grain : grains)
        {
            grain.reset();
        }
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        PERFETTO_FUNCTION();
        const int numSamples = buffer.getNumSamples();

        // ALWAYS write input to circular buffer (continuous recording)
        circularBuffer.writeToAudioBuffer ([] (float* destination, const float* source, const int numSamples, const bool shouldOverdub)
                                           { juce::FloatVectorOperations::copy (destination, source, numSamples); },
                                           buffer,
                                           numSamples,
                                           true, // isOverdub = true (so it wraps around)
                                           false // syncWriteWithRead = false (independent of playback)
        );

        if (isFreezeActive.load (std::memory_order_acquire))
        {
            generateDrone (numSamples);
            mixDroneIntoBuffer (buffer, numSamples);
        }

        // Handle fade out
        if (isFadingOut)
        {
            updateFadeOut();
        }
    }

    bool isEnabled() const { return isFreezeActive.load (std::memory_order_acquire); }

private:
    BufferManager circularBuffer;

    juce::AudioBuffer<float> frozenBuffer; // Snapshot of circular buffer when freeze activated
    juce::AudioBuffer<float> grainBuffer;  // Working buffer for grain generation

    float droneGain = 0.5f;

    double sampleRate;
    bool isFadingOut = false;
    int fadeOutSamples = 0;
    int fadeOutRemaining = 0;

    std::atomic<bool> isFreezeActive { false };
    std::array<Grain, NUM_GRAINS_FOR_FREEZE> grains;

    // Note: For smooth pad-like drones, you need:
    // - NUM_GRAINS_FOR_FREEZE >= 16 (more grains = more overlap)
    // - GRAIN_DURATION_SECONDS >= 0.2 (longer grains = smoother)
    // If it sounds "ghosty" or has tremolo, increase these values

    void takeSnapshot()
    {
        const int bufferSize = circularBuffer.getNumSamples();

        circularBuffer.readFromAudioBuffer ([] (float* destination, const float* source, const int numSamples)
                                            { juce::FloatVectorOperations::copy (destination, source, numSamples); },
                                            frozenBuffer,
                                            bufferSize,
                                            1.0f, // speedMultiplier = 1.0 (normal speed)
                                            false // isOverdub = false
        );

        // Debug: Check if we captured any audio
        float peakLevel = frozenBuffer.getMagnitude (0, 0, bufferSize);
        DBG ("Freeze snapshot taken - Buffer size: " << bufferSize << ", Peak level: " << peakLevel);

        if (peakLevel < 0.0001f)
        {
            DBG ("WARNING: Frozen buffer appears to be silent!");
        }
    }

    void generateDrone (int numSamples)
    {
        grainBuffer.clear();

        // Debug: Count active grains and check their states
        int activeGrainCount = 0;
        int finishedGrainsThisBlock = 0;

        for (auto& grain : grains)
            if (grain.getIsActive()) activeGrainCount++;

        // Beautiful symmetry - each grain handles itself completely
        for (int i = 0; i < numSamples; ++i)
        {
            for (auto& grain : grains)
            {
                grain.generateAndMixSample (frozenBuffer, grainBuffer, i);
            }
        }

        // Debug: Check output and grain states
        float peakLevel = grainBuffer.getMagnitude (0, 0, numSamples);
        float rmsLevel = grainBuffer.getRMSLevel (0, 0, numSamples);

        // Only log occasionally to avoid spam
        static int logCounter = 0;
        if (++logCounter % 100 == 0)
        {
            DBG ("Active grains: " << activeGrainCount << ", Peak: " << peakLevel << ", RMS: " << rmsLevel
                                   << ", Grain duration: " << (0.25f * sampleRate) << " samples");
        }
    }

    void mixDroneIntoBuffer (juce::AudioBuffer<float>& buffer, int numSamples)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.addFromWithRamp (ch, 0, grainBuffer.getReadPointer (ch), numSamples, droneGain, droneGain);
        }
    }

    void fadeOutGrains()
    {
        isFadingOut = true;
        fadeOutSamples = static_cast<int> (sampleRate * 0.5); // 500ms fade out
        fadeOutRemaining = fadeOutSamples;
    }

    void updateFadeOut()
    {
        if (fadeOutRemaining > 0)
        {
            fadeOutRemaining--;
            // FIX: Calculate fade multiplier correctly
            const float fadeMultiplier = fadeOutRemaining / static_cast<float> (fadeOutSamples);
            droneGain = 0.5f * fadeMultiplier; // Start at 0.5, fade to 0
        }
        else
        {
            isFadingOut = false;
            droneGain = 0.5f; // Reset for next activation

            // Deactivate all grains
            for (auto& grain : grains)
                grain.reset();
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranularFreeze)
};

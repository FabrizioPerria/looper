#pragma once
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

class VolumeProcessor
{
public:
    VolumeProcessor() {}

    void prepareToPlay (const double currentSampleRate, const int /*blockSize*/)
    {
        clear();
        sampleRate = currentSampleRate;
        setCrossFadeLength ((int) (0.01 * sampleRate)); // default 10 ms crossfade
    }

    void releaseResources() { clear(); }

    void clear()
    {
        trackVolume = previousTrackVolume = 1.0f;
        soloed = muted = false;
    }

    float getTrackVolume() const { return trackVolume; }
    void setTrackVolume (const float newVolume) { trackVolume = std::clamp (newVolume, 0.0f, 1.0f); }

    bool isSoloed() const { return soloed; }
    void setSoloed (const bool shouldBeSoloed) { soloed = shouldBeSoloed; }

    bool isMuted() const { return muted; }
    void setMuted (const bool shouldBeMuted)
    {
        PERFETTO_FUNCTION();
        static float volumeBeforeMute = 1.0f;
        if (shouldBeMuted)
        {
            if (trackVolume > 0.001f && ! muted) volumeBeforeMute = trackVolume;
            trackVolume = 0.0f;
            muted = true;
        }
        else
        {
            trackVolume = volumeBeforeMute;
            muted = false;
        }
    }

    void applyVolume (juce::AudioBuffer<float>& output, const int numSamples)
    {
        PERFETTO_FUNCTION();
        if (std::abs (trackVolume - previousTrackVolume) > 0.001f)
        {
            output.applyGainRamp (0, (int) numSamples, previousTrackVolume, trackVolume);
            previousTrackVolume = trackVolume;
        }
        else
        {
            output.applyGain (trackVolume);
        }
    }

    void setOverdubNewGain (const double newGain) { overdubNewGain = std::clamp (newGain, 0.0, 2.0); }

    void setOverdubOldGain (const double newGain) { overdubOldGain = std::clamp (newGain, 0.0, 2.0); }

    void toggleOutputNormalization() { shouldNormalizeOutput = ! shouldNormalizeOutput; }

    double getOverdubNewGain() const { return overdubNewGain; }
    double getOverdubOldGain() const { return overdubOldGain; }

    void normalizeOutput (juce::AudioBuffer<float>& audioBuffer, const int length)
    {
        PERFETTO_FUNCTION();
        if (shouldNormalizeOutput)
        {
            float maxSample = 0.0f;
            for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
                maxSample = std::max (maxSample, audioBuffer.getMagnitude (ch, 0, length));

            if (maxSample > 0.001f) // If not silent
                audioBuffer.applyGain (0, length, 0.9f / maxSample);
        }
    }

    void applyCrossfade (juce::AudioBuffer<float>& audioBuffer, const int length)
    {
        PERFETTO_FUNCTION();
        const int fadeSamples = std::min (crossFadeLength, length / 4);
        if (fadeSamples > 0)
        {
            audioBuffer.applyGainRamp (0, fadeSamples, 0.0f, 1.0f);                    // fade in
            audioBuffer.applyGainRamp (length - fadeSamples, fadeSamples, 1.0f, 0.0f); // fade out
        }
    }

    void setCrossFadeLength (const int newLength) { crossFadeLength = newLength; }

    void saveBalancedLayers (float* dest, const float* source, int numSamples, bool shouldOverdub)
    {
        PERFETTO_FUNCTION();
        juce::FloatVectorOperations::multiply (dest, shouldOverdub ? (float) overdubOldGain : 0, (int) numSamples);
        juce::FloatVectorOperations::addWithMultiply (dest, source, (float) overdubNewGain, (int) numSamples);
    }

    bool isNormalizingOutput() const { return shouldNormalizeOutput; }

private:
    float trackVolume = 1.0f;
    double overdubNewGain = 1.0;
    double overdubOldGain = 1.0;

    bool shouldNormalizeOutput = true;

    float previousTrackVolume = 1.0f;
    bool soloed = false;
    bool muted = false;

    int crossFadeLength = 0;

    double sampleRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VolumeProcessor)
};

#pragma once
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

class VolumeProcessor
{
public:
    VolumeProcessor() {}

    void prepareToPlay (const double /*sampleRate*/, const int /*blockSize*/) { clear(); }

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

    void setOverdubGains (const double oldGain, const double newGain)
    {
        PERFETTO_FUNCTION();
        overdubNewGain = std::clamp (newGain, 0.0, 2.0);
        overdubOldGain = std::clamp (oldGain, 0.0, 2.0);
        shouldNormalizeOutput = false;
    }

    void enableOutputNormalization()
    {
        PERFETTO_FUNCTION();
        overdubNewGain = 1.0f;
        overdubOldGain = 1.0f;
        shouldNormalizeOutput = true;
    }

    double getOverdubNewGain() const { return overdubNewGain; }
    double getOverdubOldGain() const { return overdubOldGain; }

private:
    float trackVolume = 1.0f;
    double overdubNewGain = 1.0;
    double overdubOldGain = 1.0;

    bool shouldNormalizeOutput = true;

    float previousTrackVolume = 1.0f;
    bool soloed = false;
    bool muted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VolumeProcessor)
};

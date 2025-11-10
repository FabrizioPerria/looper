#pragma once

#include "engine/Constants.h"
#include <JuceHeader.h>

class Metronome
{
public:
    Metronome() { bpm = METRONOME_DEFAULT_BPM; }

    ~Metronome() = default;

    bool isEnabled() const { return enabled; }

    void setBpm (int newBpm)
    {
        bpm = std::clamp (newBpm, (int) METRONOME_MIN_BPM, (int) METRONOME_MAX_BPM);
        samplesPerBeat = calculateSamplesPerBeat();
    }
    int getBpm() const { return bpm; }

    bool isTapTempoActive() const { return tapTempoActive; }
    bool wasLastTapRecent() const { return (juce::Time::getMillisecondCounter() - lastTapTime) < METRONOME_TAP_RECENT_THRESHOLD_MS; }

    void setTimeSignature (int numerator, int denominator)
    {
        timeSignature.numerator = numerator;
        timeSignature.denominator = denominator;
        samplesPerBeat = calculateSamplesPerBeat();

        if (strongBeatIndex >= timeSignature.numerator) strongBeatIndex = -1;
    }

    void setStrongBeat (int beatIndex, bool isStrong)
    {
        if (isStrong)
            strongBeatIndex = juce::jlimit (0, timeSignature.numerator - 1, beatIndex - 1);
        else
            strongBeatIndex = -1;
    }

    void disableStrongBeat() { strongBeatIndex = -1; }

    void prepareToPlay (double currentSampleRate, int /**/)
    {
        sampleRate = currentSampleRate;
        samplesPerBeat = calculateSamplesPerBeat();
        samplesSinceLastBeat = 0;
        currentBeat = 0;

        generateClickSounds();
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        if (! enabled.load (std::memory_order_relaxed)) return;

        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        float vol = volume.load (std::memory_order_relaxed);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Check if we've hit a beat at this exact sample
            if (samplesSinceLastBeat >= samplesPerBeat && samplesPerBeat > 0)
            {
                samplesSinceLastBeat -= samplesPerBeat;
                currentBeat = (currentBeat + 1) % timeSignature.numerator;

                bool isStrongBeat = (strongBeatIndex >= 0 && currentBeat == strongBeatIndex);
                currentClickBuffer = isStrongBeat ? &strongClickBuffer : &weakClickBuffer;
                currentClickPosition = 0;
            }

            // Render one sample of the click sound if active
            if (currentClickBuffer && currentClickPosition < currentClickBuffer->getNumSamples())
            {
                float clickSample = currentClickBuffer->getSample (0, currentClickPosition) * vol;

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    buffer.addSample (ch, sample, clickSample);
                }

                currentClickPosition++;
            }

            samplesSinceLastBeat++;
        }
    }

    void setEnabled (bool shouldBeEnabled)
    {
        if (! shouldBeEnabled)
        {
            currentClickPosition = 0;
            currentClickBuffer = nullptr;
        }
        enabled = shouldBeEnabled;
    }

    void reset()
    {
        samplesSinceLastBeat = 0;
        currentBeat = 0;
    }

    void releaseResources()
    {
        strongClickBuffer.setSize (0, 0);
        weakClickBuffer.setSize (0, 0);
    }

    int getCurrentBeat() const { return currentBeat; }

    void setVolume (float newVolume) { volume = newVolume; }
    float getVolume() const { return volume; }

    bool isStrongBeat() const { return (strongBeatIndex >= 0 && currentBeat == strongBeatIndex); }
    // std::function<void (bool isStrongBeat)> onBeatCallback;

    void handleTap()
    {
        auto now = juce::Time::getMillisecondCounter();

        // Check if this tap is too close to previous (debounce)
        if (! tapTimes.empty() && (now - lastTapTime) < METRONOME_MIN_TAP_INTERVAL_MS) return;

        lastTapTime = now;
        tapTempoActive = true;

        // Add this tap time
        tapTimes.push_back (now);

        // Remove taps older than timeout
        tapTimes.erase (std::remove_if (tapTimes.begin(),
                                        tapTimes.end(),
                                        [now] (juce::uint32 tapTime) { return (now - tapTime) > METRONOME_TAP_TIMEOUT_MS; }),
                        tapTimes.end());

        // Need at least 2 taps to calculate BPM
        if (tapTimes.size() < 2)
        {
            return;
        }

        // Calculate average interval between taps
        double totalInterval = 0.0;
        int numIntervals = 0;

        for (size_t i = 1; i < tapTimes.size(); ++i)
        {
            double interval = tapTimes[i] - tapTimes[i - 1];
            totalInterval += interval;
            numIntervals++;
        }

        double averageInterval = totalInterval / numIntervals;

        // Convert interval (milliseconds) to BPM
        // BPM = 60000 / interval_ms
        int newBPM = juce::roundToInt (60000.0 / averageInterval);

        newBPM = juce::jlimit ((int) METRONOME_MIN_BPM, (int) METRONOME_MAX_BPM, newBPM);
        setBpm (newBPM);
    }

private:
    void generateClickSounds()
    {
        // Generate strong click (higher pitched, longer)
        int strongClickLength = (int) (sampleRate * METRONOME_STRONG_BEAT_CLICK_LENTH_SECONDS); // 10ms
        strongClickBuffer.setSize (1, strongClickLength);

        for (int i = 0; i < strongClickLength; ++i)
        {
            float t = (float) i / (float) sampleRate;
            float envelope = std::exp (-t * METRONOME_STRONG_BEAT_ENVELOPE_DECAY); // Decay
            float frequency = METRONOME_STRONG_BEAT_FREQUENCY;                     // Higher pitch for strong beat
            float sample = std::sin (2.0f * juce::MathConstants<float>::pi * frequency * t) * envelope;
            strongClickBuffer.setSample (0, i, sample * METRONOME_STRONG_BEAT_GAIN); // Louder
        }

        // Generate weak click (lower pitched, shorter)
        int weakClickLength = (int) (sampleRate * METRONOME_WEAK_BEAT_CLICK_LENTH_SECONDS); // 8ms
        weakClickBuffer.setSize (1, weakClickLength);

        for (int i = 0; i < weakClickLength; ++i)
        {
            float t = (float) i / (float) sampleRate;
            float envelope = std::exp (-t * METRONOME_WEAK_BEAT_ENVELOPE_DECAY); // Decay
            float frequency = METRONOME_WEAK_BEAT_FREQUENCY;                     // Lower pitch for weak beat
            float sample = std::sin (2.0f * juce::MathConstants<float>::pi * frequency * t) * envelope;
            weakClickBuffer.setSample (0, i, sample * METRONOME_WEAK_BEAT_GAIN);
        }
    }

    int calculateSamplesPerBeat() const
    {
        double beatDuration = (60.0 / bpm) * (4.0 / timeSignature.denominator);
        return static_cast<int> (beatDuration * sampleRate);
    }

    double sampleRate;
    int samplesPerBeat;
    int currentBeat;
    int strongBeatIndex;
    int samplesSinceLastBeat;
    int currentClickPosition;
    std::atomic<bool> enabled { METRONOME_DEFAULT_ENABLED };
    std::atomic<float> volume { METRONOME_DEFAULT_VOLUME };

    struct TimeSignature
    {
        int numerator;
        int denominator;
    } timeSignature { METRONOME_DEFAULT_TIME_SIGNATURE_NUMERATOR, METRONOME_DEFAULT_TIME_SIGNATURE_DENOMINATOR };

    int bpm;
    juce::AudioBuffer<float> strongClickBuffer;
    juce::AudioBuffer<float> weakClickBuffer;
    juce::AudioBuffer<float>* currentClickBuffer;

    // Tap tempo state
    std::vector<juce::uint32> tapTimes;
    juce::uint32 lastTapTime = 0;
    bool tapTempoActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Metronome)
};

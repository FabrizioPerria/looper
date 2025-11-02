#pragma once

#include <JuceHeader.h>

class Metronome
{
public:
    Metronome() { bpm = 120; }

    ~Metronome() = default;

    // void setEnabled (bool shouldBeEnabled) { enabled = shouldBeEnabled; }
    bool isEnabled() const { return enabled; }

    void setBpm (int newBpm)
    {
        bpm = newBpm;
        samplesPerBeat = calculateSamplesPerBeat();
    }

    void setTimeSignature (int numerator, int denominator)
    {
        timeSignature.numerator = numerator;
        timeSignature.denominator = denominator;
        samplesPerBeat = calculateSamplesPerBeat(); // ✅ Recalculate

        // ✅ Clamp strongBeatIndex if it's now out of range
        if (strongBeatIndex >= timeSignature.numerator) strongBeatIndex = -1;
    }

    void setStrongBeat (int beatIndex, bool isStrong)
    {
        if (isStrong)
            strongBeatIndex = juce::jlimit (0, timeSignature.numerator - 1, beatIndex - 1); // ✅ Convert 1-based to 0-based
        else
            strongBeatIndex = -1;
    }

    void disableStrongBeat() { strongBeatIndex = -1; }

    void prepareToPlay (double currentSampleRate, int samplesPerBlock)
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

                // Trigger callback at the exact sample where beat occurs
                // if (onBeatCallback)
                // {
                //     onBeatCallback (isStrongBeat);
                // }
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
            // ✅ Reset click position when disabling
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

private:
    void generateClickSounds()
    {
        // Generate strong click (higher pitched, longer)
        int strongClickLength = (int) (sampleRate * 0.01); // 10ms
        strongClickBuffer.setSize (1, strongClickLength);

        for (int i = 0; i < strongClickLength; ++i)
        {
            float t = (float) i / (float) sampleRate;
            float envelope = std::exp (-t * 200.0f); // Fast decay
            float frequency = 1200.0f;               // Higher pitch for strong beat
            float sample = std::sin (2.0f * juce::MathConstants<float>::pi * frequency * t) * envelope;
            strongClickBuffer.setSample (0, i, sample * 2.0f); // Louder
        }

        // Generate weak click (lower pitched, shorter)
        int weakClickLength = (int) (sampleRate * 0.008); // 8ms
        weakClickBuffer.setSize (1, weakClickLength);

        for (int i = 0; i < weakClickLength; ++i)
        {
            float t = (float) i / (float) sampleRate;
            float envelope = std::exp (-t * 250.0f); // Faster decay
            float frequency = 800.0f;                // Lower pitch for weak beat
            float sample = std::sin (2.0f * juce::MathConstants<float>::pi * frequency * t) * envelope;
            weakClickBuffer.setSample (0, i, sample * 1.5f); // Softer
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
    std::atomic<bool> enabled { false }; // ✅ Thread-safe
    std::atomic<float> volume { 0.8f };  // ✅ Also make volume atomic

    struct TimeSignature
    {
        int numerator;
        int denominator;
    } timeSignature { 4, 4 };

    int bpm;
    juce::AudioBuffer<float> strongClickBuffer;
    juce::AudioBuffer<float> weakClickBuffer;
    juce::AudioBuffer<float>* currentClickBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Metronome)
};

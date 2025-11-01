#pragma once

#include <JuceHeader.h>

class Metronome
{
public:
    Metronome() { bpm = 120; }

    ~Metronome() = default;

    void setEnabled (bool shouldBeEnabled) { enabled = shouldBeEnabled; }
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
    }

    void setStrongBeat (int beatIndex, bool isStrong)
    {
        if (isStrong)
            strongBeatIndex = beatIndex;
        else
            strongBeatIndex = -1;
    }

    void disableStrongBeat() { strongBeatIndex = -1; }

    void syncToLoopStart()
    {
        samplesSinceLastBeat = samplesPerBeat; // This will trigger the beat immediately
        currentBeat = 0;
        // Force click initialization
        bool isStrongBeat = (strongBeatIndex >= 0 && currentBeat == strongBeatIndex);
        currentClickBuffer = isStrongBeat ? &strongClickBuffer : &weakClickBuffer;
        currentClickPosition = 0;
    }

    void syncToPosition (int loopPositionSamples)
    {
        if (samplesPerBeat > 0)
        {
            int totalBeats = loopPositionSamples / samplesPerBeat;
            currentBeat = totalBeats % timeSignature.numerator;
            samplesSinceLastBeat = loopPositionSamples % samplesPerBeat;
            // Force click initialization regardless of position
            bool isStrongBeat = (strongBeatIndex >= 0 && currentBeat == strongBeatIndex);
            currentClickBuffer = isStrongBeat ? &strongClickBuffer : &weakClickBuffer;
            currentClickPosition = 0;
        }
    }
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
        if (! enabled) return;
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // More precise beat timing
            double exactBeatPosition = static_cast<double> (samplesSinceLastBeat) / samplesPerBeat;
            bool timeForNewBeat = exactBeatPosition >= 1.0;

            if (timeForNewBeat)
            {
                samplesSinceLastBeat = samplesSinceLastBeat - samplesPerBeat; // Keep remainder
                bool isStrongBeat = (strongBeatIndex >= 0 && currentBeat == strongBeatIndex);

                currentClickBuffer = isStrongBeat ? &strongClickBuffer : &weakClickBuffer;
                currentClickPosition = 0;

                currentBeat = (currentBeat + 1) % timeSignature.numerator;
            }

            // Play the click sound if we have one active
            if (currentClickBuffer && currentClickPosition < currentClickBuffer->getNumSamples())
            {
                float clickSample = currentClickBuffer->getSample (0, currentClickPosition) * volume;

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    buffer.addSample (ch, sample, clickSample);
                }

                currentClickPosition++;
            }

            samplesSinceLastBeat++;
        }
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
    bool enabled = false;

    struct TimeSignature
    {
        int numerator;
        int denominator;
    } timeSignature { 4, 4 };

    int bpm;
    juce::AudioBuffer<float> strongClickBuffer;
    juce::AudioBuffer<float> weakClickBuffer;
    juce::AudioBuffer<float>* currentClickBuffer;

    float volume = 0.8f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Metronome)
};

#pragma once

#include "SoundTouch.h"
#include "engine/BufferManager.h"
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

class PlaybackEngine
{
public:
    PlaybackEngine() {}

    void prepareToPlay (const double currentSampleRate, const int bufferSize, const int numChannels, const int blockSize)
    {
        interpolationBuffer->setSize ((int) numChannels, (int) bufferSize, false, true, true);
        soundTouchProcessors.clear();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            soundTouchProcessors.push_back (std::make_unique<soundtouch::SoundTouch>());
        }

        for (auto& soundTouchProcessor : soundTouchProcessors)
        {
            soundTouchProcessor->setSampleRate ((uint) currentSampleRate);
            soundTouchProcessor->setChannels (1);
            soundTouchProcessor->setPitchSemiTones (0);
            soundTouchProcessor->setSetting (SETTING_USE_QUICKSEEK, 0);
            soundTouchProcessor->setSetting (SETTING_USE_AA_FILTER, 1);
            soundTouchProcessor->setSetting (SETTING_SEQUENCE_MS, 82);
            soundTouchProcessor->setSetting (SETTING_SEEKWINDOW_MS, 28);
            soundTouchProcessor->setSetting (SETTING_OVERLAP_MS, 12);
        }

        zeroBuffer.resize ((size_t) blockSize);
        std::fill (zeroBuffer.begin(), zeroBuffer.end(), 0.0f);
    }

    void releaseResources()
    {
        clear();
        interpolationBuffer->setSize (0, 0, false, false, true);
        soundTouchProcessors.clear();
        zeroBuffer.clear();
    }

    void clear()
    {
        interpolationBuffer->clear();
        playbackSpeed = 1.0f;
        playheadDirection = 1;

        for (auto& st : soundTouchProcessors)
        {
            st->clear();
        }
    }

    float getPlaybackSpeed() const { return playbackSpeed; }
    void setPlaybackSpeed (const float newSpeed)
    {
        if (newSpeed > 0.0f)
        {
            playbackSpeed = newSpeed;
        }
    }

    bool shouldKeepPitchWhenChangingSpeed() const { return keepPitchWhenChangingSpeed; }
    void setKeepPitchWhenChangingSpeed (const bool shouldKeepPitch)
    {
        for (auto& st : soundTouchProcessors)
        {
            st->flush();
            st->clear();
        }

        keepPitchWhenChangingSpeed = shouldKeepPitch;
    }

    bool isPlaybackDirectionForward() const { return playheadDirection > 0; }
    void setPlaybackDirectionForward()
    {
        if (! isPlaybackDirectionForward()) playheadDirection = 1;
    }
    void setPlaybackDirectionBackward()
    {
        if (isPlaybackDirectionForward()) playheadDirection = -1;
    }
    void setPlaybackPitchSemitones (const float semitones) { playbackPitchSemitones = juce::jlimit (-2.0f, 2.0f, semitones); }
    double getPlaybackPitchSemitones() const { return playbackPitchSemitones; }

    void processPlayback (juce::AudioBuffer<float>& output, BufferManager& audioBufferManager, const int numSamples)
    {
        PERFETTO_FUNCTION();
        if (shouldNotPlayback (audioBufferManager.getLength(), numSamples)) return;

        bool useFastPath = (std::abs (playbackSpeed - 1.0f) < 0.01f && isPlaybackDirectionForward()
                            && std::abs (playbackPitchSemitones - 0.0) < 0.01);

        if (useFastPath)
        {
            if (! wasUsingFastPath)
            {
                for (auto& st : soundTouchProcessors)
                {
                    st->setRate (1.0);
                    st->setTempo (1.0);
                    st->setPitch (1.0);
                }
            }

            int channelsToFeed = std::min ((int) soundTouchProcessors.size(), output.getNumChannels());
            for (int ch = 0; ch < channelsToFeed; ++ch)
            {
                soundTouchProcessors[(size_t) ch]->putSamples (zeroBuffer.data(), (uint) numSamples);

                while (soundTouchProcessors[(size_t) ch]->numSamples() > (uint) (numSamples * 2))
                {
                    soundTouchProcessors[(size_t) ch]->receiveSamples (zeroBuffer.data(), (uint) numSamples);
                }
            }

            processPlaybackNormalSpeedForward (output, audioBufferManager, numSamples);
        }
        else
        {
            processPlaybackInterpolatedSpeed (output, audioBufferManager, numSamples);
        }

        wasUsingFastPath = useFastPath;
    }

private:
    std::unique_ptr<juce::AudioBuffer<float>> interpolationBuffer = std::make_unique<juce::AudioBuffer<float>>();
    std::vector<std::unique_ptr<soundtouch::SoundTouch>> soundTouchProcessors;
    std::vector<float> zeroBuffer;

    bool keepPitchWhenChangingSpeed = false;

    float previousSpeedMultiplier = 1.0f;
    float playbackSpeed = 1.0f;
    double playbackPitchSemitones = 0.0;

    bool previousKeepPitch = false;
    bool wasUsingFastPath = true;

    int playheadDirection = 1; // 1 = forward, -1 = backward
    float playbackSpeedBeforeRecording = 1.0f;
    int playheadDirectionBeforeRecording = 1;

    bool shouldNotPlayback (const int trackLength, const int numSamples) const { return trackLength <= 0 || numSamples <= 0; }

    void processPlaybackInterpolatedSpeed (juce::AudioBuffer<float>& output, BufferManager& audioBufferManager, const int numSamples)
    {
        PERFETTO_FUNCTION();

        float speedMultiplier = playbackSpeed * (float) playheadDirection;
        int maxSourceSamples = (int) ((float) numSamples * std::abs (speedMultiplier));
        int outputOffset = maxSourceSamples + 100;
        bool speedChanged = std::abs (speedMultiplier - previousSpeedMultiplier) > 0.001f;
        bool modeChanged = (shouldKeepPitchWhenChangingSpeed() != previousKeepPitch);
        previousSpeedMultiplier = speedMultiplier;
        previousKeepPitch = shouldKeepPitchWhenChangingSpeed();

        audioBufferManager.linearizeAndReadFromAudioBuffer (*interpolationBuffer, maxSourceSamples, numSamples, speedMultiplier);

        int channelsToProcess = (int) std::min ({ output.getNumChannels(),
                                                  audioBufferManager.getNumChannels(),
                                                  interpolationBuffer->getNumChannels(),
                                                  (int) soundTouchProcessors.size() });

        for (int ch = 0; ch < channelsToProcess; ++ch)
        {
            auto& st = soundTouchProcessors[(size_t) ch];

            st->setPitchSemiTones (playbackPitchSemitones);
            if (speedChanged || modeChanged)
            {
                if (shouldKeepPitchWhenChangingSpeed())
                {
                    st->setRate (1.0);            // Reset rate to neutral
                    st->setTempo (playbackSpeed); // Control speed via tempo
                }
                else
                {
                    st->setTempo (1.0);          // Reset tempo to neutral
                    st->setRate (playbackSpeed); // Control speed via rate
                }
            }

            st->putSamples (interpolationBuffer->getReadPointer (ch), (uint) maxSourceSamples);
            while (st->numSamples() < (uint) numSamples)
            {
                st->putSamples (interpolationBuffer->getReadPointer (ch), (uint) maxSourceSamples);
            }

            uint received = st->receiveSamples (interpolationBuffer->getWritePointer (ch) + outputOffset, (uint) numSamples);

            if (received < (uint) numSamples)
                juce::FloatVectorOperations::clear (interpolationBuffer->getWritePointer (ch) + outputOffset + received,
                                                    (int) ((uint) numSamples - received));

            output.addFrom (ch, 0, *interpolationBuffer, ch, outputOffset, (int) numSamples);
        }
    }

    void processPlaybackNormalSpeedForward (juce::AudioBuffer<float>& output, BufferManager& audioBufferManager, const int numSamples)
    {
        PERFETTO_FUNCTION();
        audioBufferManager.readFromAudioBuffer ([&] (float* dest, const float* source, const int samples)
                                                { juce::FloatVectorOperations::add (dest, source, samples); },
                                                output,
                                                numSamples,
                                                playbackSpeed * (float) playheadDirection);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackEngine)
};

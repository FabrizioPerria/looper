#pragma once

#include "LoopFifo.h"
#include "SoundTouch.h"
#include "UndoBuffer.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

class LoopTrack
{
public:
    LoopTrack() {}
    ~LoopTrack() { releaseResources(); }

    void prepareToPlay (const double currentSampleRate,
                        const int maxBlockSize,
                        const int numChannels,
                        const int maxSeconds = MAX_SECONDS_HARD_LIMIT,
                        const int maxUndoLayers = MAX_UNDO_LAYERS);
    void releaseResources();

    void processRecord (const juce::AudioBuffer<float>& input, const int numSamples);
    void finalizeLayer();
    void processPlayback (juce::AudioBuffer<float>& output, const int numSamples);

    void clear();
    void undo();
    void redo();

    const juce::AudioBuffer<float>& getAudioBuffer() const { return *audioBuffer; }

    int getCurrentReadPosition() const { return fifo.getReadPos(); }
    int getCurrentWritePosition() const { return fifo.getWritePos(); }

    int getLength() const { return length; }
    void setLength (const int newLength) { length = newLength; }

    double getSampleRate() const { return sampleRate; }
    int getLoopDurationSeconds() const { return (int) (length / sampleRate); }

    void setCrossFadeLength (const int newLength) { crossFadeLength = newLength; }

    bool isPrepared() const { return alreadyPrepared; }

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

    const UndoBuffer& getUndoBuffer() const { return undoBuffer; }

    void loadBackingTrack (const juce::AudioBuffer<float>& backingTrack);

    void allowWrapAround() { fifo.setWrapAround (true); }
    void preventWrapAround() { fifo.setWrapAround (false); }

    bool isCurrentlyRecording() const { return isRecording; }

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

    bool isSoloed() const { return soloed; }
    void setSoloed (const bool shouldBeSoloed) { soloed = shouldBeSoloed; }

    float getTrackVolume() const { return trackVolume; }
    void setTrackVolume (const float newVolume) { trackVolume = std::clamp (newVolume, 0.0f, 1.0f); }
    bool isPlaybackDirectionForward() const { return playheadDirection > 0; }
    void setPlaybackDirectionForward()
    {
        if (! isPlaybackDirectionForward())
        {
            playheadDirection = 1;
            fifo.setPlaybackRate (playbackSpeed, playheadDirection);
        }
    }
    void setPlaybackDirectionBackward()
    {
        if (isPlaybackDirectionForward())
        {
            playheadDirection = -1;
            fifo.setPlaybackRate (playbackSpeed, playheadDirection);
        }
    }

    void setPlaybackSpeed (float speed)
    {
        PERFETTO_FUNCTION();
        float newSpeed = std::clamp (speed, 0.2f, 2.0f);

        if (std::abs (playbackSpeed - newSpeed) > 0.001f)
        {
            playbackSpeed = newSpeed;
            fifo.setPlaybackRate (playbackSpeed, playheadDirection);
        }
    }
    float getPlaybackSpeed() const { return playbackSpeed; }

    double getExactReadPosition() const { return fifo.getExactReadPos(); }

    bool shouldKeepPitchWhenChangingSpeed() const { return keepPitchWhenChangingSpeed; }
    void setKeepPitchWhenChangingSpeed (const bool shouldKeepPitch)
    {
        for (auto& st : soundTouchProcessors)
        {
            st->clear();
        }

        keepPitchWhenChangingSpeed = shouldKeepPitch;
    }

    bool hasWrappedAround()
    {
        double current = fifo.getExactReadPos();
        bool wrapped = current < previousReadPos;
        previousReadPos = current;
        return wrapped;
    }

private:
    std::unique_ptr<juce::AudioBuffer<float>> audioBuffer = std::make_unique<juce::AudioBuffer<float>>();
    std::unique_ptr<juce::AudioBuffer<float>> tmpBuffer = std::make_unique<juce::AudioBuffer<float>>();
    std::unique_ptr<juce::AudioBuffer<float>> interpolationBuffer = std::make_unique<juce::AudioBuffer<float>>();
    std::vector<std::unique_ptr<soundtouch::SoundTouch>> soundTouchProcessors;
    std::vector<float> zeroBuffer;

    bool keepPitchWhenChangingSpeed = false;
    double previousReadPos = 0.0;

    float playbackSpeed = 1.0f;
    int playheadDirection = 1; // 1 = forward, -1 = backward
    float playbackSpeedBeforeRecording = 1.0f;
    int playheadDirectionBeforeRecording = 1;

    UndoBuffer undoBuffer;

    double sampleRate = 0.0;
    int blockSize = 0;
    int channels = 0;
    size_t alignedBufferSize = 0;

    LoopFifo fifo;

    int length = 0;
    int provisionalLength = 0;
    int crossFadeLength = 0;

    bool isRecording = false;
    bool alreadyPrepared = false;

    double overdubNewGain = 1.0;
    double overdubOldGain = 1.0;

    bool shouldNormalizeOutput = true;

    float trackVolume = 1.0f;
    float previousTrackVolume = 1.0f;

    bool muted = false;
    bool soloed = false;

    void processRecordChannel (const juce::AudioBuffer<float>& input, const int numSamples, const int ch);
    void updateLoopLength (const int numSamples, const int bufferSamples);
    void saveToUndoBuffer();

    void copyInputToLoopBuffer (const int ch, const float* bufPtr, const int offset, const int numSamples);
    void copyCircularDataLinearized (int startPos, int numSamples, float speedMultiplier, int destOffset = 0);

    void advanceWritePos (const int numSamples, const int bufferSamples);
    void advanceReadPos (const int numSamples, const int bufferSamples);

    void processPlaybackChannel (juce::AudioBuffer<float>& output, const int numSamples, const int ch);

    void processPlaybackInterpolatedSpeedWithPitchCorrection (juce::AudioBuffer<float>& output, const int numSamples);
    void processPlaybackInterpolatedSpeed (juce::AudioBuffer<float>& output, const int numSamples);
    void processPlaybackApplyVolume (juce::AudioBuffer<float>& output, const int numSamples);
    void processPlaybackNormalSpeedForward (juce::AudioBuffer<float>& output, const int numSamples);

    bool shouldNotRecordInputBuffer (const juce::AudioBuffer<float>& input, const int numSamples) const;

    bool shouldNotPlayback (const int numSamples) const { return ! isPrepared() || length == 0 || numSamples == 0; }

    bool shouldOverdub() const { return length > 0; }

    void copyAudioToTmpBuffer();

    static const int MAX_SECONDS_HARD_LIMIT = 10 * 60; // 10 minutes
    static const int MAX_UNDO_LAYERS = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTrack)
};

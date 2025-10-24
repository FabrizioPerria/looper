#pragma once

#include "UndoManager.h"
#include "engine/BufferManager.h"
#include "engine/PlaybackEngine.h"
#include "engine/VolumeProcessor.h"
#include "juce_audio_basics/juce_audio_basics.h"
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
    bool undo();
    bool redo();

    int getCurrentReadPosition() const { return bufferManager.getReadPosition(); }
    int getCurrentWritePosition() const { return bufferManager.getWritePosition(); }

    int getLoopDurationSeconds() const { return (int) (bufferManager.getLength() / sampleRate); }

    void setCrossFadeLength (const int newCrossFadeLength) { volumeProcessor.setCrossFadeLength (newCrossFadeLength); }

    void loadBackingTrack (const juce::AudioBuffer<float>& backingTrack);
    juce::AudioBuffer<float>* getAudioBuffer() { return bufferManager.getAudioBuffer().get(); }

    const int getAvailableTrackSizeSamples() const { return (int) alignedBufferSize; }

    bool isCurrentlyRecording() const { return isRecording; }

    bool isPlaybackDirectionForward() const { return playbackEngine.isPlaybackDirectionForward(); }
    void setPlaybackDirectionForward() { playbackEngine.setPlaybackDirectionForward(); }
    void setPlaybackDirectionBackward() { playbackEngine.setPlaybackDirectionBackward(); }

    void setPlaybackSpeed (float speed) { playbackEngine.setPlaybackSpeed (speed); }
    float getPlaybackSpeed() const { return playbackEngine.getPlaybackSpeed(); }

    bool shouldKeepPitchWhenChangingSpeed() const { return playbackEngine.shouldKeepPitchWhenChangingSpeed(); }
    void setKeepPitchWhenChangingSpeed (const bool shouldKeepPitch) { playbackEngine.setKeepPitchWhenChangingSpeed (shouldKeepPitch); }

    bool hasWrappedAround() { return bufferManager.hasWrappedAround(); }

    float getTrackVolume() const { return volumeProcessor.getTrackVolume(); }
    void setTrackVolume (const float newVolume) { volumeProcessor.setTrackVolume (newVolume); }

    void setOverdubGains (const double oldGain, const double newGain) { volumeProcessor.setOverdubGains (oldGain, newGain); }

    bool isSoloed() const { return volumeProcessor.isSoloed(); }
    void setSoloed (const bool shouldBeSoloed) { volumeProcessor.setSoloed (shouldBeSoloed); }

    bool isMuted() const { return volumeProcessor.isMuted(); }
    void setMuted (const bool shouldBeMuted) { volumeProcessor.setMuted (shouldBeMuted); }

    int getTrackLengthSamples() const { return bufferManager.getLength(); }

    void cancelCurrentRecording();

private:
    VolumeProcessor volumeProcessor;
    BufferManager bufferManager;
    UndoStackManager undoManager;
    PlaybackEngine playbackEngine;

    double sampleRate = 0.0;
    int blockSize = 0;
    int channels = 0;
    size_t alignedBufferSize = 0;

    bool isRecording = false;

    void processRecordChannel (const juce::AudioBuffer<float>& input, const int numSamples, const int ch);
    bool shouldNotRecordInputBuffer (const juce::AudioBuffer<float>& input, const int numSamples) const
    {
        return numSamples == 0 || (int) input.getNumSamples() < numSamples || input.getNumChannels() != bufferManager.getNumChannels();
    }

    static const int MAX_SECONDS_HARD_LIMIT = 10 * 60; // 10 minutes
    static const int MAX_UNDO_LAYERS = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTrack)
};

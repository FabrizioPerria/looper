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

    void initializeForNewOverdubSession();
    void processRecord (const juce::AudioBuffer<float>& input, const int numSamples, const bool isOverdub);
    void finalizeLayer (const bool isOverdub);

    void processPlayback (juce::AudioBuffer<float>& output, const int numSamples, const bool isOverdub);

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

    bool isPlaybackDirectionForward() const { return playbackEngine.isPlaybackDirectionForward(); }
    void setPlaybackDirectionForward() { playbackEngine.setPlaybackDirectionForward(); }
    void setPlaybackDirectionBackward() { playbackEngine.setPlaybackDirectionBackward(); }

    void setPlaybackSpeed (float speed) { playbackEngine.setPlaybackSpeed (speed); }
    float getPlaybackSpeed() const { return playbackEngine.getPlaybackSpeed(); }

    void setPlaybackPitch (double pitch) { playbackEngine.setPlaybackPitchSemitones (pitch); }
    double getPlaybackPitch() const { return playbackEngine.getPlaybackPitchSemitones(); }

    bool shouldKeepPitchWhenChangingSpeed() const { return playbackEngine.shouldKeepPitchWhenChangingSpeed(); }
    void setKeepPitchWhenChangingSpeed (const bool shouldKeepPitch) { playbackEngine.setKeepPitchWhenChangingSpeed (shouldKeepPitch); }

    bool hasWrappedAround() { return bufferManager.hasWrappedAround(); }

    float getTrackVolume() const { return volumeProcessor.getTrackVolume(); }
    void setTrackVolume (const float newVolume) { volumeProcessor.setTrackVolume (newVolume); }

    void toggleNormalizingOutput() { volumeProcessor.toggleOutputNormalization(); }
    bool isOutputNormalized() const { return volumeProcessor.isNormalizingOutput(); }

    void setOverdubGainNew (const double newGain) { volumeProcessor.setOverdubNewGain (newGain); }
    void setOverdubGainOld (const double oldGain) { volumeProcessor.setOverdubOldGain (oldGain); }
    double getOverdubGainNew() const { return volumeProcessor.getOverdubNewGain(); }
    double getOverdubGainOld() const { return volumeProcessor.getOverdubOldGain(); }

    bool isSoloed() const { return volumeProcessor.isSoloed(); }
    void setSoloed (const bool shouldBeSoloed) { volumeProcessor.setSoloed (shouldBeSoloed); }

    bool isMuted() const { return volumeProcessor.isMuted(); }
    void setMuted (const bool shouldBeMuted) { volumeProcessor.setMuted (shouldBeMuted); }

    int getTrackLengthSamples() const { return bufferManager.getLength(); }

    void cancelCurrentRecording();

    void setLoopRegion (int startSample, int endSample) { bufferManager.setLoopRegion (startSample, endSample); }
    void clearLoopRegion() { bufferManager.clearLoopRegion(); }
    bool hasLoopRegion() const { return bufferManager.hasLoopRegion(); }
    int getLoopRegionStart() const { return bufferManager.getLoopRegionStart(); }
    int getLoopRegionEnd() const { return bufferManager.getLoopRegionEnd(); }

private:
    VolumeProcessor volumeProcessor;
    BufferManager bufferManager;
    UndoStackManager undoManager;
    PlaybackEngine playbackEngine;

    double sampleRate = 0.0;
    int blockSize = 0;
    int channels = 0;
    size_t alignedBufferSize = 0;

    // Recording state - tracks if we have unfinalized recording data
    // This is set when processRecord is called and cleared when finalizeLayer/cancelCurrentRecording is called
    // bool hitRecordingLimit = false;

    void processRecordChannel (const juce::AudioBuffer<float>& input, const int numSamples, const int ch);
    void applyPostProcessing (juce::AudioBuffer<float>& audioBuffer, int length);

    static const int MAX_SECONDS_HARD_LIMIT = 10 * 60; // 10 minutes
    static const int MAX_UNDO_LAYERS = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTrack)
};

#include "LooperEngine.h"
#include "engine/midiMappings.h"

LooperEngine::LooperEngine()
    : transportState (TransportState::Stopped)
    , sampleRate (0.0)
    , maxBlockSize (0)
    , numChannels (0)
    , numTracks (0)
    , activeTrackIndex (0)
    , nextTrackIndex (-1)
{
}

LooperEngine::~LooperEngine() { releaseResources(); }

void LooperEngine::prepareToPlay (double newSampleRate, int newMaxBlockSize, int newNumTracks, int newNumChannels)
{
    PERFETTO_FUNCTION();
    if (newSampleRate <= 0.0 || newMaxBlockSize <= 0 || newNumChannels <= 0 || newNumTracks <= 0) return;

    releaseResources();
    sampleRate = newSampleRate;
    maxBlockSize = newMaxBlockSize;
    numChannels = newNumChannels;

    for (int i = 0; i < newNumTracks; ++i)
        addTrack();

    setupMidiCommands();
}

void LooperEngine::releaseResources()
{
    PERFETTO_FUNCTION();
    for (auto& track : loopTracks)
        track->releaseResources();

    loopTracks.clear();
    uiBridges.clear();
    bridgeInitialized.clear();

    sampleRate = 0.0;
    maxBlockSize = 0;
    numChannels = 0;
    numTracks = 0;
    activeTrackIndex = 0;
    transportState = TransportState::Stopped;
    nextTrackIndex = -1;
    midiCommandMap.clear();
}

void LooperEngine::addTrack()
{
    PERFETTO_FUNCTION();
    auto track = std::make_unique<LoopTrack>();
    track->prepareToPlay (sampleRate, maxBlockSize, numChannels);
    loopTracks.push_back (std::move (track));

    uiBridges.push_back (std::make_unique<AudioToUIBridge>());
    bridgeInitialized.push_back (false);

    numTracks = static_cast<int> (loopTracks.size());
    activeTrackIndex = numTracks - 1;
}

void LooperEngine::selectTrack (const int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return;

    if (transportState == TransportState::Stopped || loopTracks[(size_t) activeTrackIndex]->getLength() == 0)
        activeTrackIndex = trackIndex;
    else
        //we do not switch the track here. The actual switching happens in processBlock so we finish the loop cycle before switching. Here, we signal the swap.
        nextTrackIndex = trackIndex;
}

void LooperEngine::removeTrack (const int trackIndex)
{
    PERFETTO_FUNCTION();
    if (activeTrackIndex == trackIndex || trackIndex < 0 || trackIndex >= numTracks) return;

    loopTracks.erase (loopTracks.begin() + trackIndex);
    uiBridges.erase (uiBridges.begin() + trackIndex);
    bridgeInitialized.erase (bridgeInitialized.begin() + trackIndex);

    numTracks = static_cast<int> (loopTracks.size());
    if (activeTrackIndex >= numTracks) activeTrackIndex = numTracks - 1;
}

void LooperEngine::undo (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex >= numTracks || trackIndex < 0) trackIndex = activeTrackIndex;
    loopTracks[(size_t) trackIndex]->undo();
    transportState = TransportState::Playing;
    uiBridges[(size_t) trackIndex]->signalWaveformChanged();
}

void LooperEngine::redo (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex >= numTracks || trackIndex < 0) trackIndex = activeTrackIndex;
    loopTracks[(size_t) trackIndex]->redo();
    transportState = TransportState::Playing;
    uiBridges[(size_t) trackIndex]->signalWaveformChanged();
}

void LooperEngine::clear (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex >= numTracks || trackIndex < 0) trackIndex = activeTrackIndex;
    loopTracks[(size_t) trackIndex]->clear();
    transportState = TransportState::Stopped;
    uiBridges[(size_t) trackIndex]->clear();
    bridgeInitialized[(size_t) trackIndex] = false;
    uiBridges[(size_t) trackIndex]->signalWaveformChanged();
}

void LooperEngine::startRecording()
{
    PERFETTO_FUNCTION();
    transportState = TransportState::Recording;
}

void LooperEngine::startPlaying()
{
    PERFETTO_FUNCTION();
    transportState = TransportState::Playing;
}

void LooperEngine::stop()
{
    PERFETTO_FUNCTION();
    if (isRecording())
    {
        loopTracks[(size_t) activeTrackIndex]->finalizeLayer();
        uiBridges[(size_t) activeTrackIndex]->signalWaveformChanged();
        transportState = TransportState::Playing;
    }
    else
        transportState = TransportState::Stopped;
}

void LooperEngine::setupMidiCommands()
{
    PERFETTO_FUNCTION();

    midiCommandMap[{ RECORD_BUTTON_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int /**/)
    {
        if (engine.getTransportState() != TransportState::Recording)
            engine.startRecording();
        else
            engine.stop();
    };
    midiCommandMap[{ TOGGLE_PLAY_BUTTON_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int /**/)
    {
        if (engine.getTransportState() == TransportState::Stopped)
            engine.startPlaying();
        else
            engine.stop();
    };
    midiCommandMap[{ UNDO_BUTTON_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int trackIndex) { engine.undo (trackIndex); };
    midiCommandMap[{ REDO_BUTTON_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int trackIndex) { engine.redo (trackIndex); };
    midiCommandMap[{ CLEAR_BUTTON_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int trackIndex) { engine.clear (trackIndex); };
    midiCommandMap[{ NEXT_TRACK_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int /**/) { engine.selectNextTrack(); };
    midiCommandMap[{ PREV_TRACK_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int /**/) { engine.selectPreviousTrack(); };
    midiCommandMap[{ SOLO_BUTTON_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int trackIndex)
    {
        auto* track = engine.getTrackByIndex (trackIndex);
        if (track) engine.setTrackSoloed (trackIndex, ! track->isSoloed());
    };
    midiCommandMap[{ MUTE_BUTTON_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int trackIndex)
    {
        auto* track = engine.getTrackByIndex (trackIndex);
        if (track) engine.setTrackMuted (trackIndex, ! track->isMuted());
    };

    juce::File defaultFile = juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("backing.wav");
    midiCommandMap[{ LOAD_BUTTON_MIDI_NOTE, NOTE_ON }] = [defaultFile] (LooperEngine& engine, int trackIndex)
    { engine.loadWaveFileToTrack (defaultFile, trackIndex); };
    midiCommandMap[{ REVERSE_BUTTON_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int trackIndex)
    {
        auto* track = engine.getTrackByIndex (trackIndex);
        if (track)
        {
            if (track->isPlaybackDirectionForward())
                track->setPlaybackDirectionBackward();
            else
                track->setPlaybackDirectionForward();
        }
    };
    midiCommandMap[{ KEEP_PITCH_BUTTON_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int trackIndex)
    {
        auto currentSetting = engine.getKeepPitchWhenChangingSpeed (trackIndex);
        engine.setKeepPitchWhenChangingSpeed (trackIndex, ! currentSetting);
    };
}

void LooperEngine::handleMidiCommand (const juce::MidiBuffer& midiMessages)
{
    PERFETTO_FUNCTION();
    if (midiMessages.getNumEvents() > 0)
    {
        int targetTrack = activeTrackIndex;
        for (auto midi : midiMessages)
        {
            auto m = midi.getMessage();
            if (m.isController() && m.getControllerNumber() == TRACK_SELECT_CC)
            {
                targetTrack = juce::jlimit (0, numTracks - 1, m.getControllerValue() % numTracks);
                selectTrack (targetTrack);
                continue;
            }

            if (m.isController() && m.getControllerNumber() == TRACK_VOLUME_CC)
            {
                float volume = (float) m.getControllerValue() / 127.0f;
                setTrackVolume (targetTrack, volume);
                continue;
            }

            if (m.isController() && m.getControllerNumber() == PLAYBACK_SPEED_CC)
            {
                float normalizedValue = (float) m.getControllerValue() / 127.0f; // 0.0 to 1.0
                float speed = 0.2f + (normalizedValue * 1.8f);                   // Maps to 0.2-2.0 ✓
                setTrackPlaybackSpeed (targetTrack, speed);
                continue;
            }

            auto it = midiCommandMap.find ({ m.getNoteNumber(), m.isNoteOn() });
            if (it != midiCommandMap.end()) it->second (*this, targetTrack);
        }
    }
}

void LooperEngine::processBlock (const juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    PERFETTO_FUNCTION();
    handleMidiCommand (midiMessages);

    auto* activeTrack = getActiveTrack();
    auto* bridge = getUIBridgeByIndex (activeTrackIndex);

    if (! activeTrack || ! bridge) return;

    bool wasRecording = activeTrack->isCurrentlyRecording();

    switch (transportState)
    {
        case TransportState::Recording:
            activeTrack->processRecord (buffer, buffer.getNumSamples());
            // Note: do not break here, we want to also play back while recording
        case TransportState::Playing:
            // Handle track switching. If we are switching, we wait until the current track has finished its loop
            if (nextTrackIndex >= 0 && nextTrackIndex != activeTrackIndex && activeTrack->hasWrappedAround())
            {
                stop();
                transportState = TransportState::Playing; //ensure we are not in recording state when switching
                activeTrackIndex = nextTrackIndex;
                activeTrack = getActiveTrack();
                bridge = getUIBridgeByIndex (activeTrackIndex);
                nextTrackIndex = -1;
            }
            activeTrack->processPlayback (const_cast<juce::AudioBuffer<float>&> (buffer), buffer.getNumSamples());
            break;
        case TransportState::Stopped:
            break;
    }

    bool nowRecording = activeTrack->isCurrentlyRecording();

    // Handle waveform update signals
    if (! bridgeInitialized[(size_t) activeTrackIndex] && activeTrack->getLength() > 0)
    {
        bridge->signalWaveformChanged();
        bridgeInitialized[(size_t) activeTrackIndex] = true;
    }

    bool isFinalizing = wasRecording && ! nowRecording;
    if (isFinalizing)
    {
        bridge->signalWaveformChanged();
        bridge->resetRecordingCounter();
    }

    if (nowRecording && bridge->shouldUpdateWhileRecording (buffer.getNumSamples(), sampleRate))
    {
        bridge->signalWaveformChanged();
    }

    // Determine length to show
    int lengthToShow = activeTrack->getLength();
    if (lengthToShow == 0 && nowRecording)
        lengthToShow = std::min (activeTrack->getCurrentWritePosition() + 200, (int) activeTrack->getAudioBuffer().getNumSamples());

    // Update bridge
    bridge->updateFromAudioThread (&activeTrack->getAudioBuffer(),
                                   lengthToShow,
                                   activeTrack->getCurrentReadPosition(),
                                   nowRecording,
                                   transportState == TransportState::Playing);
}

LoopTrack* LooperEngine::getActiveTrack()
{
    PERFETTO_FUNCTION();
    if (loopTracks.empty() || activeTrackIndex < 0 || activeTrackIndex >= numTracks) return nullptr;
    return loopTracks[(size_t) activeTrackIndex].get();
}

void LooperEngine::setOverdubGainsForTrack (const int trackIndex, const double oldGain, const double newGain)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return;

    auto& track = loopTracks[(size_t) trackIndex];
    if (track) track->setOverdubGains (oldGain, newGain);
}

void LooperEngine::loadBackingTrackToTrack (const juce::AudioBuffer<float>& backingTrack, int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return;

    auto& track = loopTracks[(size_t) trackIndex];
    if (track)
    {
        track->loadBackingTrack (backingTrack);
        uiBridges[(size_t) trackIndex]->signalWaveformChanged();
        startPlaying();
    }
}

void LooperEngine::loadWaveFileToTrack (const juce::File& audioFile, int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return;

    auto& track = loopTracks[(size_t) trackIndex];
    if (track)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (audioFile));
        if (reader)
        {
            juce::AudioBuffer<float> backingTrack ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&backingTrack, 0, (int) reader->lengthInSamples, 0, true, true);
            loadBackingTrackToTrack (backingTrack, trackIndex);
        }
    }
}

void LooperEngine::setTrackVolume (int trackIndex, float volume)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return;

    auto& track = loopTracks[(size_t) trackIndex];
    if (track)
    {
        track->setTrackVolume (volume);
    }
}

void LooperEngine::setTrackMuted (int trackIndex, bool muted)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return;

    auto& track = loopTracks[(size_t) trackIndex];
    if (track)
    {
        track->setMuted (muted);
    }
}

void LooperEngine::setTrackSoloed (int trackIndex, bool soloed)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return;

    for (int i = 0; i < numTracks; ++i)
    {
        auto& track = loopTracks[(size_t) i];
        if (track)
        {
            if (i == trackIndex)
            {
                track->setMuted (false);
                track->setSoloed (soloed);
            }
            else
                track->setMuted (soloed);
        }
    }
}

float LooperEngine::getTrackVolume (int trackIndex) const
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return 1.0f;

    auto& track = loopTracks[(size_t) trackIndex];
    if (track) return track->getTrackVolume();
    return 1.0f;
}

bool LooperEngine::isTrackMuted (int trackIndex) const
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return false;

    auto& track = loopTracks[(size_t) trackIndex];
    if (track) return track->isMuted();
    return false;
}

void LooperEngine::setKeepPitchWhenChangingSpeed (int trackIndex, bool shouldKeepPitch)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return;

    auto& track = loopTracks[(size_t) trackIndex];
    if (track) track->setKeepPitchWhenChangingSpeed (shouldKeepPitch);
}

bool LooperEngine::getKeepPitchWhenChangingSpeed (int trackIndex) const
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return false;

    auto& track = loopTracks[(size_t) trackIndex];
    if (track) return track->shouldKeepPitchWhenChangingSpeed();
    return false;
}

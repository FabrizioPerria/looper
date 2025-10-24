#include "LooperEngine.h"
#include "engine/midiMappings.h"
#include <algorithm>

LooperEngine::LooperEngine()
    : looperState (LooperState::Idle)
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
    nextTrackIndex = -1;
    midiCommandMap.clear();
    looperState = LooperState::Idle;
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
    if (trackIndex == activeTrackIndex) return;

    // If recording, cancel and switch immediately
    if (looperState == LooperState::Recording || looperState == LooperState::Overdubbing)
    {
        auto* track = getActiveTrack();
        if (track && track->isCurrentlyRecording())
        {
            track->cancelCurrentRecording();
        }

        activeTrackIndex = trackIndex;
        auto* newTrack = getActiveTrack();

        if (newTrack && newTrack->getTrackLengthSamples() > 0)
        {
            transitionTo (LooperState::Stopped);
        }
        else
        {
            transitionTo (LooperState::Idle);
        }
        return;
    }

    // If idle/stopped or current track is empty, switch immediately
    if (looperState == LooperState::Idle || looperState == LooperState::Stopped || getActiveTrack()->getTrackLengthSamples() == 0)
    {
        activeTrackIndex = trackIndex;
        nextTrackIndex = -1;

        auto* newTrack = getActiveTrack();
        if (newTrack && newTrack->getTrackLengthSamples() > 0)
        {
            transitionTo (LooperState::Stopped);
        }
        else
        {
            transitionTo (LooperState::Idle);
        }
        return;
    }

    // Otherwise, defer until wrap around
    setPendingAction (PendingAction::Type::SwitchTrack, trackIndex, true);
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

    // Don't allow undo during recording
    if (isRecording()) return;

    auto* track = loopTracks[(size_t) trackIndex].get();
    if (track && track->undo())
    {
        uiBridges[(size_t) trackIndex]->signalWaveformChanged();

        // Update state if we undid the last layer
        if (track->getTrackLengthSamples() == 0)
        {
            if (trackIndex == activeTrackIndex)
            {
                transitionTo (LooperState::Idle);
            }
        }
        else
        {
            // Ensure we're in a playing-capable state
            if (looperState == LooperState::Idle && trackIndex == activeTrackIndex)
            {
                transitionTo (LooperState::Stopped);
            }
        }
    }
}

void LooperEngine::redo (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex >= numTracks || trackIndex < 0) trackIndex = activeTrackIndex;

    // Don't allow redo during recording
    if (isRecording()) return;

    auto* track = loopTracks[(size_t) trackIndex].get();
    if (track && track->redo())
    {
        uiBridges[(size_t) trackIndex]->signalWaveformChanged();

        // Update state if we now have content
        if (trackIndex == activeTrackIndex && looperState == LooperState::Idle && track->getTrackLengthSamples() > 0)
        {
            transitionTo (LooperState::Stopped);
        }
    }
}

void LooperEngine::clear (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex >= numTracks || trackIndex < 0) trackIndex = activeTrackIndex;

    loopTracks[(size_t) trackIndex]->clear();
    uiBridges[(size_t) trackIndex]->clear();
    bridgeInitialized[(size_t) trackIndex] = false;
    uiBridges[(size_t) trackIndex]->signalWaveformChanged();

    // If we cleared the active track, update state
    if (trackIndex == activeTrackIndex)
    {
        transitionTo (LooperState::Stopped);
    }
}

void LooperEngine::record()
{
    PERFETTO_FUNCTION();

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    // Determine which recording state to enter
    bool hasContent = activeTrack->getTrackLengthSamples() > 0;

    if (hasContent)
    {
        // Recording on existing loop = overdubbing
        if (looperState == LooperState::Playing || looperState == LooperState::Stopped)
        {
            transitionTo (LooperState::Overdubbing);
        }
    }
    else
    {
        // Recording on empty track = recording
        if (looperState == LooperState::Idle || looperState == LooperState::Stopped)
        {
            transitionTo (LooperState::Recording);
        }
    }
}

void LooperEngine::play()
{
    PERFETTO_FUNCTION();

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    // Can only play if we have content
    if (activeTrack->getTrackLengthSamples() > 0)
    {
        if (looperState == LooperState::Stopped || looperState == LooperState::Idle)
        {
            transitionTo (LooperState::Playing);
        }
    }
}

void LooperEngine::stop()
{
    PERFETTO_FUNCTION();

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    switch (looperState)
    {
        case LooperState::Idle:
        case LooperState::Stopped:
            break;
        case LooperState::Recording:
        case LooperState::Overdubbing:
        {
            // Finalize the recording
            if (activeTrack->isCurrentlyRecording())
            {
                activeTrack->finalizeLayer();
                uiBridges[(size_t) activeTrackIndex]->signalWaveformChanged();
            }

            // Move to appropriate state
            if (activeTrack->getTrackLengthSamples() > 0)
                transitionTo (LooperState::Playing);
            else
                transitionTo (LooperState::Stopped);
            break;
        }

        case LooperState::Playing:
        case LooperState::PendingTrackChange:
        {
            // Stop playback
            if (activeTrack->getTrackLengthSamples() > 0)
                transitionTo (LooperState::Stopped);
            else
                transitionTo (LooperState::Idle);
            break;
        }

        case LooperState::Transitioning:
            transitionTo (LooperState::Stopped);
            break;

        default:
            // Already stopped/idle
            break;
    }
}

void LooperEngine::toggleRecord()
{
    if (isRecording())
        stop();
    else
        record();
}

void LooperEngine::togglePlay()
{
    if (isPlaying())
        stop();
    else
        play();
}

void LooperEngine::setupMidiCommands()
{
    PERFETTO_FUNCTION();

    midiCommandMap[{ RECORD_BUTTON_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int /**/) { engine.toggleRecord(); };
    midiCommandMap[{ TOGGLE_PLAY_BUTTON_MIDI_NOTE, NOTE_ON }] = [] (LooperEngine& engine, int /**/) { engine.togglePlay(); };
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

    // Process any pending actions
    processPendingActions();

    // Process based on current looperState - CLEAN switch statement
    switch (looperState)
    {
        case LooperState::Recording:
            activeTrack->processRecord (buffer, buffer.getNumSamples());
            break;

        case LooperState::Overdubbing:
            activeTrack->processRecord (buffer, buffer.getNumSamples());
            // Fall through to also play
            [[fallthrough]];

        case LooperState::Playing:
        case LooperState::PendingTrackChange:
        case LooperState::Transitioning:
            activeTrack->processPlayback (const_cast<juce::AudioBuffer<float>&> (buffer), buffer.getNumSamples());
            break;

        case LooperState::Idle:
        case LooperState::Stopped:
            // No audio processing
            break;
    }

    bool nowRecording = activeTrack->isCurrentlyRecording();

    // Handle waveform updates
    if (! bridgeInitialized[(size_t) activeTrackIndex] && activeTrack->getTrackLengthSamples() > 0)
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

    // Update UI bridge
    int lengthToShow = activeTrack->getTrackLengthSamples();
    if (lengthToShow == 0 && nowRecording)
    {
        lengthToShow = std::min (activeTrack->getCurrentWritePosition() + 200, (int) activeTrack->getAvailableTrackSizeSamples());
    }

    bool shouldShowPlaying = (looperState == LooperState::Playing || looperState == LooperState::PendingTrackChange
                              || looperState == LooperState::Overdubbing || looperState == LooperState::Transitioning);

    bridge->updateFromAudioThread (activeTrack->getAudioBuffer(),
                                   lengthToShow,
                                   activeTrack->getCurrentReadPosition(),
                                   nowRecording,
                                   shouldShowPlaying);
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
        play();
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

bool LooperEngine::canTransitionTo (LooperState newState) const
{
    PERFETTO_FUNCTION();

    constexpr static uint32_t allowedTransitions[] = {
        [(int) LooperState::Idle] = (1 << (int) LooperState::Recording) | (1 << (int) LooperState::Idle)
                                    | (1 << (int) LooperState::Playing),
        [(int) LooperState::Stopped] = (1 << (int) LooperState::Playing) | (1 << (int) LooperState::Recording)
                                       | (1 << (int) LooperState::Idle)
                                       | (1 << (int) LooperState::Overdubbing), // Allow overdub from stopped
        [(int) LooperState::Playing] = (1 << (int) LooperState::Stopped) | (1 << (int) LooperState::Overdubbing)
                                       | (1 << (int) LooperState::PendingTrackChange),
        [(int) LooperState::Recording] = (1 << (int) LooperState::Playing) | (1 << (int) LooperState::Stopped)
                                         | (1 << (int) LooperState::Idle) | (1 << (int) LooperState::Overdubbing),
        [(int) LooperState::Overdubbing] = (1 << (int) LooperState::Playing) | (1 << (int) LooperState::Stopped),
        [(int) LooperState::PendingTrackChange] = (1 << (int) LooperState::Transitioning) | (1 << (int) LooperState::Playing)
                                                  | (1 << (int) LooperState::Stopped),
        [(int) LooperState::Transitioning] = (1 << (int) LooperState::Playing) | (1 << (int) LooperState::Stopped)
                                             | (1 << (int) LooperState::Idle)
    };

    uint32_t currentAllowed = allowedTransitions[(int) looperState];
    return (currentAllowed & (1 << (int) newState)) != 0;
}

void LooperEngine::transitionTo (LooperState newState)
{
    if (! canTransitionTo (newState))
    {
        DBG ("Invalid state transition from " << (int) looperState << " to " << (int) newState);
        return;
    }

    // Perform cleanup when leaving certain states
    switch (looperState)
    {
        case LooperState::Recording:
        case LooperState::Overdubbing:
            // Ensure recording is properly finalized
            if (newState != LooperState::Stopped && newState != LooperState::Playing)
            {
                auto* track = getActiveTrack();
                if (track && track->isCurrentlyRecording())
                {
                    track->finalizeLayer();
                }
            }
            break;
        case LooperState::Idle:
        case LooperState::Stopped:
        case LooperState::Playing:
        case LooperState::PendingTrackChange:
        case LooperState::Transitioning:
        default:
            break;
    }

    looperState = newState;
}

void LooperEngine::setPendingAction (PendingAction::Type type, int trackIndex, bool waitForWrap)
{
    pendingAction.type = type;
    pendingAction.targetTrackIndex = trackIndex;
    pendingAction.waitForWrapAround = waitForWrap;

    if (type == PendingAction::Type::SwitchTrack && waitForWrap && looperState == LooperState::Playing)
    {
        transitionTo (LooperState::PendingTrackChange);
    }
}

void LooperEngine::processPendingActions()
{
    if (! pendingAction.isActive()) return;

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    // Check if we should wait for wrap around
    if (pendingAction.waitForWrapAround && ! activeTrack->hasWrappedAround())
    {
        return; // Not ready yet
    }

    switch (pendingAction.type)
    {
        case PendingAction::Type::SwitchTrack:
            if (pendingAction.targetTrackIndex >= 0 && pendingAction.targetTrackIndex < numTracks
                && pendingAction.targetTrackIndex != activeTrackIndex)
            {
                transitionTo (LooperState::Transitioning);
                activeTrackIndex = pendingAction.targetTrackIndex;

                // Determine new state based on new track
                auto* newTrack = getActiveTrack();
                if (newTrack && newTrack->getTrackLengthSamples() > 0)
                {
                    transitionTo (LooperState::Playing);
                }
                else
                {
                    transitionTo (LooperState::Stopped);
                }
            }
            break;

        case PendingAction::Type::CancelRecording:
            if (activeTrack->isCurrentlyRecording())
            {
                activeTrack->cancelCurrentRecording();
            }
            break;

        case PendingAction::Type::FinalizeRecording:
            if (activeTrack->isCurrentlyRecording())
            {
                activeTrack->finalizeLayer();
                transitionTo (LooperState::Playing);
            }
            break;

        default:
            break;
    }

    pendingAction.clear();
}

int LooperEngine::getPendingTrackIndex() const
{
    return (pendingAction.type == PendingAction::Type::SwitchTrack) ? pendingAction.targetTrackIndex : -1;
}

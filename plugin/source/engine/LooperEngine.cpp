#include "LooperEngine.h"
#include "audio/EngineCommandBus.h"
#include "engine/LooperStateConfig.h"
#include "engine/MidiCommandConfig.h"
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

LooperEngine::LooperEngine() {}

LooperEngine::~LooperEngine() { releaseResources(); }

void LooperEngine::prepareToPlay (double newSampleRate, int newMaxBlockSize, int newNumChannels)
{
    PERFETTO_FUNCTION();
    if (newSampleRate <= 0.0 || newMaxBlockSize <= 0 || newNumChannels <= 0) return;

    releaseResources();
    sampleRate = newSampleRate;
    maxBlockSize = newMaxBlockSize;
    numChannels = newNumChannels;

    for (int i = 0; i < NUM_TRACKS; ++i)
        addTrack (i);

    metronome->prepareToPlay (sampleRate, maxBlockSize);
    granularFreeze->prepareToPlay (sampleRate, numChannels);

    inputMeter->prepare (numChannels);
    outputMeter->prepare (numChannels);

    setPendingAction (PendingAction::Type::SwitchTrack, 0, false, currentState);
}

void LooperEngine::releaseResources()
{
    PERFETTO_FUNCTION();
    for (auto& track : loopTracks)
        if (track) track->releaseResources();

    sampleRate = 0.0;
    maxBlockSize = 0;
    numChannels = 0;
    numTracks = 0;
    activeTrackIndex = 0;
    nextTrackIndex = -1;
    currentState = LooperState::Idle;

    metronome->releaseResources();
    granularFreeze->releaseResources();
}

void LooperEngine::addTrack (int index)
{
    PERFETTO_FUNCTION();

    loopTracks[(size_t) index] = std::make_unique<LoopTrack>();
    loopTracks[(size_t) index]->prepareToPlay (sampleRate, maxBlockSize, numChannels);

    numTracks = static_cast<int> (loopTracks.size());
    activeTrackIndex = numTracks - 1;
}

LoopTrack* LooperEngine::getActiveTrack() const
{
    PERFETTO_FUNCTION();
    if (activeTrackIndex < 0 || activeTrackIndex >= numTracks) return nullptr;
    return loopTracks[(size_t) activeTrackIndex].get();
}

LoopTrack* LooperEngine::getTrackByIndex (int trackIndex) const
{
    PERFETTO_FUNCTION();
    if (trackIndex >= 0 && trackIndex < numTracks) return loopTracks[(size_t) trackIndex].get();
    return nullptr;
}

bool LooperEngine::trackHasContent (int index) const
{
    PERFETTO_FUNCTION();
    auto* track = getTrackByIndex (index);
    return track && track->getTrackLengthSamples() > 0;
}

void LooperEngine::switchToTrackImmediately (int trackIndex)
{
    PERFETTO_FUNCTION();
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::ActiveTrackCleared,
                                                         activeTrackIndex,
                                                         activeTrackIndex));
    activeTrackIndex = trackIndex;
    nextTrackIndex = -1;

    // transitionTo (LooperState::Stopped);
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::ActiveTrackChanged, trackIndex, trackIndex));
}

void LooperEngine::scheduleTrackSwitch (int trackIndex)
{
    PERFETTO_FUNCTION();
    setPendingAction (PendingAction::Type::SwitchTrack, trackIndex, true, currentState);
    nextTrackIndex = trackIndex;
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::PendingTrackChanged, trackIndex, trackIndex));
}

StateContext LooperEngine::createStateContext (const juce::AudioBuffer<float>& buffer)
{
    PERFETTO_FUNCTION();
    for (int i = 0; i < NUM_TRACKS; ++i)
        tracksToPlay[(size_t) i] = shouldTrackPlay (i);

    return StateContext { .track = getActiveTrack(),
                          .inputBuffer = &buffer,
                          .outputBuffer = const_cast<juce::AudioBuffer<float>*> (&buffer),
                          .numSamples = buffer.getNumSamples(),
                          .sampleRate = sampleRate,
                          .trackIndex = activeTrackIndex,
                          .wasRecording = StateConfig::isRecording (currentState),
                          .isSinglePlayMode = singlePlayMode.load(),
                          .syncMasterLength = syncMasterLength,
                          .syncMasterTrackIndex = syncMasterTrackIndex,
                          .allTracks = &loopTracks,
                          .tracksToPlay = &tracksToPlay };
}

bool LooperEngine::transitionTo (LooperState newState)
{
    PERFETTO_FUNCTION();
    auto ctx = createStateContext (juce::AudioBuffer<float> (numChannels, 0));
    return stateMachine.transition (currentState, newState, ctx);
}

void LooperEngine::record()
{
    PERFETTO_FUNCTION();

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    LooperState targetState = trackHasContent (activeTrackIndex) ? LooperState::Overdubbing : LooperState::Recording;
    transitionTo (targetState);
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::RecordingStateChanged, activeTrackIndex, true));
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::PlaybackStateChanged, activeTrackIndex, true));
}

void LooperEngine::play()
{
    PERFETTO_FUNCTION();

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    if (trackHasContent (activeTrackIndex))
    {
        transitionTo (LooperState::Playing);
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::PlaybackStateChanged, activeTrackIndex, true));
    }
}

void LooperEngine::stop()
{
    PERFETTO_FUNCTION();

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    if (StateConfig::isRecording (currentState))
    {
        transitionTo (LooperState::Playing);
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::RecordingStateChanged, activeTrackIndex, false));
        if (activeTrack->isSynced())
        {
            int recordedLength = activeTrack->getTrackLengthSamples();

            if (syncMasterLength == 0)
            {
                syncMasterLength = recordedLength;
                syncMasterTrackIndex = activeTrackIndex;
            }
        }
    }
    else if (StateConfig::isPlaying (currentState))
    {
        transitionTo (LooperState::Stopped);
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::PlaybackStateChanged, activeTrackIndex, false));
    }
    else if (StateConfig::isStopped (currentState))
    {
        // Reset all playheads to start
        for (auto& track : loopTracks)
            track->resetPlaybackPosition (currentState);

        transitionTo (LooperState::Idle);
    }
}

void LooperEngine::cancelRecording()
{
    PERFETTO_FUNCTION();

    // Schedule cancel action to be handled via state transition
    setPendingAction (PendingAction::Type::CancelRecording, activeTrackIndex, false, currentState);
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::RecordingStateChanged, activeTrackIndex, false));
}

void LooperEngine::toggleRecord() { StateConfig::isRecording (currentState) ? stop() : record(); }

void LooperEngine::togglePlay() { StateConfig::isPlaying (currentState) ? stop() : play(); }

void LooperEngine::toggleSync (int trackIndex)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track)
    {
        track->setSynced (! track->isSynced());
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::TrackSyncChanged, trackIndex, track->isSynced()));
    }
}

void LooperEngine::toggleReverse (int trackIndex)
{
    if (isTrackPlaybackForward (trackIndex))
        setTrackPlaybackDirectionBackward (trackIndex);
    else
        setTrackPlaybackDirectionForward (trackIndex);
}
void LooperEngine::toggleSolo (int trackIndex)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track) setTrackSoloed (trackIndex, ! track->isSoloed());
}
void LooperEngine::toggleMute (int trackIndex)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track) setTrackMuted (trackIndex, ! track->isMuted());
}
// void LooperEngine::toggleVolumeNormalize (int trackIndex)
// {
//     auto* track = getTrackByIndex (trackIndex);
//     if (track) track->toggleNormalizingOutput();
// }
void LooperEngine::toggleGranularFreeze()
{
    granularFreeze->toggleActiveState();
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::FreezeStateChanged, -1, granularFreeze->isEnabled()));
}
void LooperEngine::toggleKeepPitchWhenChangingSpeed (int trackIndex)
{
    bool current = getKeepPitchWhenChangingSpeed (trackIndex);
    setKeepPitchWhenChangingSpeed (trackIndex, ! current);
}
void LooperEngine::selectTrack (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return;
    if (trackIndex == activeTrackIndex) return;

    if (StateConfig::isRecording (currentState))
    {
        cancelRecording();
        return;
    }

    if (currentState == LooperState::Idle || currentState == LooperState::Stopped || ! trackHasContent (activeTrackIndex)
        || ! singlePlayMode.load())
    {
        switchToTrackImmediately (trackIndex);
        return;
    }

    scheduleTrackSwitch (trackIndex);
}

void LooperEngine::undo (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) trackIndex = activeTrackIndex;

    if (! StateConfig::allowsUndo (currentState)) return;

    auto* track = getTrackByIndex (trackIndex);

    track->undo();
}
void LooperEngine::setTrackPitch (int trackIndex, float pitch)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track) track->setPlaybackPitch (pitch);
}

void LooperEngine::redo (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) trackIndex = activeTrackIndex;

    if (! StateConfig::allowsUndo (currentState)) return;

    auto* track = getTrackByIndex (trackIndex);

    track->redo();
}

void LooperEngine::clear (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) trackIndex = activeTrackIndex;

    auto* track = getTrackByIndex (trackIndex);

    track->clear();

    if (trackIndex == syncMasterTrackIndex)
    {
        syncMasterLength = 0;
        syncMasterTrackIndex = -1;
        // find longest synced track to be new master
        for (int i = 0; i < numTracks; ++i)
        {
            auto* t = getTrackByIndex (i);
            if (t && t->isSynced() && t->getTrackLengthSamples() > syncMasterLength)
            {
                syncMasterLength = t->getTrackLengthSamples();
                syncMasterTrackIndex = i;
            }
        }
    }

    // if (trackIndex == activeTrackIndex)
    // {
    //     transitionTo (LooperState::Stopped);
    // }
}

void LooperEngine::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    PERFETTO_FUNCTION();

    handleMidiCommand (midiMessages, activeTrackIndex);

    buffer.applyGain (inputGain.load());
    inputMeter->processBuffer (buffer);

    processCommandsFromMessageBus();

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    processPendingActions();

    granularFreeze->processBlock (buffer);

    auto ctx = createStateContext (buffer);
    stateMachine.processAudio (currentState, ctx);

    if (metronome->isEnabled()) metronome->processBlock (buffer);

    buffer.applyGain (outputGain.load());
    outputMeter->processBuffer (buffer);

    // Update global engine state for transport controls
    engineStateBridge->updateFromAudioThread (StateConfig::isRecording (currentState),
                                              StateConfig::isPlaying (currentState),
                                              activeTrackIndex,
                                              nextTrackIndex,
                                              numTracks,
                                              inputMeter->getMeterContext(),
                                              outputMeter->getMeterContext());
    midiMessages.clear();
}

void LooperEngine::processCommandsFromMessageBus()
{
    PERFETTO_FUNCTION();

    EngineMessageBus::Command cmd;
    while (messageBus->popCommand (cmd))
    {
        auto it = commandHandlers.find (cmd.type);
        if (it != commandHandlers.end())
        {
            it->second (cmd);
        }
    }
}

void LooperEngine::setPendingAction (PendingAction::Type type, int trackIndex, bool waitForWrap, LooperState currentState)
{
    pendingAction.type = type;
    pendingAction.targetTrackIndex = trackIndex;
    pendingAction.waitForWrapAround = waitForWrap;
    pendingAction.previousState = currentState;

    if (type == PendingAction::Type::SwitchTrack && waitForWrap && currentState == LooperState::Playing)
    {
        transitionTo (LooperState::PendingTrackChange);
    }
}

void LooperEngine::processPendingActions()
{
    if (! pendingAction.isActive()) return;

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    if (pendingAction.waitForWrapAround && ! activeTrack->hasWrappedAround())
    {
        return;
    }

    switch (pendingAction.type)
    {
        case PendingAction::Type::SwitchTrack:
            if (pendingAction.targetTrackIndex >= 0 && pendingAction.targetTrackIndex < numTracks
                && pendingAction.targetTrackIndex != activeTrackIndex)
            {
                transitionTo (LooperState::Transitioning);
                switchToTrackImmediately (pendingAction.targetTrackIndex);

                transitionTo (pendingAction.previousState);
                // auto* newTrack = getActiveTrack();
                // if (newTrack && newTrack->getTrackLengthSamples() > 0)
                // {
                //     transitionTo (LooperState::Playing);
                // }
                // else
                // {
                //     transitionTo (LooperState::Stopped);
                // }
            }
            break;

        case PendingAction::Type::CancelRecording:
            transitionTo (LooperState::Idle);
            if (pendingAction.targetTrackIndex >= 0 && pendingAction.targetTrackIndex < numTracks)
            {
                activeTrackIndex = pendingAction.targetTrackIndex;
                nextTrackIndex = -1;
            }
            break;

        case PendingAction::Type::None:
        default:
            break;
    }

    pendingAction.clear();
}

// Track control implementations (delegating to tracks)
void LooperEngine::setExistingGainForTrack (int trackIndex, double oldGain)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) trackIndex = activeTrackIndex;
    auto* track = getTrackByIndex (trackIndex);
    if (track)
    {
        track->setOverdubGainOld (oldGain);
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::OldOverdubGainLevels,
                                                             trackIndex,
                                                             (float) oldGain));
    }
}
void LooperEngine::setNewOverdubGainForTrack (int trackIndex, double newGain)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) trackIndex = activeTrackIndex;
    auto* track = getTrackByIndex (trackIndex);
    if (track)
    {
        track->setOverdubGainNew (newGain);
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::NewOverdubGainLevels,
                                                             trackIndex,
                                                             (float) newGain));
    }
}

void LooperEngine::loadBackingTrackToTrack (const juce::AudioBuffer<float>& backingTrack, int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) trackIndex = activeTrackIndex;
    selectTrack (trackIndex);
    auto* track = getTrackByIndex (trackIndex);
    if (track)
    {
        track->loadBackingTrack (backingTrack, syncMasterLength);

        play();
    }
}

void LooperEngine::loadWaveFileToTrack (const juce::File& audioFile, int trackIndex)
{
    PERFETTO_FUNCTION();
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (audioFile));
    if (reader)
    {
        juce::AudioBuffer<float> backingTrack ((int) reader->numChannels, (int) reader->lengthInSamples);
        int samplesToRead = (int) reader->lengthInSamples;
        auto track = getTrackByIndex (trackIndex);
        if (track && track->isSynced())
        {
            if (syncMasterLength > 0)
            {
                samplesToRead = syncMasterLength;
            }
            else
            {
                syncMasterLength = samplesToRead;
                syncMasterTrackIndex = trackIndex;
            }
        }

        reader->read (&backingTrack, 0, samplesToRead, 0, true, true);
        loadBackingTrackToTrack (backingTrack, trackIndex);
    }
}

void LooperEngine::setTrackPlaybackSpeed (int trackIndex, float speed)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track)
    {
        track->setPlaybackSpeed (speed);
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::TrackSpeedChanged, trackIndex, speed));
    }
}

void LooperEngine::setTrackPlaybackDirectionForward (int trackIndex)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track)
    {
        track->setPlaybackDirectionForward();
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::TrackReverseDirection, trackIndex, false));
    }
}

void LooperEngine::setTrackPlaybackDirectionBackward (int trackIndex)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track)
    {
        track->setPlaybackDirectionBackward();
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::TrackReverseDirection, trackIndex, true));
    }
}

float LooperEngine::getTrackPlaybackSpeed (int trackIndex) const
{
    auto* track = const_cast<LooperEngine*> (this)->getTrackByIndex (trackIndex);
    return track ? track->getPlaybackSpeed() : 1.0f;
}

bool LooperEngine::isTrackPlaybackForward (int trackIndex) const
{
    auto* track = const_cast<LooperEngine*> (this)->getTrackByIndex (trackIndex);
    return track ? track->isPlaybackDirectionForward() : true;
}

void LooperEngine::setTrackVolume (int trackIndex, float volume)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track)
    {
        track->setTrackVolume (volume);
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::TrackVolumeChanged, trackIndex, volume));
    }
}

void LooperEngine::setTrackMuted (int trackIndex, bool muted)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track)
    {
        track->setMuted (muted);
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::TrackMuteChanged, trackIndex, muted));
    }
}

void LooperEngine::setTrackSoloed (int trackIndex, bool soloed)
{
    PERFETTO_FUNCTION();

    for (size_t i = 0; i < (size_t) numTracks; ++i)
    {
        if ((int) i == trackIndex)
        {
            loopTracks[i]->setSoloed (soloed);
        }
        else if (soloed)
        {
            loopTracks[i]->setMuted (true);
        }
        else
        {
            loopTracks[i]->setSoloed (false);
            loopTracks[i]->setMuted (false);
        }
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::TrackSoloChanged,
                                                             (int) i,
                                                             loopTracks[i]->isSoloed()));
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::TrackMuteChanged,
                                                             (int) i,
                                                             loopTracks[i]->isMuted()));
    }
}

float LooperEngine::getTrackVolume (int trackIndex) const
{
    auto* track = const_cast<LooperEngine*> (this)->getTrackByIndex (trackIndex);
    return track ? track->getTrackVolume() : 1.0f;
}

bool LooperEngine::isTrackMuted (int trackIndex) const
{
    auto* track = const_cast<LooperEngine*> (this)->getTrackByIndex (trackIndex);
    return track ? track->isMuted() : false;
}

void LooperEngine::setKeepPitchWhenChangingSpeed (int trackIndex, bool shouldKeepPitch)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track)
    {
        track->setKeepPitchWhenChangingSpeed (shouldKeepPitch);
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::TrackPitchLockChanged,
                                                             trackIndex,
                                                             shouldKeepPitch));
    }
}

bool LooperEngine::getKeepPitchWhenChangingSpeed (int trackIndex) const
{
    auto* track = const_cast<LooperEngine*> (this)->getTrackByIndex (trackIndex);
    return track ? track->shouldKeepPitchWhenChangingSpeed() : false;
}

void LooperEngine::handleMidiCommand (const juce::MidiBuffer& midiMessages, int trackIndex)
{
    PERFETTO_FUNCTION();
    if (midiMessages.getNumEvents() == 0) return;

    static int learningSessionId = 0;
    for (auto midi : midiMessages)
    {
        auto m = midi.getMessage();
        if (! m.isController() && ! m.isNoteOn()) continue;

        if (midiMappingManager->isLearning())
        {
            if (midiMappingManager->processMidiLearn (m))
                messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::MidiMappingChanged,
                                                                     -1,
                                                                     learningSessionId++));
            continue;
        }
        EngineMessageBus::CommandType commandId;
        EngineMessageBus::CommandPayload payload;
        int targetTrack = trackIndex < 0 ? activeTrackIndex : trackIndex;

        if (m.isController())
        {
            commandId = midiMappingManager->getControlChangeId (m.getControllerNumber());
            int value = m.getControllerValue();
            payload = convertCCToCommand (commandId, value, targetTrack);
        }

        if (m.isNoteOn())
        {
            uint8_t note = (uint8_t) m.getNoteNumber();
            commandId = midiMappingManager->getCommandForNoteOn (note);
            payload = std::monostate {}; // TODO
        }
        messageBus->pushCommand ({ commandId, targetTrack, payload });
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::MidiActivityReceived, targetTrack, m));
    }
}

EngineMessageBus::CommandPayload
    LooperEngine::convertCCToCommand (const EngineMessageBus::CommandType ccId, const int value, int& trackIndex)
{
    EngineMessageBus::CommandPayload payload;

    switch (ccId)
    {
        case EngineMessageBus::CommandType::SelectTrack:
        {
            trackIndex = juce::jlimit (0, numTracks - 1, value % numTracks);
            payload = std::monostate {};
        }
        break;

        case EngineMessageBus::CommandType::SetVolume:
        {
            payload = (float) value / 127.0f;
        }
        break;

        case EngineMessageBus::CommandType::SetPlaybackSpeed:
        {
            float normalizedValue = (float) value / 127.0f;
            payload = 0.5f + (normalizedValue * 1.5f); // Maps 0-127 to 0.5-2.0
        }
        break;

        case EngineMessageBus::CommandType::SetNewOverdubGain:
        {
            float normalizedValue = (float) value / 127.0f;
            payload = juce::jmap (normalizedValue, 0.0f, 2.0f); // Maps 0-127 to 0.0-2.0
        }
        break;

        case EngineMessageBus::CommandType::SetExistingAudioGain:
        {
            float normalizedValue = (float) value / 127.0f;
            payload = juce::jmap (normalizedValue, 0.0f, 2.0f); // Maps 0-127 to 0.0-2.0
        }
        break;

        case EngineMessageBus::CommandType::SetPlaybackPitch:
        {
            payload = juce::jmap ((float) value, 0.0f, 127.0f, -2.0f, 2.0f); // Maps 0-127 to -2 to +2 semitones
        }
        break;

        case EngineMessageBus::CommandType::SetMetronomeBPM:
        {
            payload = juce::jmap ((float) value, 40.0f, 240.0f); // Maps 0-127 to 40-240 BPM
            trackIndex = -1;                                     // Global
        }
        break;
        case EngineMessageBus::CommandType::SetMetronomeVolume:
        {
            payload = (float) value / 127.0f;
            trackIndex = -1; // Global
        }
        break;
        case EngineMessageBus::CommandType::SetInputGain:
        {
            payload = (float) value / 127.0f;
            trackIndex = -1; // Global
        }
        break;
        case EngineMessageBus::CommandType::SetOutputGain:
        {
            payload = (float) value / 127.0f;
            trackIndex = -1; // Global
        }
        break;

        default:
            return std::monostate {};
    }
    return payload;
}

bool LooperEngine::shouldTrackPlay (int trackIndex) const
{
    if (singlePlayMode)
    {
        return trackIndex == activeTrackIndex && trackHasContent (activeTrackIndex); // && ! isTrackMuted (trackIndex);
    }
    auto* track = getTrackByIndex (trackIndex);
    if (! track || track->getTrackLengthSamples() == 0) return false;
    // if (track->isMuted()) return false;
    return true;
}

void LooperEngine::toggleSinglePlayMode()
{
    singlePlayMode = ! singlePlayMode.load();
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::SinglePlayModeChanged, -1, singlePlayMode.load()));
}

void LooperEngine::toggleMetronomeEnabled()
{
    bool enable = ! metronome->isEnabled();
    metronome->setEnabled (enable);
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::MetronomeEnabledChanged, -1, enable));
}

void LooperEngine::setMetronomeBpm (int bpm)
{
    metronome->setBpm (bpm);
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::MetronomeBPMChanged, -1, bpm));
}

void LooperEngine::setMetronomeTimeSignature (int numerator, int denominator)
{
    metronome->setTimeSignature (numerator, denominator);
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::MetronomeTimeSignatureChanged,
                                                         -1,
                                                         std::make_pair (numerator, denominator)));
}

void LooperEngine::setMetronomeStrongBeat (int beatIndex, bool isStrong)
{
    if (isStrong)
    {
        metronome->setStrongBeat (beatIndex, isStrong);
    }
    else
    {
        metronome->disableStrongBeat();
        beatIndex = 0;
    }

    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::MetronomeStrongBeatChanged, -1, beatIndex));
}

void LooperEngine::setMetronomeVolume (float volume) { metronome->setVolume (volume); }
void LooperEngine::setPlayheadPosition (int trackIndex, int positionSamples)
{
    PERFETTO_FUNCTION();
    if (singlePlayMode.load())
    {
        auto* track = getTrackByIndex (trackIndex);
        if (track) track->setReadPosition (positionSamples);
    }
    else
    {
        for (int i = 0; i < numTracks; ++i)
        {
            auto* track = getTrackByIndex (i);
            if (track && track->isSynced()) track->setReadPosition (positionSamples);
        }
    }
}

void LooperEngine::saveTrackToFile (int trackIndex, const juce::File& audioFile)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) trackIndex = activeTrackIndex;
    auto* track = getTrackByIndex (trackIndex);
    if (track)
    {
        track->saveTrackToWavFile (audioFile);
    }
}

void LooperEngine::saveAllTracksToFolder (const juce::File& folder)
{
    PERFETTO_FUNCTION();
    for (int i = 0; i < numTracks; ++i)
    {
        if (! trackHasContent (i)) continue;
        juce::File trackFile = folder.getChildFile ("Track_" + juce::String (i + 1) + ".wav");
        saveTrackToFile (i, trackFile);
    }
}

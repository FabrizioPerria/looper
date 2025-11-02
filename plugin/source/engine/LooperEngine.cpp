#include "LooperEngine.h"
#include "engine/LooperStateConfig.h"
#include "engine/MidiCommandConfig.h"
#include "engine/MidiCommandDispatcher.h"
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

#include <algorithm>

LooperEngine::LooperEngine() {}

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

    metronome->prepareToPlay (sampleRate, maxBlockSize);

    inputMeter->prepare (numChannels);
    outputMeter->prepare (numChannels);

    setPendingAction (PendingAction::Type::SwitchTrack, 0, false);
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
    currentState = LooperState::Idle;

    metronome->releaseResources();
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

AudioToUIBridge* LooperEngine::getUIBridgeByIndex (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex >= 0 && trackIndex < numTracks) return uiBridges[(size_t) trackIndex].get();
    return nullptr;
}

bool LooperEngine::trackHasContent() const
{
    PERFETTO_FUNCTION();
    auto* track = getActiveTrack();
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

    transitionTo (LooperState::Stopped);
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::ActiveTrackChanged, trackIndex, trackIndex));
}

void LooperEngine::scheduleTrackSwitch (int trackIndex)
{
    PERFETTO_FUNCTION();
    setPendingAction (PendingAction::Type::SwitchTrack, trackIndex, true);
    nextTrackIndex = trackIndex;
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::PendingTrackChanged, trackIndex, trackIndex));
}

StateContext LooperEngine::createStateContext (const juce::AudioBuffer<float>& buffer)
{
    PERFETTO_FUNCTION();
    return StateContext { .track = getActiveTrack(),
                          .bridge = getUIBridgeByIndex (activeTrackIndex),
                          .inputBuffer = &buffer,
                          .outputBuffer = const_cast<juce::AudioBuffer<float>*> (&buffer),
                          .numSamples = buffer.getNumSamples(),
                          .sampleRate = sampleRate,
                          .trackIndex = activeTrackIndex,
                          .bridgeInitialized = bridgeInitialized[(size_t) activeTrackIndex],
                          .allTracks = &loopTracks };
}

int LooperEngine::calculateLengthToShow (const LoopTrack* track, bool isRecording) const
{
    PERFETTO_FUNCTION();
    if (! track) return 0;

    int length = track->getTrackLengthSamples();

    if (length == 0 && isRecording)
    {
        length = std::min (track->getCurrentWritePosition() + 200, track->getAvailableTrackSizeSamples());
    }

    return length;
}

void LooperEngine::updateUIBridge (StateContext& ctx, bool wasRecording)
{
    PERFETTO_FUNCTION();
    if (! ctx.track || ! ctx.bridge) return;

    bool nowRecording = StateConfig::isRecording (currentState);

    // Initialize bridge if needed
    if (! (ctx.bridgeInitialized) && ctx.track->getTrackLengthSamples() > 0)
    {
        ctx.bridge->signalWaveformChanged();
        ctx.bridgeInitialized = true;
    }

    // Handle recording finalization
    if (wasRecording && ! nowRecording)
    {
        ctx.bridge->signalWaveformChanged();
        ctx.bridge->resetRecordingCounter();
    }

    // Periodic updates during recording
    if (nowRecording && ctx.bridge->shouldUpdateWhileRecording (ctx.numSamples, ctx.sampleRate))
    {
        ctx.bridge->signalWaveformChanged();
    }

    // Update bridge state
    int lengthToShow = calculateLengthToShow (ctx.track, nowRecording);
    bool shouldShowPlaying = StateConfig::isPlaying (currentState);

    ctx.bridge->updateFromAudioThread (ctx.track->getAudioBuffer(),
                                       lengthToShow,
                                       ctx.track->getCurrentReadPosition(),
                                       nowRecording,
                                       shouldShowPlaying);
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

    LooperState targetState = trackHasContent() ? LooperState::Overdubbing : LooperState::Recording;
    transitionTo (targetState);
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::RecordingStateChanged, activeTrackIndex, true));
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::PlaybackStateChanged, activeTrackIndex, true));
}

void LooperEngine::play()
{
    PERFETTO_FUNCTION();

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    if (trackHasContent())
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
    }
    else if (StateConfig::isPlaying (currentState))
    {
        transitionTo (LooperState::Stopped);
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::PlaybackStateChanged, activeTrackIndex, false));
    }
}

void LooperEngine::cancelRecording()
{
    PERFETTO_FUNCTION();

    // Schedule cancel action to be handled via state transition
    setPendingAction (PendingAction::Type::CancelRecording, activeTrackIndex, false);
    messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::RecordingStateChanged, activeTrackIndex, false));
}

void LooperEngine::toggleRecord() { StateConfig::isRecording (currentState) ? stop() : record(); }

void LooperEngine::togglePlay() { StateConfig::isPlaying (currentState) ? stop() : play(); }

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
void LooperEngine::toggleVolumeNormalize (int trackIndex)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track) track->toggleNormalizingOutput();
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

    if (currentState == LooperState::Idle || currentState == LooperState::Stopped || ! trackHasContent())
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
    auto* bridge = getUIBridgeByIndex (trackIndex);
    if (! track || ! bridge) return;

    if (track->undo())
    {
        bridge->signalWaveformChanged();
    }
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
    auto* bridge = getUIBridgeByIndex (trackIndex);
    if (! track || ! bridge) return;

    if (track->redo())
    {
        bridge->signalWaveformChanged();
    }
}

void LooperEngine::clear (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) trackIndex = activeTrackIndex;

    auto* track = getTrackByIndex (trackIndex);
    auto* bridge = getUIBridgeByIndex (trackIndex);
    if (! track || ! bridge) return;

    track->clear();
    bridge->clear();
    bridgeInitialized[(size_t) trackIndex] = false;
    bridge->signalWaveformChanged();

    if (trackIndex == activeTrackIndex)
    {
        transitionTo (LooperState::Stopped);
    }
}

void LooperEngine::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    PERFETTO_FUNCTION();

    buffer.applyGain (inputGain.load());
    inputMeter->processBuffer (buffer);

    processCommandsFromMessageBus();

    juce::MidiBuffer outBuffer;
    midiMessages.addEvents (outBuffer, 0, buffer.getNumSamples(), 0);

    handleMidiCommand (midiMessages);
    auto* activeTrack = getActiveTrack();
    auto* bridge = getUIBridgeByIndex (activeTrackIndex);
    if (! activeTrack || ! bridge) return;

    bool wasRecording = StateConfig::isRecording (currentState);

    processPendingActions();
    auto ctx = createStateContext (buffer);
    stateMachine.processAudio (currentState, ctx);

    updateUIBridge (ctx, wasRecording);

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

void LooperEngine::setPendingAction (PendingAction::Type type, int trackIndex, bool waitForWrap)
{
    pendingAction.type = type;
    pendingAction.targetTrackIndex = trackIndex;
    pendingAction.waitForWrapAround = waitForWrap;

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

void LooperEngine::removeTrack (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (activeTrackIndex == trackIndex || trackIndex < 0 || trackIndex >= numTracks) return;

    loopTracks.erase (loopTracks.begin() + trackIndex);
    uiBridges.erase (uiBridges.begin() + trackIndex);
    bridgeInitialized.erase (bridgeInitialized.begin() + trackIndex);

    numTracks = static_cast<int> (loopTracks.size());
    if (activeTrackIndex >= numTracks) activeTrackIndex = numTracks - 1;
}

// Track control implementations (delegating to tracks)
void LooperEngine::setOverdubGainsForTrack (int trackIndex, double oldGain, double newGain)
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
        track->setOverdubGainOld (oldGain);
        messageBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::OldOverdubGainLevels,
                                                             trackIndex,
                                                             (float) oldGain));
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
        track->loadBackingTrack (backingTrack);
        auto context = createStateContext (backingTrack);
        updateUIBridge (context, false);

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
        reader->read (&backingTrack, 0, (int) reader->lengthInSamples, 0, true, true);
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

void LooperEngine::handleMidiCommand (const juce::MidiBuffer& midiMessages)
{
    PERFETTO_FUNCTION();
    if (midiMessages.getNumEvents() == 0) return;

    int targetTrack = activeTrackIndex;

    for (auto midi : midiMessages)
    {
        auto m = midi.getMessage();

        // Handle CC messages for continuous controls
        if (m.isController())
        {
            uint8_t cc = (uint8_t) m.getControllerNumber();
            uint8_t value = (uint8_t) m.getControllerValue();

            MidiControlChangeId ccId = MidiControlChangeMapping::getControlChangeId (cc);
            if (ccId != MidiControlChangeId::None)
            {
                MidiCommandDispatcher::dispatch (ccId, *this, targetTrack, value);
                continue;
            }
        }

        // Handle note messages via constexpr lookup table
        if (m.isNoteOn() || m.isNoteOff())
        {
            uint8_t note = (uint8_t) m.getNoteNumber();

            // O(1) lookup in compile-time table!
            MidiCommandId commandId = m.isNoteOn() ? MidiCommandMapping::getCommandForNoteOn (note)
                                                   : MidiCommandMapping::getCommandForNoteOff (note);

            if (commandId != MidiCommandId::None)
            {
                // Check if command can run during recording
                if (StateConfig::isRecording (currentState) && ! MidiCommandMapping::canRunDuringRecording (commandId))
                {
                    continue;
                }

                int trackIndex = MidiCommandMapping::needsTrackIndex (commandId) ? targetTrack : -1;

                MidiCommandDispatcher::dispatch (commandId, *this, trackIndex);
            }
        }
    }
}
bool LooperEngine::shouldTrackPlay (int trackIndex) const
{
    auto* track = getTrackByIndex (trackIndex);
    if (! track || track->getTrackLengthSamples() == 0) return false;
    if (track->isMuted()) return false;
    return true;
}

void LooperEngine::enableMetronome (bool enable)
{
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

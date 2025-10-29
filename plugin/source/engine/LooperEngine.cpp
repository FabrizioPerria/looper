#include "LooperEngine.h"
#include "engine/LooperStateConfig.h"
#include "engine/MidiCommandConfig.h"
#include "engine/MidiCommandDispatcher.h"
#include "profiler/PerfettoProfiler.h"
#include <algorithm>

LooperEngine::LooperEngine() { startTimerHz (30); }
LooperEngine::~LooperEngine()
{
    releaseResources();
    stopTimer();
}

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
    if (trackIndex >= 0 && trackIndex < numTracks) return loopTracks[(size_t) trackIndex].get();
    return nullptr;
}

AudioToUIBridge* LooperEngine::getUIBridgeByIndex (int trackIndex)
{
    if (trackIndex >= 0 && trackIndex < numTracks) return uiBridges[(size_t) trackIndex].get();
    return nullptr;
}

// Helper implementations
bool LooperEngine::trackHasContent() const
{
    auto* track = getActiveTrack();
    return track && track->getTrackLengthSamples() > 0;
}

void LooperEngine::switchToTrackImmediately (int trackIndex)
{
    activeTrackIndex = trackIndex;
    nextTrackIndex = -1;

    transitionTo (LooperState::Stopped);
}

void LooperEngine::scheduleTrackSwitch (int trackIndex)
{
    setPendingAction (PendingAction::Type::SwitchTrack, trackIndex, true);
    nextTrackIndex = trackIndex;
}

void LooperEngine::scheduleFinalizeRecording (int trackIndex)
{
    setPendingAction (PendingAction::Type::FinalizeRecording, trackIndex, false);
}

StateContext LooperEngine::createStateContext (const juce::AudioBuffer<float>& buffer)
{
    return StateContext { .track = getActiveTrack(),
                          .bridge = getUIBridgeByIndex (activeTrackIndex),
                          .inputBuffer = &buffer,
                          .outputBuffer = const_cast<juce::AudioBuffer<float>*> (&buffer),
                          .numSamples = buffer.getNumSamples(),
                          .sampleRate = sampleRate,
                          .trackIndex = activeTrackIndex,
                          .bridgeInitialized = bridgeInitialized[(size_t) activeTrackIndex] };
}

int LooperEngine::calculateLengthToShow (const LoopTrack* track, bool isRecording) const
{
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

// State transition
bool LooperEngine::transitionTo (LooperState newState)
{
    auto ctx = createStateContext (juce::AudioBuffer<float> (numChannels, 0));
    return stateMachine.transition (currentState, newState, ctx);
}

// User control methods
void LooperEngine::record()
{
    PERFETTO_FUNCTION();

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    LooperState targetState = trackHasContent() ? LooperState::Overdubbing : LooperState::Recording;
    transitionTo (targetState);
}

void LooperEngine::play()
{
    PERFETTO_FUNCTION();

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    if (trackHasContent())
    {
        transitionTo (LooperState::Playing);
    }
}

void LooperEngine::stop()
{
    PERFETTO_FUNCTION();

    auto* activeTrack = getActiveTrack();
    if (! activeTrack) return;

    // CRITICAL FIX: When stopping during recording, transition to appropriate state
    // The state machine exit handler will finalize the recording
    if (StateConfig::isRecording (currentState))
    {
        // Determine target state based on whether we have content
        // if (trackHasContent() || activeTrack->isCurrentlyRecording())
        transitionTo (LooperState::Playing);
        // else
        //     transitionTo (LooperState::Idle);
    }
    else if (StateConfig::isPlaying (currentState))
        transitionTo (LooperState::Stopped);
}

void LooperEngine::toggleRecord() { isRecording() ? stop() : record(); }
void LooperEngine::togglePlay() { isPlaying() ? stop() : play(); }

void LooperEngine::selectTrack (int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks) return;
    if (trackIndex == activeTrackIndex) return;

    // CRITICAL FIX: Don't directly cancel recording - let state machine handle it
    if (StateConfig::isRecording (currentState))
    {
        // Schedule cancel action to be handled via state transition
        setPendingAction (PendingAction::Type::CancelRecording, trackIndex, false);
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

        if (trackIndex == activeTrackIndex && track->getTrackLengthSamples() == 0)
        {
            transitionTo (LooperState::Idle);
        }
    }
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

        if (trackIndex == activeTrackIndex && currentState == LooperState::Idle && track->getTrackLengthSamples() > 0)
        {
            transitionTo (LooperState::Stopped);
        }
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

void LooperEngine::processBlock (const juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    PERFETTO_FUNCTION();

    juce::MidiBuffer outBuffer;
    uiToEngineBridge->fetchNextMidiBuffer (outBuffer);
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

    // Update global engine state for transport controls
    engineStateBridge->updateFromAudioThread (StateConfig::isRecording (currentState),
                                              StateConfig::isPlaying (currentState),
                                              activeTrackIndex,
                                              nextTrackIndex,
                                              numTracks);
}

int LooperEngine::getPendingTrackIndex() const
{
    return (pendingAction.type == PendingAction::Type::SwitchTrack) ? pendingAction.targetTrackIndex : -1;
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
                activeTrackIndex = pendingAction.targetTrackIndex;
                nextTrackIndex = -1;

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
            // CRITICAL FIX: Cancel recording via state transition
            // First transition to Idle (which calls exit handler to cancel)
            transitionTo (LooperState::Idle);
            // Then switch to the target track
            if (pendingAction.targetTrackIndex >= 0 && pendingAction.targetTrackIndex < numTracks)
            {
                activeTrackIndex = pendingAction.targetTrackIndex;
                nextTrackIndex = -1;
            }
            break;

        case PendingAction::Type::FinalizeRecording:
            // CRITICAL FIX: Finalize via state transition
            // The exit handler of Recording/Overdubbing states will finalize
            transitionTo (LooperState::Playing);
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
        track->setOverdubGainOld (oldGain);
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
    if (track) track->setPlaybackSpeed (speed);
}

void LooperEngine::setTrackPlaybackDirectionForward (int trackIndex)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track) track->setPlaybackDirectionForward();
}

void LooperEngine::setTrackPlaybackDirectionBackward (int trackIndex)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track) track->setPlaybackDirectionBackward();
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
    if (track) track->setTrackVolume (volume);
}

void LooperEngine::setTrackMuted (int trackIndex, bool muted)
{
    auto* track = getTrackByIndex (trackIndex);
    if (track) track->setMuted (muted);
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
    if (track) track->setKeepPitchWhenChangingSpeed (shouldKeepPitch);
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

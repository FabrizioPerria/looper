#include "LooperEngine.h"

LooperEngine::LooperEngine() : transportState (TransportState::Stopped), sampleRate (0.0), maxBlockSize (0), numChannels (0), numTracks (0)
{
    loopTracks = std::vector<std::unique_ptr<LoopTrack>>();
    midiCommandMap = std::unordered_map<MidiKey, std::function<void (LooperEngine&)>, MidiKeyHash>();
}

LooperEngine::~LooperEngine()
{
    releaseResources();
}

void LooperEngine::prepareToPlay (double newSampleRate, int newMaxBlockSize, int newNumTracks, int newNumChannels)
{
    PERFETTO_FUNCTION();
    if (newSampleRate <= 0.0 || newMaxBlockSize <= 0 || newNumChannels <= 0 || newNumTracks <= 0)
    {
        return;
    }
    releaseResources();
    sampleRate = newSampleRate;
    maxBlockSize = newMaxBlockSize;
    numChannels = newNumChannels;

    for (int i = 0; i < newNumTracks; ++i)
    {
        addTrack();
    }

    setupMidiCommands();
}

void LooperEngine::releaseResources()
{
    PERFETTO_FUNCTION();
    for (auto& track : loopTracks)
    {
        track->releaseResources();
    }
    loopTracks.clear();
    sampleRate = 0.0;
    maxBlockSize = 0;
    numChannels = 0;
    numTracks = 0;
    activeTrackIndex = 0;
    transportState = TransportState::Stopped;
}

void LooperEngine::selectTrack (const int trackIndex)
{
    PERFETTO_FUNCTION();
    if (trackIndex >= 0 && trackIndex < numTracks)
    {
        activeTrackIndex = trackIndex;
    }
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
        loopTracks[activeTrackIndex]->finalizeLayer();
        transportState = TransportState::Playing;
        return;
    }
    transportState = TransportState::Stopped;
}

void LooperEngine::addTrack()
{
    PERFETTO_FUNCTION();
    auto track = std::make_unique<LoopTrack>();
    track->prepareToPlay (sampleRate, maxBlockSize, numChannels);
    loopTracks.push_back (std::move (track));
    numTracks = static_cast<int> (loopTracks.size());
    activeTrackIndex = numTracks - 1;
}

void LooperEngine::removeTrack (const int trackIndex)
{
    PERFETTO_FUNCTION();
    if (activeTrackIndex == trackIndex)
    {
        return;
    }
    if (trackIndex >= 0 && trackIndex < numTracks)
    {
        loopTracks.erase (loopTracks.begin() + trackIndex);
        numTracks = static_cast<int> (loopTracks.size());
        if (activeTrackIndex >= numTracks)
        {
            activeTrackIndex = numTracks - 1;
        }
    }
}

void LooperEngine::undo()
{
    PERFETTO_FUNCTION();
    loopTracks[activeTrackIndex]->undo();
    waveformDirty = true; // Mark dirty when undoing
}

void LooperEngine::redo()
{
    PERFETTO_FUNCTION();
    loopTracks[activeTrackIndex]->redo();
    waveformDirty = true; // Mark dirty when redoing
}

void LooperEngine::clear()
{
    PERFETTO_FUNCTION();
    loopTracks[activeTrackIndex]->clear();
    transportState = TransportState::Stopped;
    waveformDirty = true; // Mark dirty when clearing
}

void LooperEngine::setupMidiCommands()
{
    PERFETTO_FUNCTION();
    const bool NOTE_ON = true;
    const bool NOTE_OFF = false;
    midiCommandMap[{ 60, NOTE_ON }] = [] (LooperEngine& engine) { engine.startRecording(); }; // C3
    midiCommandMap[{ 62, NOTE_ON }] = [] (LooperEngine& engine)                               // D3
    {
        if (engine.getTransportState() == TransportState::Stopped)
        {
            engine.startPlaying();
        }
        else
        {
            engine.stop();
        }
    };
    midiCommandMap[{ 72, NOTE_ON }] = [] (LooperEngine& engine) { engine.undo(); };   // C4
    midiCommandMap[{ 74, NOTE_ON }] = [] (LooperEngine& engine) { engine.redo(); };   // D4
    midiCommandMap[{ 84, NOTE_OFF }] = [] (LooperEngine& engine) { engine.clear(); }; // C5
    juce::File defaultFile = juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("backing.wav");
    midiCommandMap[{ 86, NOTE_ON }] = [defaultFile] (LooperEngine& engine) { engine.loadWaveFileToActiveTrack (defaultFile); }; // D5
}

void LooperEngine::handleMidiCommand (const juce::MidiBuffer& midiMessages)
{
    PERFETTO_FUNCTION();
    if (midiMessages.getNumEvents() > 0)
    {
        juce::MidiMessage m;
        for (auto midi : midiMessages)
        {
            m = midi.getMessage();
            auto it = midiCommandMap.find ({ m.getNoteNumber(), m.isNoteOn() });
            if (it != midiCommandMap.end())
            {
                it->second (*this);
            }
        }
    }
}

void LooperEngine::processBlock (const juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    PERFETTO_FUNCTION();
    handleMidiCommand (midiMessages);

    auto activeTrack = getActiveTrack();
    if (! activeTrack)
    {
        return;
    }

    bool wasRecording = activeTrack->isCurrentlyRecording();
    switch (transportState)
    {
        case TransportState::Recording:
            activeTrack->processRecord (buffer, buffer.getNumSamples());
            waveformDirty = true; // Mark dirty when recording
        case TransportState::Playing:
            activeTrack->processPlayback (const_cast<juce::AudioBuffer<float>&> (buffer), buffer.getNumSamples());
            break;
        case TransportState::Stopped:
            // do nothing
            break;
        default:
            break;
    }

    if (uiBridge && activeTrack)
    {
        bool nowRecording = activeTrack->isCurrentlyRecording();

        // On first call with bridge connected, always send initial snapshot
        if (! bridgeInitialized && activeTrack->getLength() > 0)
        {
            waveformDirty = true;
            bridgeInitialized = true;
        }

        // Detect finalize event
        if (wasRecording && ! nowRecording)
        {
            waveformDirty = true;
            recordingUpdateCounter = 0;
        }

        // Update waveform periodically while recording (every ~100ms)
        if (nowRecording)
        {
            recordingUpdateCounter++;
            int framesPerUpdate = (int) (sampleRate * 0.1 / buffer.getNumSamples());
            if (recordingUpdateCounter >= framesPerUpdate)
            {
                waveformDirty = true;
                recordingUpdateCounter = 0;
            }
        }

        // Use actual buffer size during recording, finalized length otherwise
        size_t lengthToReport = activeTrack->getLength();
        if (nowRecording && lengthToReport == 0)
        {
            // First recording - use buffer allocation size
            lengthToReport = activeTrack->getAudioBuffer().getNumSamples();
        }

        uiBridge->updateFromAudioThread (&activeTrack->getAudioBuffer(),
                                         lengthToReport,
                                         activeTrack->getCurrentReadPosition(),
                                         nowRecording,
                                         transportState == TransportState::Playing,
                                         waveformDirty);

        waveformDirty = false;
    }
}

LoopTrack* LooperEngine::getActiveTrack()
{
    PERFETTO_FUNCTION();
    if (loopTracks.empty() || numTracks == 0)
    {
        return nullptr;
    }
    if (activeTrackIndex < 0 || activeTrackIndex >= numTracks)
    {
        return nullptr;
    }
    return loopTracks[activeTrackIndex].get();
}

void LooperEngine::setOverdubGainsForTrack (const int trackIndex, const double oldGain, const double newGain)
{
    PERFETTO_FUNCTION();
    if (trackIndex < 0 || trackIndex >= numTracks)
    {
        return;
    }
    auto& track = loopTracks[trackIndex];
    if (track)
    {
        track->setOverdubGains (oldGain, newGain);
    }
}

void LooperEngine::loadBackingTrackToActiveTrack (const juce::AudioBuffer<float>& backingTrack)
{
    PERFETTO_FUNCTION();
    auto activeTrack = getActiveTrack();
    if (activeTrack)
    {
        activeTrack->loadBackingTrack (backingTrack);
        startPlaying();
    }
    waveformDirty = true; // Mark dirty when loading backing track
}

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
    if (trackIndex >= 0 && trackIndex < numTracks)
    {
        activeTrackIndex = trackIndex;
    }
}

void LooperEngine::startRecording()
{
    transportState = TransportState::Recording;
}

void LooperEngine::startPlaying()
{
    transportState = TransportState::Playing;
}

void LooperEngine::stop()
{
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
    auto track = std::make_unique<LoopTrack>();
    track->prepareToPlay (sampleRate, maxBlockSize, numChannels);
    loopTracks.push_back (std::move (track));
    numTracks = static_cast<int> (loopTracks.size());
    activeTrackIndex = numTracks - 1;
}

void LooperEngine::removeTrack (const int trackIndex)
{
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
    loopTracks[activeTrackIndex]->undo();
}

void LooperEngine::redo()
{
    loopTracks[activeTrackIndex]->redo();
}

void LooperEngine::clear()
{
    loopTracks[activeTrackIndex]->clear();
    transportState = TransportState::Stopped;
}

void LooperEngine::setupMidiCommands()
{
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
    handleMidiCommand (midiMessages);

    auto activeTrack = getActiveTrack();
    if (! activeTrack)
    {
        return;
    }

    switch (transportState)
    {
        case TransportState::Recording:
            activeTrack->processRecord (buffer, buffer.getNumSamples());
        case TransportState::Playing:
            activeTrack->processPlayback (const_cast<juce::AudioBuffer<float>&> (buffer), buffer.getNumSamples());
            break;
        case TransportState::Stopped:
            // do nothing
            break;
        default:
            break;
    }
}

LoopTrack* LooperEngine::getActiveTrack()
{
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
    auto activeTrack = getActiveTrack();
    if (activeTrack)
    {
        activeTrack->loadBackingTrack (backingTrack);
        startPlaying();
    }
}

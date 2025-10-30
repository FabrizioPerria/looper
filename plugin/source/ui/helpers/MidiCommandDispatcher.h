// #pragma once
// #include "audio/UIToEngineBridge.h"
// #include "engine/LooperEngine.h"
// #include "engine/MidiCommandConfig.h"
// #include <JuceHeader.h>
//
// class MidiCommandDispatcher
// {
// public:
//     /// Sends a command to the engine using MIDI protocol.
//     /// This ensures UI commands go through the same validation,
//     /// logging, and dispatch path as external MIDI controllers.
//     MidiCommandDispatcher (LooperEngine* engine, UIToEngineBridge* bridge) : looperEngine (engine), uiToEngineBridge (bridge) {}
//
//     void sendCommandToEngine (const int noteNumber, const bool isNoteOn = true)
//
//     {
//         juce::MidiBuffer midiBuffer;
//         juce::MidiMessage msg = isNoteOn ? juce::MidiMessage::noteOn (1, noteNumber, (juce::uint8) 100)
//                                          : juce::MidiMessage::noteOff (1, noteNumber);
//
//         int trackIndex = looperEngine->getActiveTrackIndex();
//
//         midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, MidiNotes::TRACK_SELECT_CC, trackIndex), 0);
//         midiBuffer.addEvent (msg, 0);
//         uiToEngineBridge->pushMidiMessage (midiBuffer);
//     }
//     void sendCommandToEngine (const int noteNumber, const int trackIndex, const bool isNoteOn = true)
//
//     {
//         juce::MidiBuffer midiBuffer;
//         juce::MidiMessage msg = isNoteOn ? juce::MidiMessage::noteOn (1, noteNumber, (juce::uint8) 100)
//                                          : juce::MidiMessage::noteOff (1, noteNumber);
//
//         midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, MidiNotes::TRACK_SELECT_CC, trackIndex), 0);
//         midiBuffer.addEvent (msg, 0);
//         uiToEngineBridge->pushMidiMessage (midiBuffer);
//     }
//
//     void sendControlChangeToEngine (const int controllerNumber, const int trackIndex, const double value)
//     {
//         juce::MidiBuffer midiBuffer;
//
//         int ccValue = 0;
//
//         if (controllerNumber == MidiNotes::PLAYBACK_SPEED_CC)
//             ccValue = (int) (((value - 0.5) / 1.5) * 127.0);
//         else if (controllerNumber == MidiNotes::PITCH_SHIFT_CC)
//             ccValue = (int) juce::jmap (value, -2.0, 2.0, 0.0, 127.0);
//         else if (controllerNumber == MidiNotes::TRACK_SELECT_CC)
//             ccValue = (int) value;
//         else
//             ccValue = (int) std::clamp (value * 127.0, 0.0, 127.0);
//
//         midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, MidiNotes::TRACK_SELECT_CC, trackIndex), 0);
//         midiBuffer.addEvent (juce::MidiMessage::controllerEvent (1, controllerNumber, ccValue), 0);
//         uiToEngineBridge->pushMidiMessage (midiBuffer);
//     }
//
//     LooperState getCurrentState() const { return looperEngine->getState(); }
//
//     LoopTrack* getTrackByIndex (int trackIndex) const { return looperEngine->getTrackByIndex (trackIndex); }
//
//     float getCurrentVolume (int trackIndex) const { return looperEngine->getTrackVolume (trackIndex); }
//
//     bool isMuted (int trackIndex) const { return looperEngine->isTrackMuted (trackIndex); }
//
//     int getPendingTrackIndex() const { return looperEngine->getPendingTrackIndex(); }
//
//     int getActiveTrackIndex() const { return looperEngine->getActiveTrackIndex(); }
//
//     float getCurrentPitchShift (int trackIndex)
//     {
//         auto* track = getTrackByIndex (trackIndex);
//         return track ? track->getPlaybackPitch() : 0.0f;
//     }
//
// private:
//     LooperEngine* looperEngine;
//     UIToEngineBridge* uiToEngineBridge;
// };

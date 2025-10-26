#pragma once
#include "audio/EngineStateToUIBridge.h"
#include "engine/MidiCommandConfig.h"
#include "ui/colors/TokyoNight.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class AccentBar : public juce::Component
{
public:
    AccentBar (MidiCommandDispatcher* midiCommandDispatcher,
               int trackIdx,
               AudioToUIBridge* audioUIBridge,
               EngineStateToUIBridge* engineBridge)
        : engineStateBridge (engineBridge), audioBridge (audioUIBridge), midiDispatcher (midiCommandDispatcher), trackIndex (trackIdx)
    {
        setInterceptsMouseClicks (true, false);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        int activeTrackIndex, numTracks, pendingTrackIndex;
        bool isRecording, isPlaying;
        engineStateBridge->getEngineState (isRecording, isPlaying, activeTrackIndex, pendingTrackIndex, numTracks);
        bool isTrackActive = activeTrackIndex == trackIndex;
        bool isPendingTrack = pendingTrackIndex == trackIndex;

        // Choose color
        if (isPendingTrack && ! isTrackActive)
            g.setColour (LooperTheme::Colors::yellow.withAlpha (0.8f));
        else if (isTrackActive)
            g.setColour (LooperTheme::Colors::cyan.withAlpha (0.8f));
        else
            g.setColour (LooperTheme::Colors::primary.withAlpha (0.3f));

        // ACTUALLY DRAW IT!
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

        // Draw track number
        g.setColour (isTrackActive ? LooperTheme::Colors::backgroundDark : LooperTheme::Colors::cyan);
        g.setFont (LooperTheme::Fonts::getBoldFont (14.0f));
        g.drawText (juce::String (trackIndex + 1), bounds, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        midiDispatcher->sendControlChangeToEngine (MidiNotes::TRACK_SELECT_CC, trackIndex, trackIndex);
    }

    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void mouseExit (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::NormalCursor); }

private:
    EngineStateToUIBridge* engineStateBridge;
    AudioToUIBridge* audioBridge;
    MidiCommandDispatcher* midiDispatcher;
    int trackIndex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AccentBar)
};

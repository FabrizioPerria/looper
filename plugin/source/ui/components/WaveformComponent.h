#pragma once

#include "audio/AudioToUIBridge.h"
#include "audio/EngineCommandBus.h"
#include "ui/helpers/WaveformCache.h"
#include "ui/renderers/LinearRenderer.h"
#include <JuceHeader.h>

class WaveformComponent : public juce::Component, public juce::Timer, public juce::AsyncUpdater, public juce::FileDragAndDropTarget
{
public:
    WaveformComponent (int trackIdx, AudioToUIBridge* audioBridge, EngineMessageBus* engineMessageBus)
        : trackIndex (trackIdx), bridge (audioBridge), uiToEngineBus (engineMessageBus)
    {
        renderer = std::make_unique<LinearRenderer>();
        vBlankAttachment = std::make_unique<juce::VBlankAttachment> (this, [this]() { onVBlankCallback(); });
        startTimerHz (10);
    }

    ~WaveformComponent() override
    {
        stopTimer();
        cancelPendingUpdate();
        backgroundProcessor.removeAllJobs (true, 5000);
    }

    void paint (juce::Graphics& g) override;
    void timerCallback() override;
    void resized() override
    {
        if (bridge) triggerAsyncUpdate();
    }

    void clearTrack()
    {
        cache.clear();
        clearRegion();
        repaint();
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        const juce::String file = files[0]; // Just take the first file if a group is provided

        return (file.endsWithIgnoreCase (".mp3") || file.endsWithIgnoreCase (".wav") || file.endsWithIgnoreCase (".aiff")
                || file.endsWithIgnoreCase (".aif") || file.endsWithIgnoreCase (".m4a") || file.endsWithIgnoreCase (".flac"));
    }

    void filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/) override
    {
        // Load the first audio file that was dropped
        const juce::String filepath = files[0]; // Just take the first file if a group is provided
        juce::File file (filepath);

        if (isAudioFile (file))
        {
            uiToEngineBus->pushCommand ({ EngineMessageBus::CommandType::LoadAudioFile, trackIndex, file });
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isShiftDown())
        { // Shift+drag to set loop region
            isDraggingRegion = true;
            dragStartX = event.x;
            regionStartSample = xToSample (event.x);
        }
        else
        {
            int samplePos = xToSample (event.x);
            uiToEngineBus->pushCommand ({ EngineMessageBus::CommandType::SetPlayheadPosition, trackIndex, samplePos });
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (isDraggingRegion)
        {
            dragEndX = event.x;
            regionEndSample = xToSample (event.x);
            if (regionEndSample < regionStartSample) std::swap (regionStartSample, regionEndSample);
            repaint();
        }
    }

    void clearRegion()
    {
        regionStartSample = 0;
        regionEndSample = 0;
        isDraggingRegion = false;
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (isDraggingRegion)
        {
            dragEndX = event.x;
            if (dragEndX - dragStartX < 5)
            {
                clearRegion();
                uiToEngineBus->pushCommand ({ EngineMessageBus::CommandType::ClearSubLoopRegion, trackIndex, {} });
                repaint();
                return;
            }
            regionEndSample = xToSample (event.x);
            if (regionEndSample < regionStartSample) std::swap (regionStartSample, regionEndSample);
            isDraggingRegion = false;
            repaint();
            uiToEngineBus->pushCommand ({ EngineMessageBus::CommandType::SetSubLoopRegion,
                                          trackIndex,
                                          std::make_pair (regionStartSample, regionEndSample) });
        }
    }

private:
    void onVBlankCallback();
    void handleAsyncUpdate() override;

    bool isDraggingRegion = false;
    int dragStartX = 0;
    int dragEndX = 0;
    int regionStartSample = 0;
    int regionEndSample = 0;
    int xToSample (int x)
    {
        float normalized = (float) x / (float) getWidth();
        return (int) (normalized * (float) cache.getTrackLength());
    }

    int sampleToX (int sample)
    {
        float normalized = (float) sample / (float) cache.getTrackLength();
        return (int) (normalized * (float) getWidth());
    }

    int trackIndex;
    WaveformCache cache;
    std::unique_ptr<LinearRenderer> renderer;
    AudioToUIBridge* bridge = nullptr;
    EngineMessageBus* uiToEngineBus = nullptr;
    juce::ThreadPool backgroundProcessor { 1 };

    int vblankCounter = 0;
    std::unique_ptr<juce::VBlankAttachment> vBlankAttachment;

    // State tracking
    int lastReadPos = 0;
    bool lastRecording = false;
    bool lastPlaying = false;
    int lastProcessedVersion = -1;

    bool isAudioFile (const juce::File& file)
    {
        auto extension = file.getFileExtension().toLowerCase();
        return extension == ".mp3" || extension == ".wav" || extension == ".aiff" || extension == ".aif" || extension == ".m4a"
               || extension == ".flac";
    }

    juce::String formatTime (int samples, double sampleRate) const
    {
        if (sampleRate <= 0) return "00:00";

        int totalSeconds = (int) (samples / sampleRate);
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;

        return juce::String::formatted ("%02d:%02d", minutes, seconds);
    }

    void drawTimeOverlay (juce::Graphics& g, const juce::String& text, float x, float y, juce::Justification justification)
    {
        g.setColour (LooperTheme::Colors::backgroundDark.withAlpha (0.7f));

        g.setFont (LooperTheme::Fonts::getRegularFont (12.0f));

        int textWidth = GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 8;
        int textHeight = 16;

        juce::Rectangle<float> bgRect;
        if (justification == juce::Justification::topLeft)
            bgRect = juce::Rectangle<float> (x, y, (float) textWidth, (float) textHeight);
        else if (justification == juce::Justification::topRight)
            bgRect = juce::Rectangle<float> (x - textWidth, y, (float) textWidth, (float) textHeight);
        else if (justification == juce::Justification::bottomLeft)
            bgRect = juce::Rectangle<float> (x, y - textHeight, (float) textWidth, (float) textHeight);
        else if (justification == juce::Justification::bottomRight)
            bgRect = juce::Rectangle<float> (x - textWidth, y - textHeight, (float) textWidth, (float) textHeight);
        else if (justification == juce::Justification::centredTop)
            bgRect = juce::Rectangle<float> (x - textWidth / 2, y, (float) textWidth, (float) textHeight);

        g.fillRoundedRectangle (bgRect, 3.0f);

        g.setColour (LooperTheme::Colors::text);
        g.drawText (text, bgRect.toNearestInt(), juce::Justification::centred);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformComponent)
};

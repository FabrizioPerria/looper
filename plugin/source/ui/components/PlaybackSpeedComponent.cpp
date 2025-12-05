#include "PlaybackSpeedComponent.h"
#include "ui/components/ProgressiveAutomationPopup.h"
#include "ui/editor/LooperEditor.h"

void PlaybackSpeedComponent::openProgressiveSpeedPopup()
{
    if (! progressiveSpeedPopup)
    {
        ProgressiveAutomationConfig config { MIN_PLAYBACK_SPEED,           MAX_PLAYBACK_SPEED, 0.7f, 1.0f, 0.03f, "x",
                                             "Progressive Speed Practice", "End Speed" };

        auto getLoopLength = [this]()
        {
            int length, readPos;
            bool recording, playing;
            double sampleRate;
            uiBridge->getPlaybackState (length, readPos, recording, playing, sampleRate);
            return (float) length / (float) sampleRate;
        };

        progressiveSpeedPopup = std::make_unique<ProgressiveAutomationPopup> (config, currentSpeedCurve, getLoopLength);
        progressiveSpeedPopup->onStart = [this] (const ProgressiveAutomationCurve& curve)
        {
            speedMode = SpeedMode::Automation;

            applyProgressiveSpeed (curve, 0);
            closeProgressiveSpeedPopup();
        };

        progressiveSpeedPopup->onCancel = [this]() { closeProgressiveSpeedPopup(); };

        if (auto* editor = findParentComponentOfClass<LooperEditor>())
        {
            editor->addAndMakeVisible (progressiveSpeedPopup.get());
            progressiveSpeedPopup->setBounds (editor->getLocalBounds());
        }
    }
}

void PlaybackSpeedComponent::closeProgressiveSpeedPopup()
{
    if (progressiveSpeedPopup)
    {
        if (auto* editor = findParentComponentOfClass<LooperEditor>())
        {
            editor->removeChildComponent (progressiveSpeedPopup.get());
        }
        progressiveSpeedPopup.reset();
    }
}

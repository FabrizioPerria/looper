#include "PlaybackSpeedComponent.h"
#include "ProgressiveSpeedPopup.h"
#include "ui/editor/LooperEditor.h"

void PlaybackSpeedComponent::openProgressiveSpeedPopup()
{
    if (! progressiveSpeedPopup)
    {
        progressiveSpeedPopup = std::make_unique<ProgressiveSpeedPopup> (trackIndex, currentSpeedCurve, uiToEngineBus, uiBridge);

        progressiveSpeedPopup->onStart = [this] (const ProgressiveSpeedCurve& curve)
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

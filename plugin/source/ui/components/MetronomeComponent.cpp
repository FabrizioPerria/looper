#include "MetronomeComponent.h"
#include "ProgressiveMetronomePopup.h"
#include "ui/editor/LooperEditor.h"

void MetronomeComponent::openProgressiveMetronomePopup()
{
    if (! progressiveMetronomePopup)
    {
        progressiveMetronomePopup = std::make_unique<ProgressiveMetronomePopup> (currentMetronomeCurve, uiToEngineBus);

        progressiveMetronomePopup->onStart = [this] (const ProgressiveMetronomeCurve& curve)
        {
            speedMode = SpeedMode::Automation;

            applyProgressiveSpeed (curve, 0);
            closeProgressiveMetronomePopup();
        };

        progressiveMetronomePopup->onCancel = [this]() { closeProgressiveMetronomePopup(); };

        if (auto* editor = findParentComponentOfClass<LooperEditor>())
        {
            editor->addAndMakeVisible (progressiveMetronomePopup.get());
            progressiveMetronomePopup->setBounds (editor->getLocalBounds());
        }
    }
}

void MetronomeComponent::closeProgressiveMetronomePopup()
{
    if (progressiveMetronomePopup)
    {
        if (auto* editor = findParentComponentOfClass<LooperEditor>())
        {
            editor->removeChildComponent (progressiveMetronomePopup.get());
        }
        progressiveMetronomePopup.reset();
    }
}

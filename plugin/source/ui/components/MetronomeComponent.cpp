#include "MetronomeComponent.h"
#include "ui/editor/LooperEditor.h"

void MetronomeComponent::openProgressiveMetronomePopup()
{
    if (! progressiveMetronomePopup)
    {
        ProgressiveAutomationConfig config { METRONOME_MIN_BPM,
                                             METRONOME_MAX_BPM,
                                             (float) METRONOME_DEFAULT_BPM,
                                             (float) METRONOME_DEFAULT_BPM,
                                             1,
                                             " BPM",
                                             "Progressive Metronome Practice",
                                             "End Speed" };

        progressiveMetronomePopup = std::make_unique<ProgressiveAutomationPopup> (config, currentMetronomeCurve, nullptr);

        progressiveMetronomePopup->onStart = [this] (const ProgressiveAutomationCurve& curve)
        {
            speedMode = SpeedMode::Automation;

            applyProgressiveSpeed (curve, 0);
            uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleMetronomeEnabled, -1, {} });
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

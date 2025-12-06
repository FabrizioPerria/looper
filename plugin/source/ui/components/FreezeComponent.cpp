#include "ui/components/FreezeComponent.h"
#include "ui/editor/LooperEditor.h"
#include <JuceHeader.h>

void FreezeComponent::openPopup()
{
    if (! freezeParametersPopup)
    {
        freezeParametersPopup = std::make_unique<FreezeParametersPopup> (uiToEngineBus, currentFreezeParams);

        freezeParametersPopup->onApply = [this] (const FreezeParameters& params, bool shouldQuit = true)
        {
            currentFreezeParams = params;

            uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::SetFreezeParameters, -1, params });
            if (shouldQuit) closePopup();
        };

        freezeParametersPopup->onCancel = [this]() { closePopup(); };

        if (auto* editor = findParentComponentOfClass<LooperEditor>())
        {
            editor->addAndMakeVisible (freezeParametersPopup.get());
            auto b = editor->getLocalBounds();
            freezeParametersPopup->setBounds (b);
        }
    }
}

void FreezeComponent::closePopup()
{
    if (freezeParametersPopup)
    {
        if (auto* editor = findParentComponentOfClass<LooperEditor>())
        {
            editor->removeChildComponent (freezeParametersPopup.get());
        }
        freezeParametersPopup.reset();
    }
}

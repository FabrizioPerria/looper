#pragma once

#include "audio/EngineCommandBus.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class ButtonIconComponent : public juce::Component, public juce::MouseListener
{
public:
    ButtonIconComponent (EngineMessageBus* engineMessageBus, const juce::String& svgData, EngineMessageBus::CommandType command)
        : uiToEngineBus (engineMessageBus), commandType (command)
    {
        auto dataSvg = svgData.toRawUTF8();
        auto sizeSvg = svgData.getNumBytesAsUTF8();

        juce::MemoryInputStream stream (dataSvg, static_cast<size_t> (sizeSvg), false);
        svgDrawable = juce::Drawable::createFromImageDataStream (stream);
    }

    void paint (Graphics& g) override
    {
        if (! svgDrawable) return;

        auto bounds = getLocalBounds().toFloat();
        auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto drawBounds = bounds.withSizeKeepingCentre (size, size);

        auto newColour = LooperTheme::Colors::cyan.withAlpha (isEnabled ? 1.0f : 0.3f);
        svgDrawable->replaceColour (enabledColour, newColour);
        svgDrawable->drawWithin (g, drawBounds, juce::RectanglePlacement::centred, 1.0f);
        enabledColour = newColour;
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        uiToEngineBus->pushCommand (EngineMessageBus::Command { commandType, -1, {} });
    }

    void setFreezeEnabled (bool enabled)
    {
        isEnabled.store (enabled);
        repaint();
    }

private:
    std::unique_ptr<juce::Drawable> svgDrawable;
    EngineMessageBus* uiToEngineBus;
    std::atomic<bool> isEnabled { false };
    EngineMessageBus::CommandType commandType;
    juce::Colour enabledColour = juce::Colours::black;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ButtonIconComponent)
};

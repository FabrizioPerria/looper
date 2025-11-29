// ProgressiveSpeedButton.h
#pragma once
#include "audio/EngineCommandBus.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class ProgressiveSpeedButton : public juce::TextButton
{
public:
    ProgressiveSpeedButton()
    {
        setButtonText ("SPEED");
        setComponentID ("speed");
    }

    void setMode (const juce::String& modeText)
    {
        mode = modeText;
        updateButtonText();
    }

    void setActive (bool isActive)
    {
        active = isActive;
        setToggleState (isActive, juce::dontSendNotification);
        updateButtonText();
    }

private:
    juce::String mode = "FLAT";
    bool active = false;

    void updateButtonText()
    {
        if (active)
            setButtonText ("PROG");
        else
            setButtonText ("SPEED");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgressiveSpeedButton)
};

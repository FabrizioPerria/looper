#pragma once
#include <JuceHeader.h>

class DraggableToggleButtonComponent : public juce::TextButton
{
public:
    DraggableToggleButtonComponent (int maxValues = 16) : maxValueValue (maxValues), currentValue (1)
    {
        updateButtonText();
        setToggleState (true, juce::dontSendNotification);
    }

    void setMaxValue (int max)
    {
        maxValueValue = max;
        if (currentValue > maxValueValue) currentValue = maxValueValue;
        updateButtonText();
    }

    void setCurrentValue (int value)
    {
        currentValue = juce::jlimit (0, maxValueValue, value);
        setToggleState (currentValue > 0, juce::dontSendNotification);
        updateButtonText();
    }

    int getCurrentValue() const { return currentValue; }

    std::function<void (int)> onValueChanged;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            dragStartY = e.getScreenPosition().y;
            dragStartValue = currentValue;
            isDragging = false; // Will be set true if we actually drag
            wasToggled = false;
        }
        TextButton::mouseDown (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! isDragging)
        {
            // Check if we've moved enough to start dragging
            int deltaY = std::abs (dragStartY - e.getScreenPosition().y);
            if (deltaY > 3) isDragging = true;
        }

        if (isDragging)
        {
            int deltaY = dragStartY - e.getScreenPosition().y;
            int newValue = juce::jlimit (0, maxValueValue, dragStartValue + deltaY / 10);

            if (newValue != currentValue)
            {
                setCurrentValue (newValue);
                if (onValueChanged) onValueChanged (currentValue);
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! isDragging && ! wasToggled)
        {
            // Simple click - toggle on/off
            if (currentValue == 0)
                setCurrentValue (1); // Turn on to value 1
            else
                setCurrentValue (0); // Turn off

            if (onValueChanged) onValueChanged (currentValue);
        }

        isDragging = false;
        TextButton::mouseUp (e);
    }

private:
    int maxValueValue;
    int currentValue;
    int dragStartY = 0;
    int dragStartValue = 0;
    bool isDragging = false;
    bool wasToggled = false;

    void updateButtonText()
    {
        if (currentValue == 0)
            setButtonText ("OFF");
        else
            setButtonText (juce::String (currentValue));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DraggableToggleButtonComponent)
};

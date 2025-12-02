#pragma once

#include <JuceHeader.h>
class DraggableValueLabel : public juce::Label
{
public:
    DraggableValueLabel (int minVal, int maxVal, int step = 1) : minValue (minVal), maxValue (maxVal), stepSize (step) {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isShiftDown() && onShiftClick)
        {
            onShiftClick();
            return;
        }

        if (e.mods.isLeftButtonDown())
        {
            dragStartY = e.getScreenPosition().y;
            dragStartValue = getText().getIntValue();
            isDragging = true;
        }
        Label::mouseDown (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (isDragging)
        {
            int deltaY = dragStartY - e.getScreenPosition().y;
            int newValue = juce::jlimit (minValue, maxValue, dragStartValue + (deltaY / 5) * stepSize);
            setText (juce::String (newValue), juce::sendNotificationSync);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        isDragging = false;
        Label::mouseUp (e);
    }

    std::function<void()> onShiftClick;

private:
    int minValue, maxValue, stepSize;
    int dragStartY = 0;
    int dragStartValue = 0;
    bool isDragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DraggableValueLabel)
};

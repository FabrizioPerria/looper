#pragma once

#include "ui/daw/DawEditor.h"
#include "ui/daw/DawLookAndFeel.h"
#include <JuceHeader.h>

struct Theme
{
    std::unique_ptr<juce::LookAndFeel> lookAndFeel;
    std::unique_ptr<juce::Component> editorComponent;
};

class ThemeFactory
{
public:
    static std::unique_ptr<Theme> createTheme (const juce::String& themeName, LooperEngine* engine)
    {
        auto theme = std::make_unique<Theme>();

        if (themeName == "Daw")
        {
            theme->lookAndFeel = std::make_unique<DawLookAndFeel>();
            theme->editorComponent = std::make_unique<DawEditor> (engine);
        }
        else
        {
            // Default to Daw theme if unknown
            theme->lookAndFeel = std::make_unique<DawLookAndFeel>();
            theme->editorComponent = std::make_unique<DawEditor> (engine);
        }
        return theme;
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThemeFactory)
};

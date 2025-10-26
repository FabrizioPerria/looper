#pragma once
#include <JuceHeader.h>

namespace LooperTheme
{
// Tokyo Night Color Palette
namespace Colors
{
    // Backgrounds
    const juce::Colour background = juce::Colour (0xff1a1b26);      // Dark blue-grey
    const juce::Colour backgroundLight = juce::Colour (0xff24283b); // Slightly lighter
    const juce::Colour backgroundDark = juce::Colour (0xff16161e);  // Even darker

    const juce::Colour surface = juce::Colour (0xff1f2335);          // Component backgrounds
    const juce::Colour surfaceHighlight = juce::Colour (0xff292e42); // Hover states

    // Accent colors
    const juce::Colour primary = juce::Colour (0xff7aa2f7); // Blue
    const juce::Colour primaryDark = juce::Colour (0xff3d59a1);
    const juce::Colour primaryLight = juce::Colour (0xff9ece6a);

    const juce::Colour cyan = juce::Colour (0xff7dcfff);    // Bright cyan
    const juce::Colour teal = juce::Colour (0xff73daca);    // Teal
    const juce::Colour green = juce::Colour (0xff9ece6a);   // Green
    const juce::Colour yellow = juce::Colour (0xffe0af68);  // Yellow
    const juce::Colour orange = juce::Colour (0xffff9e64);  // Orange
    const juce::Colour red = juce::Colour (0xfff7768e);     // Red/pink
    const juce::Colour magenta = juce::Colour (0xffbb9af7); // Purple/magenta

    // Text
    const juce::Colour text = juce::Colour (0xffc0caf5);         // Light blue-white
    const juce::Colour textDim = juce::Colour (0xff565f89);      // Dimmed
    const juce::Colour textDisabled = juce::Colour (0xff414868); // Disabled

    // UI elements
    const juce::Colour border = juce::Colour (0xff292e42);
    const juce::Colour borderLight = juce::Colour (0xff3b4261);

    // Status colors
    const juce::Colour success = juce::Colour (0xff9ece6a);   // Green
    const juce::Colour warning = juce::Colour (0xffe0af68);   // Yellow
    const juce::Colour error = juce::Colour (0xfff7768e);     // Red
    const juce::Colour recording = juce::Colour (0xffff9e64); // Orange
} // namespace Colors

namespace Dimensions
{
    const float cornerRadius = 8.0f;
    const float smallCornerRadius = 4.0f;
    const float borderWidth = 1.5f;
    const float shadowSize = 4.0f;

    const int buttonHeight = 32;
    const int sliderHeight = 28;
    const int labelHeight = 24;
    const int spacing = 8;
    const int padding = 12;
} // namespace Dimensions

namespace Fonts
{
    inline juce::Font getRegularFont (float size = 14.0f)
    {
        juce::FontOptions options = juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain);
        return juce::Font (options);
    }

    inline juce::Font getBoldFont (float size = 14.0f)
    {
        juce::FontOptions options = juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold);
        return juce::Font (options);
    }

    inline juce::Font getTitleFont (float size = 18.0f)
    {
        juce::FontOptions options = juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), size, juce::Font::bold);
        return juce::Font (options);
    }
} // namespace Fonts
} // namespace LooperTheme

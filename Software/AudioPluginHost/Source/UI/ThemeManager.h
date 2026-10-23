#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Colour IDs for the parts of the host's own UI that JUCE's colour schemes don't
    cover. Look them up with findColour() like any JUCE colour ID.
*/
namespace AppColourIds
{
    enum
    {
        titleBarBackground = 0x7a000001,
        titleBarIcon,
        titleBarIconPressed,
        pluginListRow,
        pluginListSelectedRow,
        pluginListText,
        pluginListSelectedText,
        bypassedNodeBackground,
        signalPath
    };
}

//==============================================================================
/** A named set of colours: a JUCE colour scheme for the standard widgets, plus the host's own colours. */
struct Theme
{
    String id;      // stored in the settings file
    String name;    // shown in the theme picker
    LookAndFeel_V4::ColourScheme colourScheme;
    std::vector<std::pair<int, Colour>> appColours;
};

//==============================================================================
/**
    Holds the available themes and applies the chosen one to every LookAndFeel the
    app uses, then repaints all windows. The choice is saved in the settings file.

    To add a theme, add another make...Theme() function in ThemeManager.cpp.
*/
class ThemeManager
{
public:
    ThemeManager();

    const std::vector<Theme>& getThemes() const noexcept   { return themes; }
    int getCurrentThemeIndex() const noexcept              { return currentIndex; }

    /** Applies the theme saved in the settings file. Call before any windows are created. */
    void loadSavedTheme();

    /** Applies a theme everywhere and saves it as the current choice. */
    void setTheme(int index);

    /** LookAndFeels other than the default one register here so they follow the theme too. */
    void addLookAndFeel(LookAndFeel_V4& lookAndFeel);
    void removeLookAndFeel(LookAndFeel_V4& lookAndFeel);

private:
    void applyTo(LookAndFeel_V4& lookAndFeel) const;
    void applyToAll();

    std::vector<Theme> themes;
    int currentIndex = 0;
    Array<LookAndFeel_V4*> lookAndFeels;

    JUCE_DECLARE_NON_COPYABLE(ThemeManager)
};

ThemeManager& getThemeManager();

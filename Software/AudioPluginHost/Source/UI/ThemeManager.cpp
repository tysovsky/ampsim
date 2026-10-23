#include "ThemeManager.h"
#include "MainHostWindow.h"

namespace
{
    using UIColour = LookAndFeel_V4::ColourScheme::UIColour;

    Theme makeDarkTheme()
    {
        auto scheme = LookAndFeel_V4::getDarkColourScheme();

        return { "dark", "Dark", scheme, {
            { AppColourIds::titleBarBackground,     scheme.getUIColour(UIColour::windowBackground).darker() },
            { AppColourIds::titleBarIcon,           Colours::lightgrey },
            { AppColourIds::titleBarIconPressed,    Colours::white },
            { AppColourIds::pluginListRow,          scheme.getUIColour(UIColour::widgetBackground) },
            { AppColourIds::pluginListSelectedRow,  scheme.getUIColour(UIColour::defaultFill) },
            { AppColourIds::pluginListText,         Colours::white },
            { AppColourIds::pluginListSelectedText, Colours::black },
            { AppColourIds::bypassedNodeBackground, scheme.getUIColour(UIColour::widgetBackground).brighter() },
            { AppColourIds::signalPath,             scheme.getUIColour(UIColour::defaultFill) }
        } };
    }

    Theme makeLightTheme()
    {
        auto scheme = LookAndFeel_V4::getLightColourScheme();

        return { "light", "Light", scheme, {
            { AppColourIds::titleBarBackground,     scheme.getUIColour(UIColour::windowBackground).darker(0.1f) },
            { AppColourIds::titleBarIcon,           Colour(0xff505050) },
            { AppColourIds::titleBarIconPressed,    Colours::black },
            { AppColourIds::pluginListRow,          scheme.getUIColour(UIColour::widgetBackground) },
            { AppColourIds::pluginListSelectedRow,  scheme.getUIColour(UIColour::highlightedFill) },
            { AppColourIds::pluginListText,         Colours::black },
            { AppColourIds::pluginListSelectedText, Colours::white },
            { AppColourIds::bypassedNodeBackground, scheme.getUIColour(UIColour::widgetBackground).darker(0.15f) },
            { AppColourIds::signalPath,             scheme.getUIColour(UIColour::highlightedFill) }
        } };
    }
}

//==============================================================================
ThemeManager::ThemeManager()
{
    themes.push_back(makeDarkTheme());
    themes.push_back(makeLightTheme());
}

void ThemeManager::loadSavedTheme()
{
    const auto savedId = getAppProperties().getUserSettings()->getValue("theme");

    for (int i = 0; i < (int)themes.size(); ++i)
        if (themes[(size_t)i].id == savedId)
            currentIndex = i;

    applyToAll();
}

void ThemeManager::setTheme(int index)
{
    if (!isPositiveAndBelow(index, (int)themes.size()))
        return;

    currentIndex = index;
    applyToAll();

    auto* settings = getAppProperties().getUserSettings();
    settings->setValue("theme", themes[(size_t)index].id);
    settings->saveIfNeeded();

    // Repaint every window with the new colours
    auto& desktop = Desktop::getInstance();

    for (int i = 0; i < desktop.getNumComponents(); ++i)
        desktop.getComponent(i)->sendLookAndFeelChange();
}

void ThemeManager::addLookAndFeel(LookAndFeel_V4& lookAndFeel)
{
    lookAndFeels.addIfNotAlreadyThere(&lookAndFeel);
    applyTo(lookAndFeel);
}

void ThemeManager::removeLookAndFeel(LookAndFeel_V4& lookAndFeel)
{
    lookAndFeels.removeFirstMatchingValue(&lookAndFeel);
}

void ThemeManager::applyTo(LookAndFeel_V4& lookAndFeel) const
{
    const auto& theme = themes[(size_t)currentIndex];

    lookAndFeel.setColourScheme(theme.colourScheme);

    for (const auto& [id, colour] : theme.appColours)
        lookAndFeel.setColour(id, colour);
}

void ThemeManager::applyToAll()
{
    if (auto* defaultLookAndFeel = dynamic_cast<LookAndFeel_V4*>(&LookAndFeel::getDefaultLookAndFeel()))
        applyTo(*defaultLookAndFeel);

    for (auto* lookAndFeel : lookAndFeels)
        applyTo(*lookAndFeel);
}

#pragma once

#include <JuceHeader.h>
#include "ThemeManager.h"

//==============================================================================
/**
    LookAndFeel for screens used with a finger: larger text, and dropdown lists
    whose items are as tall as the dropdown itself.

    Dropdown lists open inside the app window rather than the whole display, and
    long lists are spread over enough columns to fit the window's height, so they
    never need scrolling on the 400 px tall screen.
*/
class TouchLookAndFeel : public LookAndFeel_V4
{
public:
    static constexpr float fontSize = 20.0f;

    // Follow the app's theme like the default LookAndFeel does
    TouchLookAndFeel()              { getThemeManager().addLookAndFeel(*this); }
    ~TouchLookAndFeel() override    { getThemeManager().removeLookAndFeel(*this); }

    /** Options for a menu shown from a component: it opens inside the app window, with items
        itemHeight tall, spread over enough columns to fit the window's height.
    */
    static PopupMenu::Options getMenuOptions(Component& target, int numItems, int itemHeight)
    {
        Component* parent = nullptr;

        if (auto* window = target.findParentComponentOfClass<ResizableWindow>())
            parent = window->getContentComponent();

        const int availableHeight = parent != nullptr ? parent->getHeight()
                                                      : Desktop::getInstance().getDisplays().getDisplayForRect(target.getScreenBounds())->userArea.getHeight();

        // Leave room for the menu's border and the margin JUCE keeps around it
        const int rowsPerColumn = jmax(1, (availableHeight - 32) / itemHeight);
        const int numColumns = jmax(1, (numItems + rowsPerColumn - 1) / rowsPerColumn);

        return PopupMenu::Options().withTargetComponent(&target)
            .withParentComponent(parent)
            .withStandardItemHeight(itemHeight)
            .withMinimumNumColumns(numColumns)
            .withMaximumNumColumns(numColumns);
    }

    PopupMenu::Options getOptionsForComboBoxPopupMenu(ComboBox& box, Label&) override
    {
        // Unlike the default options, don't ask for the selected item to be lined up with the box:
        // JUCE does that by moving the list down, which pushes it off the bottom of the window
        return getMenuOptions(box, box.getNumItems(), box.getHeight())
            .withInitiallySelectedItem(box.getSelectedId())
            .withMinimumWidth(box.getWidth());
    }

    Font getPopupMenuFont() override
    {
        return Font(fontSize);
    }

    Font getComboBoxFont(ComboBox& box) override
    {
        return Font(jmin(fontSize, (float)box.getHeight() * 0.45f));
    }

    Font getTextButtonFont(TextButton&, int buttonHeight) override
    {
        return Font(jmin(fontSize, (float)buttonHeight * 0.45f));
    }
};

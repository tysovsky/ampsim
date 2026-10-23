#pragma once

#include <JuceHeader.h>

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

    PopupMenu::Options getOptionsForComboBoxPopupMenu(ComboBox& box, Label&) override
    {
        Component* parent = nullptr;

        if (auto* window = box.findParentComponentOfClass<ResizableWindow>())
            parent = window->getContentComponent();

        const int itemHeight = box.getHeight();
        const int availableHeight = parent != nullptr ? parent->getHeight()
                                                      : Desktop::getInstance().getDisplays().getDisplayForRect(box.getScreenBounds())->userArea.getHeight();

        // Leave room for the menu's border and the margin JUCE keeps around it
        const int rowsPerColumn = jmax(1, (availableHeight - 32) / itemHeight);
        const int numColumns = jmax(1, (box.getNumItems() + rowsPerColumn - 1) / rowsPerColumn);

        // Unlike the default options, don't ask for the selected item to be lined up with the box:
        // JUCE does that by moving the list down, which pushes it off the bottom of the window
        return PopupMenu::Options().withTargetComponent(&box)
            .withParentComponent(parent)
            .withInitiallySelectedItem(box.getSelectedId())
            .withMinimumWidth(box.getWidth())
            .withStandardItemHeight(itemHeight)
            .withMinimumNumColumns(numColumns)
            .withMaximumNumColumns(numColumns);
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

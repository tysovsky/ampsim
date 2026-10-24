#pragma once

#include <JuceHeader.h>
#include "TouchLookAndFeel.h"

//==============================================================================
/**
    A popup covering the window for typing a line of text without a keyboard: a
    text field, and an on-screen keyboard of keys big enough for a finger. A
    keyboard plugged in works too, with Return for Done and Escape for Cancel.

    Shift capitalises the next letter. Backspace and the arrow keys repeat while
    they're held.
*/
class TextEntryPopup final : public Component
{
public:
    TextEntryPopup();
    ~TextEntryPopup() override;

    /** Opens over the whole of a component with some text to edit, all selected so typing
        replaces it, and calls onDone with the text if Done is tapped.
    */
    void show (Component& parent, const String& title, const String& text, std::function<void (const String&)> onDone);
    void hide();

    void paint (Graphics&) override;
    void resized() override;
    void parentSizeChanged() override;

private:
    struct Key
    {
        std::unique_ptr<TextButton> button;
        String character;       // what it types, or empty for the keys that do something else
        int row = 0;
        float width = 1.0f;     // in key widths
    };

    TextButton& addKey (int row, const String& character, float width = 1.0f, std::function<void()> action = nullptr);
    void type (const String& character);
    void backspace();
    void setShifted (bool);
    void finish();
    Rectangle<int> getPanelBounds() const;

    TouchLookAndFeel lookAndFeel;
    Label titleLabel;
    TextButton cancelButton { "Cancel" }, doneButton { "Done" };
    TextEditor field;
    std::vector<Key> keys;
    TextButton* shiftKey = nullptr;
    bool shifted = false;
    std::function<void (const String&)> onDone;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextEntryPopup)
};

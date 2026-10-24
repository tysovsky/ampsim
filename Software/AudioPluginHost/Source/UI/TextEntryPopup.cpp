#include "TextEntryPopup.h"
#include "ThemeManager.h"

namespace
{
    constexpr int margin = 12;
    constexpr int padding = 16;
    constexpr int headerHeight = 52;
    constexpr int headerButtonWidth = 120;
    constexpr int fieldHeight = 48;
    constexpr int keyGap = 6;
    constexpr int numRows = 5;
    constexpr float rowWidth = 13.0f;   // the widest row, in key widths, which sets the size of a key
}

TextEntryPopup::TextEntryPopup()
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setFont (Font (24.0f, Font::bold));
    titleLabel.setJustificationType (Justification::centred);
    addAndMakeVisible (titleLabel);

    for (auto* button : { &cancelButton, &doneButton })
    {
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (button);
    }

    cancelButton.onClick = [this] { hide(); };
    doneButton.onClick = [this] { finish(); };

    field.setFont (Font (24.0f));
    field.setIndents (12, 0);
    field.setJustification (Justification::centredLeft);
    field.onReturnKey = [this] { finish(); };
    field.onEscapeKey = [this] { hide(); };
    addAndMakeVisible (field);

    // Only characters that can go in a file name, so the same keyboard does for naming files
    const auto addKeys = [this] (int row, const char* characters)
    {
        for (auto* c = characters; *c != 0; ++c)
            addKey (row, String::charToString ((juce_wchar) (uint8) *c));
    };

    // Backspace and the arrows repeat while they're held, like a real keyboard's
    addKeys (0, "1234567890-");
    addKey (0, "Backspace", 2.0f, [this] { backspace(); }).setRepeatSpeed (400, 60);
    addKeys (1, "qwertyuiop()");
    addKeys (2, "asdfghjkl'&");
    shiftKey = &addKey (3, "Shift", 1.5f, [this] { setShifted (! shifted); });
    addKeys (3, "zxcvbnm,._");
    addKey (4, String (CharPointer_UTF8 ("\xe2\x80\xb9")), 1.5f, [this] { field.setCaretPosition (field.getCaretPosition() - 1); })
        .setRepeatSpeed (400, 60);
    addKey (4, "Space", 7.0f, [this] { type (" "); });
    addKey (4, String (CharPointer_UTF8 ("\xe2\x80\xba")), 1.5f, [this] { field.setCaretPosition (field.getCaretPosition() + 1); })
        .setRepeatSpeed (400, 60);
}

TextEntryPopup::~TextEntryPopup()
{
    setLookAndFeel (nullptr);
}

TextButton& TextEntryPopup::addKey (int row, const String& character, float width, std::function<void()> action)
{
    Key key;
    key.button = std::make_unique<TextButton> (character);
    key.row = row;
    key.width = width;

    // A key that types a character gets it; the others do what they're given
    if (action == nullptr)
    {
        key.character = character;
        key.button->onClick = [this, character] { type (shifted ? character.toUpperCase() : character); };
    }
    else
    {
        key.button->onClick = std::move (action);
    }

    // The field keeps the keyboard focus, and with it the caret
    key.button->setWantsKeyboardFocus (false);
    addAndMakeVisible (key.button.get());
    keys.push_back (std::move (key));
    return *keys.back().button;
}

//==============================================================================
void TextEntryPopup::show (Component& parent, const String& title, const String& text, std::function<void (const String&)> callback)
{
    onDone = std::move (callback);

    if (getParentComponent() != &parent)
        parent.addChildComponent (this);

    setBounds (parent.getLocalBounds());

    // The text starts selected, so it's shown in the accent colour rather than the theme's faint grey
    field.setColour (TextEditor::highlightColourId, findColour (AppColourIds::signalPath).withAlpha (0.3f));
    field.setColour (TextEditor::highlightedTextColourId, findColour (TextEditor::textColourId));

    titleLabel.setText (title, dontSendNotification);
    field.setText (text, false);
    setShifted (false);

    setVisible (true);
    toFront (false);

    field.grabKeyboardFocus();
    field.selectAll();
}

void TextEntryPopup::hide()
{
    setVisible (false);
    onDone = nullptr;
}

void TextEntryPopup::finish()
{
    const auto text = field.getText();
    const auto callback = onDone;
    hide();

    if (callback != nullptr)
        callback (text);
}

void TextEntryPopup::type (const String& character)
{
    field.insertTextAtCaret (character);
    setShifted (false);
}

void TextEntryPopup::backspace()
{
    // What's selected, or else the character before the caret
    if (field.getHighlightedRegion().isEmpty())
    {
        const int caret = field.getCaretPosition();

        if (caret == 0)
            return;

        field.setHighlightedRegion ({ caret - 1, caret });
    }

    field.insertTextAtCaret ({});
}

void TextEntryPopup::setShifted (bool shouldBeShifted)
{
    shifted = shouldBeShifted;
    shiftKey->setToggleState (shifted, dontSendNotification);

    for (auto& key : keys)
        if (key.character.isNotEmpty())
            key.button->setButtonText (shifted ? key.character.toUpperCase() : key.character);
}

//==============================================================================
void TextEntryPopup::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.6f));

    const auto panel = getPanelBounds().toFloat();

    g.setColour (findColour (ResizableWindow::backgroundColourId));
    g.fillRoundedRectangle (panel, 10.0f);

    g.setColour (findColour (ComboBox::outlineColourId));
    g.drawRoundedRectangle (panel, 10.0f, 1.5f);
}

void TextEntryPopup::resized()
{
    auto panel = getPanelBounds();
    auto header = panel.removeFromTop (headerHeight).reduced (padding, 6);

    cancelButton.setBounds (header.removeFromLeft (headerButtonWidth));
    doneButton.setBounds (header.removeFromRight (headerButtonWidth));
    titleLabel.setBounds (header);

    auto content = panel.reduced (padding, 0).withTrimmedBottom (padding);
    field.setBounds (content.removeFromTop (fieldHeight));
    content.removeFromTop (10);

    // Each row centred, with its keys in proportion to their widths
    const auto keyWidth = ((float) content.getWidth() + keyGap) / rowWidth;
    const auto keyHeight = (content.getHeight() - (numRows - 1) * keyGap) / numRows;

    for (int row = 0; row < numRows; ++row)
    {
        float width = 0.0f;

        for (auto& key : keys)
            if (key.row == row)
                width += key.width * keyWidth;

        auto x = (float) content.getX() + ((float) content.getWidth() + keyGap - width) / 2.0f;
        const int y = content.getY() + row * (keyHeight + keyGap);

        for (auto& key : keys)
        {
            if (key.row != row)
                continue;

            key.button->setBounds (roundToInt (x), y, roundToInt (key.width * keyWidth) - keyGap, keyHeight);
            x += key.width * keyWidth;
        }
    }
}

void TextEntryPopup::parentSizeChanged()
{
    if (auto* parent = getParentComponent())
        setBounds (parent->getLocalBounds());
}

Rectangle<int> TextEntryPopup::getPanelBounds() const
{
    return getLocalBounds().reduced (margin);
}

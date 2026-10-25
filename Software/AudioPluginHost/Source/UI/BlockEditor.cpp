#include "BlockEditor.h"
#include "ThemeManager.h"

namespace
{
    constexpr int padding = 12;
    constexpr int headerHeight = 56;
    constexpr int titleWidth = 170;
    constexpr int closeButtonWidth = 110;
    constexpr int maxTabWidth = 150;
    constexpr int rowLabelWidth = 96;
    constexpr int maxRowHeight = 180;
    constexpr int keyboardHeight = 150;
    constexpr int octaveButtonWidth = 64;

    // Four octaves and a top C at a time, from C2 to start with
    constexpr int numWhiteKeys = 29;
    constexpr int firstLowestKey = 36;

    // The block property that remembers which page was open (saved with the block, as a view)
    const Identifier pageProperty ("view.editorPage");
}

//==============================================================================
// A keyboard to play along the bottom of a page, with buttons to move it up and down an octave
class BlockEditor::Keyboard final : public Component
{
public:
    explicit Keyboard (MidiKeyboardState& state)
        : keys (state, MidiKeyboardComponent::horizontalKeyboard)
    {
        // A fixed velocity, as where a finger lands on a key says nothing about how hard it hit it
        keys.setVelocity (0.8f, false);
        keys.setScrollButtonsVisible (false);
        keys.setWantsKeyboardFocus (false);
        keys.setMidiChannel (1);

        // Piano colours whatever the theme, with the app's colour for the keys being played
        keys.setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xffd9dee2));
        keys.setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff1c2226));
        keys.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff6d777e));
        keys.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Colours::transparentBlack);
        keys.setColour (MidiKeyboardComponent::textLabelColourId, Colour (0xff4a545b));
        addAndMakeVisible (keys);

        for (auto* button : { &downButton, &upButton })
        {
            button->setWantsKeyboardFocus (false);
            addAndMakeVisible (button);
        }

        downButton.setButtonText (String (CharPointer_UTF8 ("\xe2\x80\xb9 Oct")));
        upButton.setButtonText (String (CharPointer_UTF8 ("Oct \xe2\x80\xba")));
        downButton.onClick = [this] { setLowestKey (lowestKey - 12); };
        upButton.onClick   = [this] { setLowestKey (lowestKey + 12); };
        setLowestKey (firstLowestKey);
    }

    void lookAndFeelChanged() override
    {
        keys.setColour (MidiKeyboardComponent::keyDownOverlayColourId, findColour (AppColourIds::signalPath).withAlpha (0.8f));
    }

    void resized() override
    {
        auto area = getLocalBounds();
        downButton.setBounds (area.removeFromLeft (octaveButtonWidth).reduced (0, 4));
        upButton.setBounds (area.removeFromRight (octaveButtonWidth).reduced (0, 4));
        area.reduce (8, 0);

        keys.setBounds (area);
        keys.setKeyWidth ((float) area.getWidth() / (float) numWhiteKeys);
        keys.setBlackNoteLengthProportion (0.62f);
    }

private:
    // Without its scroll buttons, the keyboard shows its range from the bottom, so moving an octave
    // moves the range
    void setLowestKey (int key)
    {
        lowestKey = jlimit (0, 127 - octaves * 12, key);
        keys.setAvailableRange (lowestKey, lowestKey + octaves * 12);
        downButton.setEnabled (lowestKey > 0);
        upButton.setEnabled (lowestKey + octaves * 12 + 12 <= 127);
    }

    static constexpr int octaves = (numWhiteKeys - 1) / 7;

    MidiKeyboardComponent keys;
    TextButton downButton, upButton;
    int lowestKey = firstLowestKey;
};

//==============================================================================
BlockEditor::BlockEditor (MidiKeyboardState& state)
    : keyState (state)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setFont (Font (24.0f, Font::bold));
    titleLabel.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (titleLabel);

    closeButton.setWantsKeyboardFocus (false);
    closeButton.onClick = [this] { hide(); };
    addAndMakeVisible (closeButton);
}

BlockEditor::~BlockEditor()
{
    hide();
    setLookAndFeel (nullptr);
}

void BlockEditor::show (Component& parent, AudioProcessorGraph::Node::Ptr newNode, const XmlElement& definition, const String& title)
{
    hide();

    node = std::move (newNode);
    editor = std::make_unique<XmlElement> (definition);
    titleLabel.setText (title, dontSendNotification);

    // A tab for each page
    tabs.clear();

    for (auto* page : editor->getChildWithTagNameIterator ("Page"))
    {
        auto* tab = tabs.add (new TextButton (page->getStringAttribute ("name")));
        const int index = tabs.size() - 1;

        tab->setWantsKeyboardFocus (false);
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (1);
        tab->onClick = [this, index]
        {
            if (! tabs[index]->getToggleState())
                return;

            showPage (index);

            if (onViewChange != nullptr)
                onViewChange();
        };
        addAndMakeVisible (tab);
    }

    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setConnectedEdges ((i > 0 ? Button::ConnectedOnLeft : 0) | (i + 1 < tabs.size() ? Button::ConnectedOnRight : 0));

    if (getParentComponent() != &parent)
        parent.addChildComponent (this);

    setBounds (parent.getLocalBounds());
    showPage (node != nullptr ? (int) node->properties[pageProperty] : 0);
    setVisible (true);
    toFront (true);
}

void BlockEditor::hide()
{
    // Notes still held on its keyboard would otherwise keep sounding
    if (keyboard != nullptr)
        keyState.allNotesOff (0);

    rows.clear();
    rowLabels.clear();
    keyboard = nullptr;
    tabs.clear();
    node = nullptr;
    editor = nullptr;
    setVisible (false);
}

bool BlockEditor::isShowing (AudioProcessorGraph::NodeID nodeID) const
{
    return isVisible() && node != nullptr && node->nodeID == nodeID;
}

void BlockEditor::showPage (int index)
{
    if (editor == nullptr || tabs.isEmpty())
        return;

    currentPage = jlimit (0, tabs.size() - 1, index);
    node->properties.set (pageProperty, currentPage);
    tabs[currentPage]->setToggleState (true, dontSendNotification);

    if (keyboard != nullptr)
        keyState.allNotesOff (0);

    rows.clear();
    rowLabels.clear();
    keyboard = nullptr;

    if (auto* page = editor->getChildByName ("Page"))
    {
        for (int i = 0; i < currentPage && page != nullptr; ++i)
            page = page->getNextElementWithTagName ("Page");

        if (page != nullptr)
        {
            for (auto* child : page->getChildIterator())
            {
                if (child->hasTagName ("Row"))
                {
                    auto* row = rows.add (new ControlStrip());
                    row->setControls (node, *child);
                    row->onViewChange = [this] { if (onViewChange != nullptr) onViewChange(); };
                    rowLabels.add (child->getStringAttribute ("label"));
                    addAndMakeVisible (row);
                }
                else if (child->hasTagName ("Keyboard"))
                {
                    keyboard = std::make_unique<Keyboard> (keyState);
                    addAndMakeVisible (*keyboard);
                }
            }
        }
    }

    resized();
    repaint();
}

//==============================================================================
void BlockEditor::paint (Graphics& g)
{
    g.fillAll (findColour (ResizableWindow::backgroundColourId));

    // The header, in the title bar's colour
    g.setColour (findColour (AppColourIds::titleBarBackground));
    g.fillRect (getLocalBounds().removeFromTop (headerHeight));
    g.setColour (findColour (ComboBox::outlineColourId));
    g.fillRect (0, headerHeight, getWidth(), 1);

    // Each row's label down its left-hand side, and a line between rows
    for (int i = 0; i < rows.size(); ++i)
    {
        const auto bounds = rows[i]->getBounds();

        if (i > 0)
        {
            g.setColour (findColour (ComboBox::outlineColourId).withAlpha (0.5f));
            g.fillRect (padding, bounds.getY() - padding / 2, getWidth() - 2 * padding, 1);
        }

        if (rowLabels[i].isNotEmpty())
        {
            g.setColour (findColour (AppColourIds::signalPath));
            g.setFont (Font (15.0f, Font::bold));
            g.drawFittedText (rowLabels[i].toUpperCase(), Rectangle<int> (padding, bounds.getY(), rowLabelWidth - padding, bounds.getHeight()),
                              Justification::centredLeft, 3);
        }
    }
}

void BlockEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight).reduced (padding, 6);

    titleLabel.setBounds (header.removeFromLeft (titleWidth));
    closeButton.setBounds (header.removeFromRight (closeButtonWidth));
    header.removeFromRight (padding);
    header.removeFromLeft (padding);

    // The tabs share what's left, up to a width each
    if (! tabs.isEmpty())
    {
        const int tabWidth = jmin (maxTabWidth, header.getWidth() / tabs.size());
        auto tabArea = header.withSizeKeepingCentre (tabWidth * tabs.size(), header.getHeight());

        for (auto* tab : tabs)
            tab->setBounds (tabArea.removeFromLeft (tabWidth));
    }

    area.removeFromTop (padding / 2);

    if (keyboard != nullptr)
        keyboard->setBounds (area.removeFromBottom (keyboardHeight).reduced (padding, padding / 2));

    // The rows share the rest, with room for their labels if they have any, up to a height each so a
    // page with one row doesn't stretch it from top to bottom; they sit in the middle of the page
    const bool hasLabels = std::any_of (rowLabels.begin(), rowLabels.end(), [] (const String& s) { return s.isNotEmpty(); });
    auto rowsArea = area.reduced (padding, 0).withTrimmedLeft (hasLabels ? rowLabelWidth - padding : 0);
    rowsArea = rowsArea.withSizeKeepingCentre (rowsArea.getWidth(), jmin (rowsArea.getHeight(), rows.size() * maxRowHeight));

    for (int i = 0; i < rows.size(); ++i)
    {
        const int height = rowsArea.getHeight() / (rows.size() - i);
        rows[i]->setBounds (rowsArea.removeFromTop (height).reduced (0, padding / 2));
    }
}

void BlockEditor::parentSizeChanged()
{
    if (auto* parent = getParentComponent())
        setBounds (parent->getLocalBounds());
}

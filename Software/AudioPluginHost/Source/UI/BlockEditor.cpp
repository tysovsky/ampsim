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

    // Under a visualiser, the rows are only as tall as they need to be, to give it the room
    constexpr int rowHeightUnderVisualiser = 150;

    // The block property that remembers which page was open (saved with the block, as a view)
    const Identifier pageProperty ("view.editorPage");
}

//==============================================================================
BlockEditor::BlockEditor()
{
    // It covers the window, so what's under it needn't be drawn (such as the chain, for every
    // frame of a visualiser)
    setOpaque (true);
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

void BlockEditor::show (Component& parent, AudioProcessorGraph::Node::Ptr newNode, const XmlElement& definition, const String& title,
                        AudioProcessorGraph::Node::Ptr newLevelStage)
{
    hide();

    node = std::move (newNode);
    levelStage = std::move (newLevelStage);
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
    rows.clear();
    rowLabels.clear();
    visualiser = nullptr;
    tabs.clear();
    node = nullptr;
    levelStage = nullptr;
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

    rows.clear();
    rowLabels.clear();
    visualiser = nullptr;

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
                else if (child->hasTagName ("Visualiser"))
                {
                    visualiser = std::make_unique<SoundVisualiser>();
                    visualiser->setSound (levelStage);
                    addAndMakeVisible (*visualiser);
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

    area.removeFromTop (1);

    // The rows have room for their labels if they have any
    const bool hasLabels = std::any_of (rowLabels.begin(), rowLabels.end(), [] (const String& s) { return s.isNotEmpty(); });
    auto rowsArea = area.reduced (padding, 0).withTrimmedLeft (hasLabels ? rowLabelWidth - padding : 0);

    if (visualiser != nullptr)
    {
        // The rows along the bottom, and the visualiser above them, across the whole width. It doesn't
        // go behind them, so the controls aren't drawn again for every frame it draws.
        rowsArea = rowsArea.removeFromBottom (jmin (rowsArea.getHeight() / 2, rows.size() * rowHeightUnderVisualiser));
        visualiser->setBounds (area.withBottom (rowsArea.getY()));
    }
    else
    {
        // Up to a height each, so a page with one row doesn't stretch it from top to bottom; they sit
        // in the middle of the page
        rowsArea.removeFromTop (padding / 2);
        rowsArea = rowsArea.withSizeKeepingCentre (rowsArea.getWidth(), jmin (rowsArea.getHeight(), rows.size() * maxRowHeight));
    }

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

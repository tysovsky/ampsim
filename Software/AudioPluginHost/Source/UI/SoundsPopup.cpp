#include "SoundsPopup.h"
#include "RowButtons.h"
#include "ThemeManager.h"
#include "../Plugins/SoundLibrary.h"

namespace
{
    constexpr int margin = 12;
    constexpr int padding = 16;
    constexpr int headerHeight = 52;
    constexpr int headerButtonWidth = 120;

    // The sounds go across the screen in as many columns of at least this width as fit
    constexpr int minTileWidth = 360;
    constexpr int tileHeight = 64;
    constexpr int tileGap = 8;
    constexpr int tileButtonWidth = 56;
}

//==============================================================================
// A saved sound: tapping it loads it, and its buttons rename and delete it
class SoundsPopup::Tile final : public Component
{
public:
    Tile (SoundsPopup& p, const String& soundName, Time saved)
        : name (soundName), popup (p), saveTime (saved)
    {
        renameButton.onClick = [this] { popup.renameSound (name); };
        deleteButton.onClick = [this] { popup.deleteSound (name); };
        addAndMakeVisible (renameButton);
        addAndMakeVisible (deleteButton);
    }

    void setCurrent (bool isCurrentSound)
    {
        if (isCurrent != isCurrentSound)
        {
            isCurrent = isCurrentSound;
            repaint();
        }
    }

    bool isCurrentSound() const noexcept    { return isCurrent; }

    void paint (Graphics& g) override
    {
        const auto box = getLocalBounds().toFloat();
        const auto accent = findColour (AppColourIds::signalPath);
        const auto textColour = findColour (Label::textColourId);

        // The sound the block is set to stands out in the accent colour
        auto background = isCurrent ? accent.withAlpha (0.3f) : findColour (TextEditor::backgroundColourId);

        if (isMouseButtonDown())
            background = background.contrasting (0.1f);

        g.setColour (background);
        g.fillRoundedRectangle (box, 8.0f);

        if (isCurrent)
        {
            g.setColour (accent);
            g.drawRoundedRectangle (box.reduced (1.0f), 8.0f, 2.0f);
        }

        // Room for the rename and delete buttons at the right-hand end
        auto area = getLocalBounds().withTrimmedRight (2 * tileButtonWidth).reduced (16, 8);

        g.setColour (textColour);
        g.setFont (Font (18.0f, Font::bold));
        g.drawText (name, area.removeFromTop (area.getHeight() / 2), Justification::bottomLeft, true);

        g.setColour (textColour.withAlpha (0.6f));
        g.setFont (Font (14.0f));
        g.drawText (isCurrent ? "Loaded now" : "Saved " + saveTime.formatted ("%d %b %Y"), area, Justification::topLeft, true);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        deleteButton.setBounds (area.removeFromRight (tileButtonWidth));
        renameButton.setBounds (area.removeFromRight (tileButtonWidth));
    }

    void mouseDown (const MouseEvent&) override    { repaint(); }

    void mouseUp (const MouseEvent& e) override
    {
        repaint();

        // Not at the end of a drag to scroll the sounds
        if (e.mouseWasClicked())
            popup.loadSound (name);
    }

    const String name;

private:
    SoundsPopup& popup;
    const Time saveTime;
    bool isCurrent = false;
    PencilButton renameButton;
    BinButton deleteButton;
};

//==============================================================================
SoundsPopup::SoundsPopup()
{
    // Covering the window, so nothing under it needs drawing, e.g. while a sound's being drawn
    // behind it in a block's editor
    setOpaque (true);
    setLookAndFeel (&lookAndFeel);

    titleLabel.setFont (Font (24.0f, Font::bold));
    addAndMakeVisible (titleLabel);

    emptyLabel.setFont (Font (TouchLookAndFeel::fontSize));
    emptyLabel.setJustificationType (Justification::centred);
    addChildComponent (emptyLabel);

    previousButton.setButtonText (String (CharPointer_UTF8 ("\xe2\x80\xb9  Preset")));
    nextButton.setButtonText (String (CharPointer_UTF8 ("Preset  \xe2\x80\xba")));
    previousButton.setConnectedEdges (Button::ConnectedOnRight);
    nextButton.setConnectedEdges (Button::ConnectedOnLeft);

    previousButton.onClick = [this] { if (onBrowse != nullptr) onBrowse (-1); };
    nextButton.onClick = [this] { if (onBrowse != nullptr) onBrowse (1); };
    saveButton.onClick = [this] { saveSound(); };
    closeButton.onClick = [this] { hide(); };

    for (auto* button : { &previousButton, &nextButton, &saveButton, &closeButton })
    {
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (button);
    }

    viewport.setViewedComponent (&tileHolder, false);
    viewport.setScrollOnDragMode (Viewport::ScrollOnDragMode::all);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
}

SoundsPopup::~SoundsPopup()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void SoundsPopup::show (Component& parent, AudioProcessorGraph::Node::Ptr newNode, const String& title, bool canBrowse)
{
    node = std::move (newNode);

    titleLabel.setText (title, dontSendNotification);
    previousButton.setVisible (canBrowse);
    nextButton.setVisible (canBrowse);
    emptyLabel.setText (canBrowse ? "No sounds saved yet. Find one with Preset, then tap Save to keep it here."
                                  : "No sounds saved yet. Tap Save to keep the sound as it is now.",
                        dontSendNotification);

    if (getParentComponent() != &parent)
        parent.addChildComponent (this);

    setBounds (parent.getLocalBounds());
    updateTiles();

    // Scrolled to the sound the block is set to
    viewport.setViewPosition (0, 0);

    for (auto* tile : tiles)
        if (tile->isCurrentSound() && tile->getBottom() > viewport.getViewHeight())
            viewport.setViewPosition (0, tile->getY());

    setVisible (true);
    toFront (false);
}

void SoundsPopup::hide()
{
    // The tiles stay until it's opened again, as this can be from tapping one of them
    textEntry.hide();
    setVisible (false);
    node = nullptr;
}

void SoundsPopup::refresh()
{
    if (node == nullptr)
        return;

    const auto current = SoundLibrary::getCurrentSound (*node);

    for (auto* tile : tiles)
        tile->setCurrent (tile->name.equalsIgnoreCase (current));
}

void SoundsPopup::updateTiles()
{
    tiles.clear();

    if (node != nullptr)
    {
        const auto library = SoundLibrary::forBlock (*node);

        for (auto& name : library.getNames())
            tileHolder.addAndMakeVisible (tiles.add (new Tile (*this, name, library.getSaveTime (name))));
    }

    refresh();
    emptyLabel.setVisible (tiles.isEmpty());
    resized();
}

//==============================================================================
void SoundsPopup::loadSound (const String& name)
{
    if (node == nullptr)
        return;

    if (SoundLibrary::forBlock (*node).load (*node->getProcessor(), name))
        setCurrentSound (name);

    // Like picking from a dropdown list
    hide();
}

void SoundsPopup::saveSound()
{
    if (node == nullptr)
        return;

    const auto library = SoundLibrary::forBlock (*node);
    const auto current = SoundLibrary::getCurrentSound (*node);

    // The name of the sound the block is set to, to save over it, or a new one
    const auto suggestion = library.contains (current) ? current : library.getUnusedName ("Sound 1");

    textEntry.show (*this, "Save sound", suggestion, [safeThis = SafePointer<SoundsPopup> (this), library, current] (const String& typed)
    {
        const auto name = SoundLibrary::makeLegalName (typed);

        if (safeThis == nullptr || safeThis->node == nullptr || name.isEmpty())
            return;

        // Keeping the name of the sound it's set to saves over that sound; another sound's name gets
        // a number on the end, rather than replacing that sound
        const auto saveAs = name.equalsIgnoreCase (current) ? name : library.getUnusedName (name);

        if (library.save (*safeThis->node->getProcessor(), saveAs))
        {
            safeThis->setCurrentSound (saveAs);
            safeThis->updateTiles();

            for (auto* tile : safeThis->tiles)
                if (tile->isCurrentSound())
                    safeThis->viewport.setViewPosition (0, jmax (0, tile->getBottom() - safeThis->viewport.getViewHeight()));
        }
    });
}

void SoundsPopup::renameSound (const String& name)
{
    textEntry.show (*this, "Rename sound", name, [safeThis = SafePointer<SoundsPopup> (this), name] (const String& typed)
    {
        auto newName = SoundLibrary::makeLegalName (typed);

        if (safeThis == nullptr || safeThis->node == nullptr || newName.isEmpty() || newName == name)
            return;

        // Another sound with the name gets a number on the end; changing only the capitals is fine
        const auto library = SoundLibrary::forBlock (*safeThis->node);

        if (! newName.equalsIgnoreCase (name))
            newName = library.getUnusedName (newName);

        if (! library.rename (name, newName))
            return;

        if (SoundLibrary::getCurrentSound (*safeThis->node).equalsIgnoreCase (name))
            safeThis->setCurrentSound (newName);

        safeThis->updateTiles();
    });
}

void SoundsPopup::deleteSound (const String& name)
{
    // Later, as the tiles are made again without it, including the one with the button that's just
    // been tapped
    MessageManager::callAsync ([safeThis = SafePointer<SoundsPopup> (this), name]
    {
        if (safeThis == nullptr || safeThis->node == nullptr)
            return;

        SoundLibrary::forBlock (*safeThis->node).remove (name);

        if (SoundLibrary::getCurrentSound (*safeThis->node).equalsIgnoreCase (name))
            safeThis->setCurrentSound ({});

        safeThis->updateTiles();
    });
}

void SoundsPopup::setCurrentSound (const String& name)
{
    SoundLibrary::setCurrentSound (*node, name);
    refresh();

    if (onSoundChange != nullptr)
        onSoundChange();
}

//==============================================================================
void SoundsPopup::paint (Graphics& g)
{
    // The window behind, as if dimmed
    const auto background = findColour (ResizableWindow::backgroundColourId);
    g.fillAll (background.interpolatedWith (Colours::black, 0.6f));

    const auto panel = getPanelBounds().toFloat();

    g.setColour (background);
    g.fillRoundedRectangle (panel, 10.0f);

    g.setColour (findColour (ComboBox::outlineColourId));
    g.drawRoundedRectangle (panel, 10.0f, 1.5f);
    g.fillRect (panel.getX() + padding, panel.getY() + headerHeight, panel.getWidth() - 2 * padding, 1.0f);
}

void SoundsPopup::resized()
{
    auto panel = getPanelBounds();
    auto header = panel.removeFromTop (headerHeight).reduced (padding, 6);

    closeButton.setBounds (header.removeFromRight (headerButtonWidth));
    header.removeFromRight (padding);
    saveButton.setBounds (header.removeFromRight (headerButtonWidth));
    header.removeFromRight (padding);

    if (nextButton.isVisible())
    {
        nextButton.setBounds (header.removeFromRight (headerButtonWidth));
        previousButton.setBounds (header.removeFromRight (headerButtonWidth));
        header.removeFromRight (padding);
    }

    titleLabel.setBounds (header);

    const auto content = panel.reduced (padding, 10);
    viewport.setBounds (content);
    emptyLabel.setBounds (content);

    // Across and then down, leaving room for the scroll bar on the right
    const int width = content.getWidth() - viewport.getScrollBarThickness() - 4;
    const int columns = jmax (1, (width + tileGap) / (minTileWidth + tileGap));
    const int tileWidth = (width - (columns - 1) * tileGap) / columns;
    const int rows = (tiles.size() + columns - 1) / columns;

    tileHolder.setSize (width, jmax (0, rows * (tileHeight + tileGap) - tileGap));

    for (int i = 0; i < tiles.size(); ++i)
        tiles[i]->setBounds ((i % columns) * (tileWidth + tileGap), (i / columns) * (tileHeight + tileGap), tileWidth, tileHeight);
}

void SoundsPopup::parentSizeChanged()
{
    if (auto* parent = getParentComponent())
        setBounds (parent->getLocalBounds());
}

Rectangle<int> SoundsPopup::getPanelBounds() const
{
    return getLocalBounds().reduced (margin);
}

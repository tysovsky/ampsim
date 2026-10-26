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

    // The column of groups down the left
    constexpr int groupColumnWidth = 200;
    constexpr int groupButtonHeight = 44;
    constexpr int groupGap = 6;
    constexpr int scrollBarRoom = 12;

    // The sounds go across the screen in as many columns of at least this width as fit: the user's
    // own are wider, for their rename and delete buttons
    constexpr int minOwnTileWidth = 360;
    constexpr int minPresetTileWidth = 240;
    constexpr int tileHeight = 56;
    constexpr int tileGap = 8;
    constexpr int tileButtonWidth = 56;
}

//==============================================================================
// A sound: tapping it loads it, and for one of the user's own, its buttons rename and delete it
class SoundsPopup::Tile final : public Component
{
public:
    Tile (SoundsPopup& p, const String& soundId, const String& details, bool canBeChanged)
        : id (soundId), popup (p), subtitle (details), hasButtons (canBeChanged)
    {
        if (hasButtons)
        {
            renameButton.onClick = [this] { popup.renameSound (id); };
            deleteButton.onClick = [this] { popup.deleteSound (id); };
            addAndMakeVisible (renameButton);
            addAndMakeVisible (deleteButton);
        }
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
        auto area = getLocalBounds().withTrimmedRight (hasButtons ? 2 * tileButtonWidth : 0).reduced (14, 7);
        const auto details = isCurrent ? String ("Loaded now") : subtitle;

        g.setColour (textColour);
        g.setFont (Font (18.0f, Font::bold));

        if (details.isEmpty())
        {
            g.drawText (SoundLibrary::getName (id), area, Justification::centredLeft, true);
            return;
        }

        g.drawText (SoundLibrary::getName (id), area.removeFromTop (area.getHeight() / 2), Justification::bottomLeft, true);

        g.setColour (textColour.withAlpha (0.6f));
        g.setFont (Font (14.0f));
        g.drawText (details, area, Justification::topLeft, true);
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
            popup.loadSound (id);
    }

    const String id;

private:
    SoundsPopup& popup;
    const String subtitle;
    const bool hasButtons;
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

    for (auto* viewport : { &groupViewport, &tileViewport })
    {
        viewport->setScrollOnDragMode (Viewport::ScrollOnDragMode::all);
        viewport->setScrollBarsShown (true, false);
        viewport->setScrollBarThickness (8);
        addAndMakeVisible (viewport);
    }

    groupViewport.setViewedComponent (&groupHolder, false);
    tileViewport.setViewedComponent (&tileHolder, false);
}

SoundsPopup::~SoundsPopup()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void SoundsPopup::show (Component& parent, AudioProcessorGraph::Node::Ptr newNode, const String& title, bool canBrowsePresets)
{
    node = std::move (newNode);
    canBrowse = canBrowsePresets;
    titleLabel.setText (title, dontSendNotification);

    if (getParentComponent() != &parent)
        parent.addChildComponent (this);

    setBounds (parent.getLocalBounds());
    updateGroups();

    // The group of the sound it's set to, or with none, the user's own sounds unless there are
    // only presets
    const auto library = SoundLibrary::forBlock (*node);
    const auto current = SoundLibrary::getCurrentSound (*node);
    auto groupToShow = SoundLibrary::getGroup (current);

    if (! groups.contains (groupToShow) || (current.isEmpty() && groups.size() > 1 && library.getNames().isEmpty()))
        groupToShow = groups.size() > 1 ? groups[1] : String();

    showGroup (groupToShow);
    scrollToCurrentSound();

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
        tile->setCurrent (tile->id.equalsIgnoreCase (current));
}

void SoundsPopup::updateGroups()
{
    groupButtons.clear();
    groups.clear();

    const auto presetGroups = SoundLibrary::forBlock (*node).getGroups();

    // The column's only there for a library with presets, and the user's own sounds come first
    if (! presetGroups.isEmpty())
    {
        groups.add ({});
        groups.addArray (presetGroups);
    }

    for (auto& g : groups)
    {
        auto* button = groupButtons.add (new TextButton (g.isEmpty() ? "My sounds" : g));
        button->setWantsKeyboardFocus (false);
        button->onClick = [this, g] { showGroup (g); };
        groupHolder.addAndMakeVisible (button);
    }

    // The plugin's own presets are only stepped through here if the library hasn't got them
    previousButton.setVisible (canBrowse && groups.isEmpty());
    nextButton.setVisible (canBrowse && groups.isEmpty());
    groupViewport.setVisible (! groups.isEmpty());
}

void SoundsPopup::showGroup (const String& groupToShow)
{
    group = groupToShow;

    for (int i = 0; i < groupButtons.size(); ++i)
        groupButtons[i]->setToggleState (groups[i] == group, dontSendNotification);

    emptyLabel.setText (canBrowse && groups.isEmpty() ? "No sounds saved yet. Find one with Preset, then tap Save to keep it here."
                                                      : "No sounds of your own yet. Tap Save to keep the sound as it is now.",
                        dontSendNotification);

    updateTiles();
    tileViewport.setViewPosition (0, 0);
}

void SoundsPopup::updateTiles()
{
    tiles.clear();

    if (node != nullptr)
    {
        const auto library = SoundLibrary::forBlock (*node);
        const bool isOwn = group.isEmpty();

        for (auto& name : library.getNames (group))
        {
            const auto id = SoundLibrary::makeId (group, name);
            const auto details = isOwn ? "Saved " + library.getSaveTime (id).formatted ("%d %b %Y") : String();

            tileHolder.addAndMakeVisible (tiles.add (new Tile (*this, id, details, isOwn)));
        }
    }

    refresh();
    emptyLabel.setVisible (tiles.isEmpty());
    resized();
}

void SoundsPopup::scrollToCurrentSound()
{
    for (auto* tile : tiles)
        if (tile->isCurrentSound() && tile->getBottom() > tileViewport.getViewPositionY() + tileViewport.getViewHeight())
            tileViewport.setViewPosition (0, jmax (0, tile->getBottom() - tileViewport.getViewHeight()));
}

//==============================================================================
void SoundsPopup::loadSound (const String& id)
{
    if (node == nullptr)
        return;

    if (SoundLibrary::forBlock (*node).load (*node->getProcessor(), id))
        setCurrentSound (id);

    // Like picking from a dropdown list
    hide();
}

void SoundsPopup::saveSound()
{
    if (node == nullptr)
        return;

    const auto library = SoundLibrary::forBlock (*node);
    const auto current = SoundLibrary::getCurrentSound (*node);
    const bool isOwnSound = current.isNotEmpty() && SoundLibrary::getGroup (current).isEmpty() && library.contains (current);

    // The name of the sound it's set to: to save over it if it's one of the user's own, or to keep
    // a preset as it is now; or a new one
    const auto suggestion = current.isNotEmpty() ? SoundLibrary::getName (current) : library.getUnusedName ("Sound 1");

    textEntry.show (*this, "Save sound", suggestion, [safeThis = SafePointer<SoundsPopup> (this), library, current, isOwnSound] (const String& typed)
    {
        const auto name = SoundLibrary::makeLegalName (typed);

        if (safeThis == nullptr || safeThis->node == nullptr || name.isEmpty())
            return;

        // Keeping the name of the user's own sound it's set to saves over it; another sound's name
        // gets a number on the end, rather than replacing that sound
        const auto saveAs = isOwnSound && name.equalsIgnoreCase (current) ? name : library.getUnusedName (name);

        if (library.save (*safeThis->node->getProcessor(), saveAs))
        {
            safeThis->setCurrentSound (saveAs);
            safeThis->updateGroups();
            safeThis->showGroup ({});
            safeThis->scrollToCurrentSound();
        }
    });
}

void SoundsPopup::renameSound (const String& id)
{
    textEntry.show (*this, "Rename sound", SoundLibrary::getName (id), [safeThis = SafePointer<SoundsPopup> (this), id] (const String& typed)
    {
        auto newName = SoundLibrary::makeLegalName (typed);
        const auto name = SoundLibrary::getName (id);

        if (safeThis == nullptr || safeThis->node == nullptr || newName.isEmpty() || newName == name)
            return;

        // Another sound with the name gets a number on the end; changing only the capitals is fine
        const auto library = SoundLibrary::forBlock (*safeThis->node);
        const auto groupOfSound = SoundLibrary::getGroup (id);

        if (! newName.equalsIgnoreCase (name))
            newName = library.getUnusedName (newName, groupOfSound);

        if (! library.rename (id, newName))
            return;

        if (SoundLibrary::getCurrentSound (*safeThis->node).equalsIgnoreCase (id))
            safeThis->setCurrentSound (SoundLibrary::makeId (groupOfSound, newName));

        safeThis->updateTiles();
    });
}

void SoundsPopup::deleteSound (const String& id)
{
    // Later, as the tiles are made again without it, including the one with the button that's just
    // been tapped
    MessageManager::callAsync ([safeThis = SafePointer<SoundsPopup> (this), id]
    {
        if (safeThis == nullptr || safeThis->node == nullptr)
            return;

        SoundLibrary::forBlock (*safeThis->node).remove (id);

        if (SoundLibrary::getCurrentSound (*safeThis->node).equalsIgnoreCase (id))
            safeThis->setCurrentSound ({});

        safeThis->updateTiles();
    });
}

void SoundsPopup::setCurrentSound (const String& id)
{
    SoundLibrary::setCurrentSound (*node, id);
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

    auto content = panel.reduced (padding, 10);

    // The groups down the left, leaving room for the scroll bar
    if (! groupButtons.isEmpty())
    {
        groupViewport.setBounds (content.removeFromLeft (groupColumnWidth));
        content.removeFromLeft (padding);

        const int buttonWidth = groupColumnWidth - scrollBarRoom;
        groupHolder.setSize (buttonWidth, groupButtons.size() * (groupButtonHeight + groupGap) - groupGap);

        for (int i = 0; i < groupButtons.size(); ++i)
            groupButtons[i]->setBounds (0, i * (groupButtonHeight + groupGap), buttonWidth, groupButtonHeight);
    }

    tileViewport.setBounds (content);
    emptyLabel.setBounds (content);

    // Across and then down, leaving room for the scroll bar on the right
    const int width = content.getWidth() - scrollBarRoom;
    const int minTileWidth = group.isEmpty() ? minOwnTileWidth : minPresetTileWidth;
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

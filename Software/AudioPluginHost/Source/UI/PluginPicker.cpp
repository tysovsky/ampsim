#include "PluginPicker.h"
#include "MainHostWindow.h"
#include "ThemeManager.h"

namespace
{
    // The setting that remembers whether the first page was last by plugin or by type
    const char* const groupingKey = "pluginPickerGrouping";

    constexpr int margin = 12;
    constexpr int padding = 16;
    constexpr int headerHeight = 52;
    constexpr int headerButtonWidth = 120;
    constexpr int tileGap = 12;
    constexpr int minTileWidth = 170;
    constexpr int tileHeight = 142;     // two rows fit on the 400 px tall screen

    // Moving further than this between pressing and letting go is scrolling the grid, not tapping a tile
    constexpr float maxTapDistance = 12.0f;

    String describeCount (size_t count)
    {
        return String ((int) count) + (count == 1 ? " block" : " blocks");
    }
}

//==============================================================================
// A large button with an icon, a name and an optional line underneath
class PluginPicker::Tile final : public Component
{
public:
    explicit Tile (const Entry& e) : entry (e) {}

    void paint (Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        const auto accent = findColour (AppColourIds::signalPath);
        const auto textColour = findColour (TextEditor::textColourId);

        g.setColour (findColour (TextEditor::backgroundColourId).overlaidWith (accent.withAlpha (isPressed ? 0.3f : 0.0f)));
        g.fillRoundedRectangle (area, 10.0f);

        g.setColour (findColour (ComboBox::outlineColourId));
        g.drawRoundedRectangle (area, 10.0f, 1.0f);

        auto content = area.reduced (12.0f, 10.0f);

        // A chevron in the corner of tiles that open another page
        if (entry.opensPage)
        {
            const auto corner = content.withLeft (content.getRight() - 9.0f).withHeight (18.0f);

            Path chevron;
            chevron.startNewSubPath (corner.getTopLeft());
            chevron.lineTo (corner.getRight(), corner.getCentreY());
            chevron.lineTo (corner.getBottomLeft());

            g.setColour (textColour.withAlpha (0.5f));
            g.strokePath (chevron, PathStrokeType (2.5f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        drawBlockIcon (g, entry.icon, content.removeFromTop (content.getHeight() * 0.45f), accent);
        content.removeFromTop (6.0f);

        if (entry.subtitle.isNotEmpty())
        {
            g.setColour (textColour.withAlpha (0.6f));
            g.setFont (Font (14.0f));
            g.drawFittedText (entry.subtitle, content.removeFromBottom (18.0f).toNearestInt(), Justification::centred, 1);
        }

        g.setColour (textColour);
        g.setFont (Font (18.0f, Font::bold));
        g.drawFittedText (entry.name, content.toNearestInt(), Justification::centred, 2);
    }

    void mouseDown (const MouseEvent&) override
    {
        setPressed (true);
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (getTravel (e) > maxTapDistance)
            setPressed (false);
    }

    void mouseUp (const MouseEvent& e) override
    {
        const bool wasTapped = isPressed && contains (e.getPosition()) && getTravel (e) <= maxTapDistance;
        setPressed (false);

        if (wasTapped && entry.onTap != nullptr)
        {
            // Copied first, as tapping can open another page, which deletes this tile
            const auto onTap = entry.onTap;
            onTap();
        }
    }

private:
    // Measured on the screen, as the tile moves along with the finger while the grid scrolls
    static float getTravel (const MouseEvent& e)
    {
        return e.getMouseDownScreenPosition().toFloat().getDistanceFrom (e.getScreenPosition().toFloat());
    }

    void setPressed (bool shouldBePressed)
    {
        if (isPressed != shouldBePressed)
        {
            isPressed = shouldBePressed;
            repaint();
        }
    }

    Entry entry;
    bool isPressed = false;
};

//==============================================================================
PluginPicker::PluginPicker (KnownPluginList& kpl, const std::map<String, PluginModuleSet>& sets)
    : knownPlugins (kpl), moduleSets (sets)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setFont (Font (24.0f, Font::bold));
    titleLabel.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (titleLabel);

    emptyLabel.setText ("No plugins found - scan for plugins first", dontSendNotification);
    emptyLabel.setFont (Font (TouchLookAndFeel::fontSize));
    emptyLabel.setJustificationType (Justification::centred);
    addChildComponent (emptyLabel);

    backButton.setButtonText (String (CharPointer_UTF8 ("\xe2\x80\xb9  Back")));
    backButton.onClick = [this] { goBack(); };
    addChildComponent (backButton);

    closeButton.onClick = [this] { hide(); };
    addAndMakeVisible (closeButton);

    // A two-way switch for how the first page is grouped. Turning one on turns the other off, which
    // also calls its onClick, so each only acts when it's the one that's on.
    for (auto* button : { &byPluginButton, &byTypeButton })
    {
        button->setClickingTogglesState (true);
        button->setRadioGroupId (1);
        addAndMakeVisible (button);
    }

    // There's no keyboard, and a focused button is drawn in a stronger colour than the others
    for (auto* button : { &backButton, &closeButton, &byPluginButton, &byTypeButton })
        button->setWantsKeyboardFocus (false);

    byPluginButton.setConnectedEdges (Button::ConnectedOnRight);
    byTypeButton.setConnectedEdges (Button::ConnectedOnLeft);
    byPluginButton.onClick = [this] { if (byPluginButton.getToggleState()) setGrouping (Grouping::byPlugin); };
    byTypeButton.onClick   = [this] { if (byTypeButton.getToggleState())   setGrouping (Grouping::byType); };

    viewport.setViewedComponent (&grid, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    viewport.setScrollOnDragMode (Viewport::ScrollOnDragMode::all);
    addAndMakeVisible (viewport);
}

PluginPicker::~PluginPicker()
{
    viewport.setViewedComponent (nullptr, false);
    setLookAndFeel (nullptr);
}

//==============================================================================
void PluginPicker::show (Component& parent, std::function<void (const PluginChoice&)> callback)
{
    onChosen = std::move (callback);

    if (getParentComponent() != &parent)
        parent.addChildComponent (this);

    setBounds (parent.getLocalBounds());

    // It opens grouped the way it was last left
    const auto grouping = getAppProperties().getUserSettings()->getValue (groupingKey) == "byType" ? Grouping::byType
                                                                                                   : Grouping::byPlugin;
    (grouping == Grouping::byType ? byTypeButton : byPluginButton).setToggleState (true, dontSendNotification);
    setGrouping (grouping);

    setVisible (true);
    toFront (true);
}

void PluginPicker::hide()
{
    setVisible (false);
    onChosen = nullptr;
}

void PluginPicker::choose (const PluginChoice& choice)
{
    // Closed first, so the chain with the new plugin in it is what's left showing
    const auto callback = onChosen;
    hide();

    if (callback != nullptr)
        callback (choice);
}

//==============================================================================
void PluginPicker::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.6f));

    const auto panel = getPanelBounds().toFloat();

    g.setColour (findColour (ResizableWindow::backgroundColourId));
    g.fillRoundedRectangle (panel, 10.0f);

    g.setColour (findColour (ComboBox::outlineColourId));
    g.drawRoundedRectangle (panel, 10.0f, 1.5f);
    g.fillRect (panel.getX() + padding, panel.getY() + headerHeight, panel.getWidth() - 2 * padding, 1.0f);
}

void PluginPicker::resized()
{
    auto panel = getPanelBounds();
    auto header = panel.removeFromTop (headerHeight).reduced (padding, 6);

    closeButton.setBounds (header.removeFromRight (headerButtonWidth));
    header.removeFromRight (padding);
    byTypeButton.setBounds (header.removeFromRight (headerButtonWidth));
    byPluginButton.setBounds (header.removeFromRight (headerButtonWidth));
    header.removeFromRight (padding);

    if (backButton.isVisible())
    {
        backButton.setBounds (header.removeFromLeft (headerButtonWidth));
        header.removeFromLeft (padding);
    }

    titleLabel.setBounds (header);

    viewport.setBounds (panel.reduced (padding, 10));
    emptyLabel.setBounds (viewport.getBounds());
    layoutTiles();
}

void PluginPicker::parentSizeChanged()
{
    if (auto* parent = getParentComponent())
        setBounds (parent->getLocalBounds());
}

void PluginPicker::mouseUp (const MouseEvent& e)
{
    // Tapping outside the panel closes the popup
    if (e.mouseWasClicked() && ! getPanelBounds().contains (e.getPosition()))
        hide();
}

Rectangle<int> PluginPicker::getPanelBounds() const
{
    return getLocalBounds().reduced (margin);
}

void PluginPicker::layoutTiles()
{
    const int numTiles = tiles.size();
    const int visibleHeight = viewport.getHeight();

    const auto getNumColumns = [] (int width) { return jmax (1, (width + tileGap) / (minTileWidth + tileGap)); };
    const auto getHeight = [] (int numRows)   { return numRows * tileHeight + jmax (0, numRows - 1) * tileGap; };

    // As many columns as fit, leaving room for the scrollbar if that's still too many rows to see at once
    int width = viewport.getWidth();
    int numColumns = getNumColumns (width);

    if (getHeight ((numTiles + numColumns - 1) / numColumns) > visibleHeight)
    {
        width -= viewport.getScrollBarThickness() + 4;
        numColumns = getNumColumns (width);
    }

    const int numRows = (numTiles + numColumns - 1) / numColumns;
    const int tileWidth = (width - (numColumns - 1) * tileGap) / numColumns;
    const int usedColumns = jmin (numColumns, numTiles);

    // A page that fits sits in the middle; a longer one starts at the top and scrolls
    const int left = (width - (usedColumns * tileWidth + jmax (0, usedColumns - 1) * tileGap)) / 2;
    const int top = jmax (0, (visibleHeight - getHeight (numRows)) / 2);

    grid.setSize (width, jmax (visibleHeight, getHeight (numRows)));

    for (int i = 0; i < numTiles; ++i)
        tiles[i]->setBounds (left + (i % numColumns) * (tileWidth + tileGap),
                             top + (i / numColumns) * (tileHeight + tileGap),
                             tileWidth, tileHeight);
}

//==============================================================================
void PluginPicker::setGrouping (Grouping grouping)
{
    auto* settings = getAppProperties().getUserSettings();
    settings->setValue (groupingKey, grouping == Grouping::byType ? "byType" : "byPlugin");
    settings->saveIfNeeded();

    pages.clear();
    pages.push_back (grouping == Grouping::byPlugin ? makePluginsPage() : makeTypesPage());
    showCurrentPage();
}

void PluginPicker::openPage (Page page)
{
    pages.back().scrollPosition = viewport.getViewPositionY();
    pages.push_back (std::move (page));
    showCurrentPage();
}

void PluginPicker::goBack()
{
    if (pages.size() > 1)
        pages.pop_back();

    showCurrentPage();
}

void PluginPicker::showCurrentPage()
{
    // The way here, e.g. "Add to chain > Archetype Tim Henson X > Amps"
    StringArray path;

    for (auto& page : pages)
        path.add (page.title);

    titleLabel.setText (path.joinIntoString (String (CharPointer_UTF8 ("  \xe2\x80\xba  "))), dontSendNotification);
    backButton.setVisible (pages.size() > 1);

    tiles.clear();

    for (auto& entry : pages.back().entries)
        grid.addAndMakeVisible (tiles.add (new Tile (entry)));

    emptyLabel.setVisible (tiles.isEmpty());
    resized();
    viewport.setViewPosition (0, pages.back().scrollPosition);
}

//==============================================================================
Array<PluginDescription> PluginPicker::getPluginTypes() const
{
    // Everything that's been scanned, apart from the input and output nodes
    Array<PluginDescription> types;

    for (auto& type : knownPlugins.getTypes())
        if (type.category != "I/O devices")
            types.add (type);

    std::sort (types.begin(), types.end(),
               [] (const PluginDescription& a, const PluginDescription& b) { return a.name.compareNatural (b.name) < 0; });

    return types;
}

const PluginModuleSet* PluginPicker::findModuleSet (const PluginDescription& type) const
{
    // Only plugins split into modules; some definition files only give a plugin its controls
    const auto it = moduleSets.find (type.name);
    return it != moduleSets.end() && it->second.hasModules() ? &it->second : nullptr;
}

PluginPicker::Entry PluginPicker::makeChoiceEntry (const String& name, const String& subtitle, BlockIcon icon, PluginChoice choice)
{
    return { name, subtitle, icon, false, [this, choice] { choose (choice); } };
}

PluginPicker::Entry PluginPicker::makePageEntry (const String& name, const String& subtitle, BlockIcon icon,
                                                 std::function<Page()> makePage)
{
    return { name, subtitle, icon, true, [this, makePage] { openPage (makePage()); } };
}

PluginPicker::Entry PluginPicker::makePluginEntry (const PluginDescription& type)
{
    // A plugin with modules opens a page of them; any other is added as it is
    if (auto* set = findModuleSet (type))
        return makePageEntry (type.name, type.manufacturerName, BlockIcon::suite,
                              [this, type, set] { return makePluginPage (type, *set); });

    return makeChoiceEntry (type.name, type.manufacturerName, BlockIcon::plugin, { PluginDescriptionAndPreference { type } });
}

PluginPicker::Page PluginPicker::makePluginsPage()
{
    Page page { "Add to chain", {} };

    for (auto& type : getPluginTypes())
        page.entries.push_back (makePluginEntry (type));

    return page;
}

PluginPicker::Page PluginPicker::makePluginPage (const PluginDescription& type, const PluginModuleSet& set)
{
    // Its modules, grouped as in its definition file. The whole plugin can't be added, only its parts.
    Page page { type.name, {} };
    StringArray groups;

    for (auto& module : set.getModules())
    {
        if (module.group.isEmpty())
        {
            page.entries.push_back (makeChoiceEntry (module.name, {}, blockIconFromName (module.icon),
                                                     { PluginDescriptionAndPreference { type }, &set, &module }));
        }
        else if (! groups.contains (module.group))
        {
            groups.add (module.group);

            const auto numModules = std::count_if (set.getModules().begin(), set.getModules().end(),
                                                   [&] (const PluginModule& m) { return m.group == module.group; });

            page.entries.push_back (makePageEntry (module.group, describeCount ((size_t) numModules), blockIconFromName (module.icon),
                                                   [this, type, &set, group = module.group] { return makeGroupPage (type, set, group); }));
        }
    }

    return page;
}

PluginPicker::Page PluginPicker::makeGroupPage (const PluginDescription& type, const PluginModuleSet& set, const String& group)
{
    Page page { group, {} };

    for (auto& module : set.getModules())
        if (module.group == group)
            page.entries.push_back (makeChoiceEntry (module.name, {}, blockIconFromName (module.icon),
                                                     { PluginDescriptionAndPreference { type }, &set, &module }));

    return page;
}

PluginPicker::Page PluginPicker::makeTypesPage()
{
    // A page for each kind of block there's at least one of, with plugins that aren't split into modules last
    Page page { "Add to chain", {} };

    for (auto icon : { BlockIcon::amp, BlockIcon::pedal, BlockIcon::cab, BlockIcon::eq,
                       BlockIcon::dynamics, BlockIcon::pitch, BlockIcon::effect, BlockIcon::plugin })
    {
        const auto numEntries = makeTypePage (icon).entries.size();

        if (numEntries > 0)
            page.entries.push_back (makePageEntry (getBlockIconCategory (icon), describeCount (numEntries), icon,
                                                   [this, icon] { return makeTypePage (icon); }));
    }

    return page;
}

PluginPicker::Page PluginPicker::makeTypePage (BlockIcon icon)
{
    Page page { getBlockIconCategory (icon), {} };
    const auto types = getPluginTypes();

    // Plugins that aren't split into modules go with the plain plugin icon, along with any modules
    // that don't say what kind they are. A plugin with modules is only added as its parts.
    if (icon == BlockIcon::plugin)
        for (auto& type : types)
            if (findModuleSet (type) == nullptr)
                page.entries.push_back (makeChoiceEntry (type.name, type.manufacturerName, BlockIcon::plugin,
                                                         { PluginDescriptionAndPreference { type } }));

    // Every plugin's modules of this kind, labelled with the plugin they're from
    for (auto& type : types)
        if (auto* set = findModuleSet (type))
            for (auto& module : set->getModules())
                if (blockIconFromName (module.icon) == icon)
                    page.entries.push_back (makeChoiceEntry (module.name, type.name, icon,
                                                             { PluginDescriptionAndPreference { type }, set, &module }));

    return page;
}

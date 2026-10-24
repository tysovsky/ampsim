#pragma once

#include <JuceHeader.h>
#include "../Plugins/PluginGraph.h"
#include "../Plugins/PluginModules.h"
#include "BlockIcons.h"
#include "TouchLookAndFeel.h"

//==============================================================================
/** What to add from the plugin picker: a whole plugin, or one module of it. */
struct PluginChoice
{
    PluginDescriptionAndPreference description;
    const PluginModuleSet* moduleSet = nullptr;
    const PluginModule* module = nullptr;
};

//==============================================================================
/**
    A popup covering the window for choosing what to add to the chain, as pages
    of large tiles.

    By default it starts with a tile for each plugin. A plugin with modules (one
    with a definition file) opens a page of its groups, such as Amps and Pedals,
    and a group opens a page of its modules; such a plugin is only added as one
    of its modules, never whole. "By type" starts from the kinds of block
    instead, such as every amp from every plugin. It opens either way, as it
    was last left.
*/
class PluginPicker final : public Component
{
public:
    PluginPicker (KnownPluginList&, const std::map<String, PluginModuleSet>& moduleSets);
    ~PluginPicker() override;

    /** Opens over the whole of a component at the first page, by plugin or by type as last left,
        and calls onChosen with what's picked. */
    void show (Component& parent, std::function<void (const PluginChoice&)> onChosen);
    void hide();

    void paint (Graphics&) override;
    void resized() override;
    void parentSizeChanged() override;
    void mouseUp (const MouseEvent&) override;

private:
    class Tile;

    /** A tile: something to add, or another page to open. */
    struct Entry
    {
        String name, subtitle;
        BlockIcon icon = BlockIcon::plugin;
        bool opensPage = false;
        std::function<void()> onTap;
    };

    struct Page
    {
        String title;
        std::vector<Entry> entries;
        int scrollPosition = 0;     // kept while another page is open, for coming back to
    };

    enum class Grouping { byPlugin, byType };

    Array<PluginDescription> getPluginTypes() const;
    const PluginModuleSet* findModuleSet (const PluginDescription&) const;

    Entry makeChoiceEntry (const String& name, const String& subtitle, BlockIcon, PluginChoice);
    Entry makePageEntry (const String& name, const String& subtitle, BlockIcon, std::function<Page()> makePage);
    Entry makePluginEntry (const PluginDescription&);

    Page makePluginsPage();
    Page makePluginPage (const PluginDescription&, const PluginModuleSet&);
    Page makeGroupPage (const PluginDescription&, const PluginModuleSet&, const String& group);
    Page makeTypesPage();
    Page makeTypePage (BlockIcon);

    void setGrouping (Grouping);
    void openPage (Page);
    void goBack();
    void showCurrentPage();
    void choose (const PluginChoice&);
    void layoutTiles();
    Rectangle<int> getPanelBounds() const;

    KnownPluginList& knownPlugins;
    const std::map<String, PluginModuleSet>& moduleSets;
    std::function<void (const PluginChoice&)> onChosen;
    TouchLookAndFeel lookAndFeel;

    Label titleLabel, emptyLabel;
    TextButton backButton, closeButton { "Close" }, byPluginButton { "By plugin" }, byTypeButton { "By type" };
    Viewport viewport;
    Component grid;
    OwnedArray<Tile> tiles;
    std::vector<Page> pages;    // the open page last, after the ones it was opened from

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginPicker)
};

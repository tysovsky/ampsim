/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2022 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 7 End-User License
   Agreement and JUCE Privacy Policy.

   End User License Agreement: www.juce.com/juce-7-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "GraphEditorPanel.h"
#include "../Plugins/InternalPlugins.h"
#include "MainHostWindow.h"
#include "ThemeManager.h"
#include <set>

namespace
{
    // Sizes at full scale; tiles and gaps shrink together when the chain gets too long or tall
    constexpr int tileSize = 120;
    constexpr int gapWidth = 72;
    constexpr int rowGap = 24;
    constexpr int insertButtonSize = 44;
    constexpr int chainMargin = 16;
    constexpr int chainGap = 24;

    // Tiles don't shrink further than this, to stay big enough to tap; chains that still don't fit scroll
    constexpr float minScale = 0.6f;

    // More rows than this won't fit legibly on the 400 px tall screen
    constexpr size_t maxParallelBlocks = 3;

    // A dragged block goes to the + nearest the finger, if it's this close to one
    constexpr float dropDistance = 60.0f;

    // Let go further than this from every tile (and further than dropDistance from every +), a
    // dragged block is deleted
    constexpr int emptySpaceMargin = 24;

    // How long a link's held to remove it, how soon it lights up (not straight away, so a quick tap on
    // a + doesn't flash it), and how close to it a finger has to be
    constexpr int linkHoldMilliseconds = 600;
    constexpr int linkLightUpMilliseconds = 150;
    constexpr float linkTouchDistance = 16.0f;

    // A link past the next column goes this far under the blocks it passes
    constexpr float linkDetourGap = 6.0f;

    // The dots at each end of a tile, where links start and end, and how close to one a touch starts
    // dragging a link out (at least, as the tiles shrink)
    constexpr float linkDotRadius = 6.0f;
    constexpr float minLinkReach = 20.0f;

    // A link being dragged out goes to the block nearest the finger, if it's this close to one
    constexpr float linkDropDistance = 40.0f;

    // Tiles are drawn this far inside their bounds, leaving room for their shadows
    constexpr float tileInset = 4.0f;

    const Colour removingColour { 0xffe0342c };

    template <typename T>
    void addOnce (std::vector<T>& items, T item)
    {
        if (std::find (items.begin(), items.end(), item) == items.end())
            items.push_back (item);
    }
}

//==============================================================================
// A round "+" on the signal path, for inserting a plugin at that point
class GraphEditorPanel::InsertButton final : public Button
{
public:
    explicit InsertButton (GraphEditorPanel& p) : Button ("Insert plugin"), panel (p) {}

    // A + on the main line sits on a link, so holding it holds the link, to remove it
    void mouseDown (const MouseEvent& e) override
    {
        panel.beginHold (getBounds().getCentre().toFloat());
        Button::mouseDown (e);
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() > 5)
            panel.cancelHold();

        Button::mouseDrag (e);
    }

    void mouseUp (const MouseEvent& e) override
    {
        // Not a tap as well, if it removed the link
        if (panel.endHold())
            setState (buttonNormal);

        Button::mouseUp (e);
    }

    /** Lit up while a dragged block would go here if it were let go. */
    void setLit (bool shouldBeLit)
    {
        if (isLit != shouldBeLit)
        {
            isLit = shouldBeLit;
            repaint();
        }
    }

    void paintButton (Graphics& g, bool, bool isButtonDown) override
    {
        auto area = getLocalBounds().toFloat().reduced (2.0f);
        auto colour = findColour (AppColourIds::signalPath);
        const auto background = findColour (ResizableWindow::backgroundColourId);

        // Fill the circle so the signal line doesn't show through it, or with the colour while it's lit
        g.setColour (isLit ? colour : background);
        g.fillEllipse (area);

        g.setColour (isButtonDown ? colour.brighter() : colour);
        g.drawEllipse (area, 2.5f);

        if (isLit)
            g.setColour (background);

        auto plus = area.reduced (area.getWidth() * 0.28f);
        g.fillRect (plus.withSizeKeepingCentre (plus.getWidth(), 3.0f));
        g.fillRect (plus.withSizeKeepingCentre (3.0f, plus.getHeight()));
    }

private:
    GraphEditorPanel& panel;
    bool isLit = false;
};

//==============================================================================
GraphEditorPanel::GraphEditorPanel (PluginGraph& g, KnownPluginList& kpl)
    : graph (g), knownPlugins (kpl)
{
    graph.addChangeListener (this);
    setOpaque (true);

    activeComponent = nullptr;

    // Where a plugin has definitions in more than one folder, the first folder wins
    for (auto& folder : getPluginDefinitionFolders())
        for (auto& [name, set] : PluginModuleSet::loadAll (folder))
            moduleSets.emplace (name, std::move (set));

    picker = std::make_unique<PluginPicker> (knownPlugins, moduleSets);
    chainEndControls = parseXML ("<Controls><Choice param=\"Channel\" label=\"Channel\" style=\"list\"/></Controls>");
}

GraphEditorPanel::~GraphEditorPanel()
{
    graph.removeChangeListener (this);
    draggingConnector = nullptr;
    nodes.clear();
    connectors.clear();
}

void GraphEditorPanel::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

    // Solid where audio is connected, dashed where it isn't (or where nothing is plugged in yet)
    const auto colour = findColour (AppColourIds::signalPath);
    const float dashes[] = { 6.0f, 6.0f };

    const auto drawPath = [&] (Line<float> line, bool connected)
    {
        if (connected)
        {
            g.setColour (colour);
            g.drawLine (line, 3.0f);
        }
        else
        {
            g.setColour (colour.withAlpha (0.6f));
            g.drawDashedLine (line, dashes, numElementsInArray (dashes), 3.0f);
        }
    };

    const auto centreY = [this] (NodeID id)
    {
        return (float) getTileBounds (id).getCentreY();
    };

    for (size_t k = 0; k < chains.size(); ++k)
    {
        const auto& chain = chains[k];
        const auto& columns = chain.columns;

        // A faint line between one chain and the next
        if (k > 0)
        {
            g.setColour (findColour (ComboBox::outlineColourId).withAlpha (0.5f));
            g.fillRect (chainMargin, (chains[k - 1].bottom + chain.top) / 2, getWidth() - 2 * chainMargin, 1);
        }

        // Each column's empty lane, running between the rails either side of it. At either end of the
        // chain there's only a rail on one side, so the lane runs from there to the + under the column.
        for (size_t c = 0; c < columns.size() && c < chain.laneY.size(); ++c)
        {
            if (chain.laneY[c] < 0)
                continue;

            const bool hasRailBefore = c > 0;
            const bool hasRailAfter = c < chain.busX.size();
            const auto y = (float) chain.laneY[c];
            const auto left  = (float) (hasRailBefore ? chain.busX[c - 1] : chain.columnX[c]);
            const auto right = (float) (hasRailAfter  ? chain.busX[c]     : chain.columnX[c]);
            const auto lowestTile = centreY (columns[c].nodes.back());

            if (hasRailBefore)
                drawPath ({ left, lowestTile, left, y }, false);

            drawPath ({ left, y, right, y }, false);

            if (hasRailAfter)
                drawPath ({ right, lowestTile, right, y }, false);
        }

    }

    // Each link, from the block the audio leaves to the block it goes into, with the one being held
    // (to remove it) lit up
    for (auto& link : links)
    {
        const bool isHeld = heldLink.has_value() && heldLink->from == link.from && heldLink->to == link.to;

        g.setColour (isHeld ? removingColour : colour);
        g.strokePath (getLinkPath (link), PathStrokeType (isHeld ? 6.0f : 3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // A dashed outline where a dragged block's tile was
    if (draggedBlock != NodeID())
    {
        Path outline;
        outline.addRoundedRectangle (draggedBlockHome.toFloat().reduced (4.0f), 8.0f);

        Path dashedOutline;
        PathStrokeType (2.0f).createDashedStroke (dashedOutline, outline, dashes, numElementsInArray (dashes));

        g.setColour (colour.withAlpha (0.6f));
        g.fillPath (dashedOutline);
    }
}

void GraphEditorPanel::paintOverChildren (Graphics& g)
{
    const auto colour = findColour (AppColourIds::signalPath);
    const auto background = findColour (ResizableWindow::backgroundColourId);

    // A dot at each end of each tile, where links can be dragged out from, filled where one's linked.
    // Inputs only have an output end, and outputs an input end.
    for (auto& chain : chains)
    {
        for (size_t c = 0; c < chain.columns.size(); ++c)
        {
            for (auto id : chain.columns[c].nodes)
            {
                if (id == draggedBlock)
                    continue;

                for (const bool isOutput : { false, true })
                {
                    if (isOutput ? c + 1 >= chain.columns.size() : c == 0)
                        continue;

                    const bool isLinked = std::any_of (links.begin(), links.end(), [&] (const Link& link)
                    {
                        return isOutput ? link.from == id : link.to == id;
                    });

                    const auto dot = Rectangle<float> (2.0f * linkDotRadius, 2.0f * linkDotRadius).withCentre (getEnd (id, isOutput));

                    g.setColour (isLinked ? colour : background);
                    g.fillEllipse (dot);
                    g.setColour (colour);
                    g.drawEllipse (dot, 2.0f);
                }
            }
        }
    }

    if (! linkDrag.has_value())
        return;

    // While a link's being dragged out, the blocks it can link to, the one it would link to now lit up
    for (auto& chain : chains)
    {
        for (auto& column : chain.columns)
        {
            for (auto id : column.nodes)
            {
                if (! (linkDrag->fromOutput ? canLink (linkDrag->block, id) : canLink (id, linkDrag->block)))
                    continue;

                const bool isTarget = id == linkDrag->target;
                const auto tile = getTileBounds (id).toFloat().reduced (tileInset);

                g.setColour (colour.withAlpha (isTarget ? 0.35f : 0.12f));
                g.fillRoundedRectangle (tile, 8.0f);
                g.setColour (colour);
                g.drawRoundedRectangle (tile, 8.0f, isTarget ? 3.0f : 1.5f);
            }
        }
    }

    // The link itself, from the dot it was dragged out of to the finger, or to the block it would link to
    const auto start = getEnd (linkDrag->block, linkDrag->fromOutput);
    const auto end = linkDrag->target != NodeID() ? getEnd (linkDrag->target, ! linkDrag->fromOutput)
                                                  : linkDrag->finger.toFloat();

    g.setColour (colour);
    g.strokePath (linkDrag->fromOutput ? getLinkPath (start, end) : getLinkPath (end, start),
                  PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

void GraphEditorPanel::mouseDown (const MouseEvent& e)
{
    beginHold (e.position);

    if (e.mods.isPopupMenu())
        showPopupMenu (e.position.toInt());
}

void GraphEditorPanel::mouseUp (const MouseEvent& e)
{
    // A tap on the background clears the selection; a drag is scrolling
    if (! endHold() && e.mouseWasClicked())
        activeComponentChanged(nullptr);
}

void GraphEditorPanel::mouseDrag (const MouseEvent& e)
{
    // Moving lets go of a link, as it's scrolling
    if (e.getDistanceFromDragStart() > 5)
        cancelHold();
}

void GraphEditorPanel::beginHold (Point<float> position)
{
    cancelHold();
    pendingLink = findLinkAt (position);

    if (pendingLink.has_value())
        startTimer (linkLightUpMilliseconds);
}

void GraphEditorPanel::cancelHold()
{
    stopTimer();
    pendingLink.reset();
    removedHeldLink = false;

    if (heldLink.has_value())
    {
        heldLink.reset();
        repaint();
    }
}

bool GraphEditorPanel::endHold()
{
    const bool removed = removedHeldLink;
    cancelHold();
    return removed;
}

void GraphEditorPanel::createNewPlugin (const PluginDescriptionAndPreference& desc, Point<int> position)
{
    // The chain the position is in, or the nearest one, then its nearest rail
    const Chain* nearestChain = nullptr;
    int nearestChainDistance = std::numeric_limits<int>::max();

    for (auto& chain : chains)
    {
        const int distance = position.y < chain.top ? chain.top - position.y : jmax (0, position.y - chain.bottom);

        if (distance < nearestChainDistance)
        {
            nearestChain = &chain;
            nearestChainDistance = distance;
        }
    }

    if (nearestChain == nullptr || nearestChain->busX.empty())
        return;

    size_t nearestGap = 0;
    int nearestDistance = std::numeric_limits<int>::max();

    for (size_t gap = 0; gap < nearestChain->busX.size(); ++gap)
    {
        const int distance = std::abs (nearestChain->busX[gap] - position.x);

        if (distance < nearestDistance)
        {
            nearestGap = gap;
            nearestDistance = distance;
        }
    }

    insertPlugin ({ desc }, getSeriesInsertPoint (*nearestChain, nearestGap));
}

// For a module, switches the new plugin to that module and labels its node with the module's name
static std::function<void (AudioProcessorGraph::Node&)> getModuleSetUp (const PluginModuleSet* moduleSet, const PluginModule* module)
{
    if (moduleSet == nullptr || module == nullptr)
        return nullptr;

    // Copies, so they stay valid however long the plugin takes to load
    return [set = *moduleSet, module = *module] (AudioProcessorGraph::Node& node)
    {
        set.apply (*node.getProcessor(), module);
        node.properties.set ("module", module.name);
    };
}

GraphEditorPanel::InsertPoint GraphEditorPanel::getSeriesInsertPoint (const Chain& chain, size_t gap) const
{
    // A new column halfway between its neighbours' positions, so it sorts between them. It goes into
    // the links from the column before it straight to the column after; with none, it's between
    // every block either side.
    const auto& columns = chain.columns;
    const auto isIn = [] (const Column& column, NodeID id)
    {
        return std::find (column.nodes.begin(), column.nodes.end(), id) != column.nodes.end();
    };

    std::vector<NodeID> sources, destinations;

    for (auto& link : links)
    {
        if (isIn (columns[gap], link.from) && isIn (columns[gap + 1], link.to))
        {
            addOnce (sources, link.source);
            addOnce (destinations, link.to);
        }
    }

    if (sources.empty())
    {
        sources = columns[gap].getOutputs();
        destinations = columns[gap + 1].getInputs();
    }

    return { chain.id, sources, destinations,
             { (columns[gap].position + columns[gap + 1].position) / 2.0, 0.0 }, true };
}

GraphEditorPanel::InsertPoint GraphEditorPanel::getParallelInsertPoint (const Chain& chain, size_t column) const
{
    // Same x as the rest of the column, and a row below its lowest block. An input only feeds the
    // column after it, and an output is only fed by the one before.
    const auto& columns = chain.columns;
    const auto row = graph.getNodePosition (columns[column].nodes.back()).y + 1.0;

    return { chain.id,
             column > 0 ? columns[column - 1].getOutputs() : std::vector<NodeID>(),
             column + 1 < columns.size() ? columns[column + 1].getInputs() : std::vector<NodeID>(),
             { columns[column].position, row }, false };
}

void GraphEditorPanel::insertPlugin (const PluginChoice& choice, const InsertPoint& point)
{
    const auto moduleSetUp = getModuleSetUp (choice.moduleSet, choice.module);

    graph.insertPlugin (choice.description, point.sources, point.destinations, point.position, point.inSeries,
                        [moduleSetUp, chain = point.chain] (AudioProcessorGraph::Node& node)
                        {
                            node.properties.set ("chain", chain);

                            if (moduleSetUp != nullptr)
                                moduleSetUp (node);
                        });
}

void GraphEditorPanel::addChain()
{
    // The next chain number, and the first input channel that no chain is using yet
    int id = 0;
    std::set<int> usedChannels;

    for (auto& chain : chains)
    {
        id = jmax (id, chain.id + 1);

        for (auto& column : chain.columns)
            for (auto node : column.nodes)
                if (auto* input = dynamic_cast<ChainInputProcessor*> (getChainEnd (node)))
                    usedChannels.insert (input->getChosenChannels().first);
    }

    int channel = 0;

    while (usedChannels.count (channel) > 0 && channel + 1 < ChainEndProcessor::maxChannels)
        ++channel;

    const auto input = graph.addChainEnd (true, id, { 0.0, 0.0 }, ChainEndProcessor::getChoiceFor (channel, false), {}, {});
    graph.addChainEnd (false, id, { 1.0, 0.0 }, ChainEndProcessor::getChoiceFor (0, true), { input }, {});
}

void GraphEditorPanel::addParallelEnd (const Chain& chain, size_t column)
{
    // The channel after the column's last one, so Input 1 is followed by Input 2, and Output 1+2 by Output 3+4
    int choice = 0;

    if (auto* last = getChainEnd (chain.columns[column].nodes.back()))
    {
        const auto [first, second] = last->getChosenChannels();
        const bool isPair = second >= 0;
        const int next = first + (isPair ? 2 : 1);

        choice = ChainEndProcessor::getChoiceFor (next < ChainEndProcessor::maxChannels ? next : 0, isPair);
    }

    const auto point = getParallelInsertPoint (chain, column);
    graph.addChainEnd (column == 0, point.chain, point.position, choice, point.sources, point.destinations);
}

void GraphEditorPanel::removeFromChain (NodeID nodeID)
{
    for (auto& chain : chains)
    {
        const auto& columns = chain.columns;

        for (size_t c = 0; c < columns.size(); ++c)
        {
            const auto& column = columns[c];
            const auto it = std::find (column.nodes.begin(), column.nodes.end(), nodeID);

            if (it == column.nodes.end())
                continue;

            // An input or output just goes, unless it's the chain's last one, which takes its chain with it
            if (isEndColumn (column))
            {
                std::vector<NodeID> toRemove;

                if (column.nodes.size() > 1)
                {
                    toRemove.push_back (nodeID);
                }
                else
                {
                    for (auto& each : columns)
                    {
                        toRemove.insert (toRemove.end(), each.nodes.begin(), each.nodes.end());

                        for (auto level : each.levels)
                            if (level != NodeID())
                                toRemove.push_back (level);
                    }
                }

                graph.removeFromChain (toRemove, {}, {}, false);
                return;
            }

            if (c == 0 || c + 1 >= columns.size())
                return;

            const auto index = (size_t) std::distance (column.nodes.begin(), it);
            std::vector<NodeID> toRemove { nodeID };

            if (column.levels[index] != NodeID())
                toRemove.push_back (column.levels[index]);

            // When one of two parallel plugins goes, the other is on its own again, so put its level
            // back to full rather than leave it quietly turned down with no mix bar to show it
            if (column.nodes.size() == 2)
                if (auto* remaining = graph.graph.getNodeForId (column.levels[1 - index]))
                    if (auto* level = dynamic_cast<LaneLevelProcessor*> (remaining->getProcessor()))
                        level->getLevel().setValueNotifyingHost (1.0f);

            // What fed it then feeds what it fed, where that isn't reached some other way, so the signal
            // still flows
            const auto [sources, destinations] = getNeighbours (nodeID);
            graph.removeFromChain (toRemove, sources, destinations, true);
            return;
        }
    }
}

bool GraphEditorPanel::removesWholeChain (NodeID nodeID) const
{
    for (auto& chain : chains)
        for (auto& column : chain.columns)
            if (std::find (column.nodes.begin(), column.nodes.end(), nodeID) != column.nodes.end())
                return isEndColumn (column) && column.nodes.size() == 1;

    return false;
}

BlockIcon GraphEditorPanel::getBlockIcon (AudioProcessorGraph::NodeID nodeID) const
{
    auto* node = graph.graph.getNodeForId (nodeID);

    if (node == nullptr)
        return BlockIcon::plugin;

    if (dynamic_cast<ChainInputProcessor*> (node->getProcessor()) != nullptr)
        return BlockIcon::input;

    if (dynamic_cast<ChainOutputProcessor*> (node->getProcessor()) != nullptr)
        return BlockIcon::output;

    auto* set = findModuleSet (*node);

    if (set == nullptr)
        return BlockIcon::plugin;

    // A plugin that isn't split into modules may say what kind of block it is (e.g. a synth)
    if (! set->hasModules())
        return blockIconFromName (set->getPluginIcon());

    // One of a plugin's modules shows what kind of block it is; the whole plugin shows it's a suite
    const auto moduleName = node->properties["module"].toString();

    if (moduleName.isEmpty())
        return BlockIcon::suite;

    if (auto* module = set->findModule (moduleName))
        return blockIconFromName (module->icon);

    return BlockIcon::plugin;
}

const XmlElement* GraphEditorPanel::getBlockControls (AudioProcessorGraph::NodeID nodeID) const
{
    // A chain's inputs and outputs have the one control: which channel
    if (getChainEnd (nodeID) != nullptr)
        return chainEndControls.get();

    auto* node = graph.graph.getNodeForId (nodeID);
    auto* set = node != nullptr ? findModuleSet (*node) : nullptr;

    if (set == nullptr)
        return nullptr;

    const auto moduleName = node->properties["module"].toString();

    if (moduleName.isEmpty())
        return set->getPluginControls();

    auto* module = set->findModule (moduleName);
    return module != nullptr ? module->controls.get() : nullptr;
}

const XmlElement* GraphEditorPanel::getBlockEditor (AudioProcessorGraph::NodeID nodeID) const
{
    // Only whole plugins have editors so far, not the modules of a plugin
    auto* node = graph.graph.getNodeForId (nodeID);
    auto* set = node != nullptr ? findModuleSet (*node) : nullptr;

    if (set == nullptr || node->properties["module"].toString().isNotEmpty())
        return nullptr;

    return set->getPluginEditor();
}

AudioProcessorGraph::Node::Ptr GraphEditorPanel::getLevelStage (AudioProcessorGraph::NodeID nodeID) const
{
    if (const auto place = findBlock (nodeID))
    {
        const auto& column = chains[place->chain].columns[place->column];

        if (place->row < column.levels.size())
            return graph.graph.getNodeForId (column.levels[place->row]);
    }

    return nullptr;
}

ChainEndProcessor* GraphEditorPanel::getChainEnd (NodeID nodeID) const
{
    auto* node = graph.graph.getNodeForId (nodeID);
    return node != nullptr ? dynamic_cast<ChainEndProcessor*> (node->getProcessor()) : nullptr;
}

const PluginModuleSet* GraphEditorPanel::findModuleSet (const AudioProcessorGraph::Node& node) const
{
    auto* plugin = dynamic_cast<AudioPluginInstance*> (node.getProcessor());
    const auto it = plugin != nullptr ? moduleSets.find (plugin->getPluginDescription().name) : moduleSets.end();

    return it != moduleSets.end() ? &it->second : nullptr;
}

PluginComponent* GraphEditorPanel::getComponentForPlugin (AudioProcessorGraph::NodeID nodeID) const
{
    for (auto* fc : nodes)
       if (fc->pluginID == nodeID)
            return fc;

    return nullptr;
}

ConnectorComponent* GraphEditorPanel::getComponentForConnection (const AudioProcessorGraph::Connection& conn) const
{
    for (auto* cc : connectors)
        if (cc->connection == conn)
            return cc;

    return nullptr;
}

PinComponent* GraphEditorPanel::findPinAt (Point<float> pos) const
{
    for (auto* fc : nodes)
    {
        // NB: A Visual Studio optimiser error means we have to put this Component* in a local
        // variable before trying to cast it, or it gets mysteriously optimised away..
        auto* comp = fc->getComponentAt (pos.toInt() - fc->getPosition());

        if (auto* pin = dynamic_cast<PinComponent*> (comp))
            return pin;
    }

    return nullptr;
}

void GraphEditorPanel::resized()
{
    updateComponents();
}

void GraphEditorPanel::changeListenerCallback (ChangeBroadcaster*)
{
    updateComponents();
}

void GraphEditorPanel::updateComponents()
{
    // Telling the listeners a tile has gone can resize this panel (the selected block's controls go
    // with it), and resizing updates the components, as does laying out the chains when they need
    // more room. Updating in the middle of an update would change the tiles under the loops going
    // through them, so it's done again once this one has finished instead.
    if (isUpdatingComponents)
    {
        needsAnotherUpdate = true;
        return;
    }

    const ScopedValueSetter<bool> updating (isUpdatingComponents, true);

    do
    {
        needsAnotherUpdate = false;

        addChainEndsToOldGraph();

        // The device may have more channels than when the inputs and outputs were added
        graph.connectDeviceChannels();

        // The tiles whose nodes have gone, found before any are removed
        Array<PluginComponent*> gone;

        for (auto* fc : nodes)
            if (graph.graph.getNodeForId (fc->pluginID) == nullptr)
                gone.add (fc);

        for (auto* fc : gone)
        {
            Component::BailOutChecker checker (this);
            listeners.callChecked (checker, &Listener::onNodeDeleted, fc);

            if (activeComponent == fc)
                activeComponent = nullptr;

            nodes.removeObject (fc);
        }

        for (auto* fc : nodes)
            fc->update();

        for (auto* f : graph.graph.getNodes())
        {
            if (getComponentForPlugin (f->nodeID) == nullptr)
            {
                auto* comp = nodes.add (new PluginComponent (*this, f->nodeID));
                addAndMakeVisible (comp);
                comp->update();
            }
        }

        layoutChains();
    }
    while (needsAnotherUpdate);

    listeners.call ([] (Listener& l) { l.onComponentsUpdated(); });
}

std::vector<GraphEditorPanel::Chain> GraphEditorPanel::findChains() const
{
    // Every block belongs to a chain, by its "chain" property. Within a chain, the blocks are ordered
    // by their stored x: the inputs are at 0, the outputs at 1, and the plugins in between. Blocks with
    // the same x run in parallel, top to bottom by their stored y. The order doesn't come from the
    // connections, so a block stays in place even when no audio reaches it (e.g. with no input
    // device). The audio device's own nodes, and the MIDI ones, aren't blocks.
    using Key = std::tuple<int, double, double>;    // chain, x, y

    const auto keyOf = [] (const AudioProcessorGraph::Node* node)
    {
        return Key { (int) node->properties["chain"], (double) node->properties["x"], (double) node->properties["y"] };
    };

    std::map<int, std::vector<AudioProcessorGraph::Node*>> blocksByChain;
    std::map<Key, NodeID> levelsByPosition;

    for (auto* node : graph.graph.getNodes())
    {
        // Level stages aren't shown; each one belongs to the plugin at the same position
        if (dynamic_cast<LaneLevelProcessor*> (node->getProcessor()) != nullptr)
            levelsByPosition[keyOf (node)] = node->nodeID;
        else if (dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor*> (node->getProcessor()) == nullptr)
            blocksByChain[(int) node->properties["chain"]].push_back (node);
    }

    std::vector<Chain> result;

    for (auto& [id, blocks] : blocksByChain)
    {
        std::sort (blocks.begin(), blocks.end(), [] (const auto* a, const auto* b)
        {
            const auto ax = (double) a->properties["x"], bx = (double) b->properties["x"];
            const auto ay = (double) a->properties["y"], by = (double) b->properties["y"];

            if (ax != bx)   return ax < bx;
            if (ay != by)   return ay < by;
            return a->nodeID.uid < b->nodeID.uid;
        });

        Chain chain;
        chain.id = id;

        for (auto* node : blocks)
        {
            const auto x = (double) node->properties["x"];
            const auto levelIt = levelsByPosition.find (keyOf (node));
            const auto level = levelIt != levelsByPosition.end() ? levelIt->second : NodeID();

            if (! chain.columns.empty() && chain.columns.back().position == x)
            {
                chain.columns.back().nodes.push_back (node->nodeID);
                chain.columns.back().levels.push_back (level);
            }
            else
            {
                chain.columns.push_back ({ { node->nodeID }, x, { level } });
            }
        }

        result.push_back (std::move (chain));
    }

    return result;
}

void GraphEditorPanel::addChainEndsToOldGraph()
{
    for (auto* node : graph.graph.getNodes())
        if (dynamic_cast<ChainEndProcessor*> (node->getProcessor()) != nullptr)
            return;

    // Before chains had inputs and outputs of their own, there was one chain, wired straight to the
    // audio device's nodes, which were where its inputs and outputs are now. So the plugins become
    // that chain's, between an input and an output, or a new graph gets a chain with nothing in it.
    graph.disconnectAudioDevice();

    const auto oldChains = findChains();
    std::vector<NodeID> firstInputs, lastOutputs;

    if (! oldChains.empty() && ! oldChains.front().columns.empty())
    {
        firstInputs = oldChains.front().columns.front().getInputs();
        lastOutputs = oldChains.front().columns.back().getOutputs();
    }

    const auto input = graph.addChainEnd (true, 0, { 0.0, 0.0 }, ChainEndProcessor::getChoiceFor (0, false), {}, firstInputs);

    graph.addChainEnd (false, 0, { 1.0, 0.0 }, ChainEndProcessor::getChoiceFor (0, true),
                       firstInputs.empty() ? std::vector<NodeID> { input } : lastOutputs, {});
}

std::vector<GraphEditorPanel::NodeID> GraphEditorPanel::Column::getOutputs() const
{
    auto outputs = nodes;

    for (size_t i = 0; i < outputs.size() && i < levels.size(); ++i)
        if (levels[i] != NodeID())
            outputs[i] = levels[i];

    return outputs;
}

bool GraphEditorPanel::isEndColumn (const Column& column) const
{
    return ! column.nodes.empty() && getChainEnd (column.nodes.front()) != nullptr;
}

std::vector<GraphEditorPanel::Link> GraphEditorPanel::findLinks() const
{
    // Each block in the chains, by the nodes its audio goes in at (the block) and leaves from (its
    // level stage, or the block itself)
    std::map<NodeID, NodeID> blockOf;

    for (auto& chain : chains)
    {
        for (auto& column : chain.columns)
        {
            for (size_t row = 0; row < column.nodes.size(); ++row)
            {
                blockOf[column.nodes[row]] = column.nodes[row];

                if (row < column.levels.size() && column.levels[row] != NodeID())
                    blockOf[column.levels[row]] = column.nodes[row];
            }
        }
    }

    // A link for each pair of blocks with audio between them, however many channels it has. A plugin
    // into its own level stage isn't one, and nor is the audio device into the inputs or from the outputs.
    std::vector<Link> result;

    for (auto& connection : graph.graph.getConnections())
    {
        if (connection.source.isMIDI())
            continue;

        const auto from = blockOf.find (connection.source.nodeID);
        const auto to = blockOf.find (connection.destination.nodeID);

        if (from == blockOf.end() || to == blockOf.end() || from->second == to->second
             || connection.destination.nodeID != to->second)
            continue;

        const auto isSame = [&] (const Link& link) { return link.from == from->second && link.to == to->second; };

        if (std::none_of (result.begin(), result.end(), isSame))
            result.push_back ({ from->second, to->second, connection.source.nodeID });
    }

    return result;
}

GraphEditorPanel::NodeID GraphEditorPanel::getOutputOf (NodeID block) const
{
    if (const auto place = findBlock (block))
    {
        const auto& column = chains[place->chain].columns[place->column];

        if (place->row < column.levels.size() && column.levels[place->row] != NodeID())
            return column.levels[place->row];
    }

    return block;
}

std::pair<std::vector<GraphEditorPanel::NodeID>, std::vector<GraphEditorPanel::NodeID>>
    GraphEditorPanel::getNeighbours (NodeID block) const
{
    // What feeds the block, and what its audio goes on to, apart from its own level stage
    const auto output = getOutputOf (block);
    std::vector<NodeID> sources, destinations;

    for (auto& connection : graph.graph.getConnections())
    {
        if (connection.source.isMIDI())
            continue;

        if (connection.destination.nodeID == block && connection.source.nodeID != output)
            addOnce (sources, connection.source.nodeID);

        if (connection.source.nodeID == output && connection.destination.nodeID != block)
            addOnce (destinations, connection.destination.nodeID);
    }

    return { sources, destinations };
}

bool GraphEditorPanel::canLink (NodeID from, NodeID to) const
{
    // Left to right within a chain, so the inputs only feed and the outputs are only fed, and only once
    const auto a = findBlock (from);
    const auto b = findBlock (to);

    if (! a.has_value() || ! b.has_value() || a->chain != b->chain || a->column >= b->column)
        return false;

    if (std::any_of (links.begin(), links.end(), [&] (const Link& link) { return link.from == from && link.to == to; }))
        return false;

    // Never round in a loop
    return ! graph.graph.isAnInputTo (to, getOutputOf (from));
}

Point<float> GraphEditorPanel::getEnd (NodeID block, bool isOutput) const
{
    // Halfway up the tile's right-hand side, where its audio leaves, or its left, where it comes in
    const auto tile = getTileBounds (block).toFloat().reduced (tileInset);
    return { isOutput ? tile.getRight() : tile.getX(), tile.getCentreY() };
}

Path GraphEditorPanel::getLinkPath (Point<float> start, Point<float> end)
{
    // Leaving and arriving level, so a link between blocks in the same row is a straight line
    const auto bend = jmax (24.0f, (end.x - start.x) * 0.5f);

    Path path;
    path.startNewSubPath (start);
    path.cubicTo (start.translated (bend, 0.0f), end.translated (-bend, 0.0f), end);
    return path;
}

Path GraphEditorPanel::getLinkPath (const Link& link) const
{
    const auto start = getEnd (link.from, true);
    const auto end = getEnd (link.to, false);
    const auto a = findBlock (link.from);
    const auto b = findBlock (link.to);

    // To the next column, it goes straight across
    if (! a.has_value() || ! b.has_value() || b->column <= a->column + 1)
        return getLinkPath (start, end);

    // Further, it goes under the blocks in between rather than behind them: down in the gap after the
    // first block, along under the others, and up in the gap before the second
    const auto& chain = chains[a->chain];
    auto under = jmax (start.y, end.y);

    for (auto c = a->column + 1; c < b->column; ++c)
        under = jmax (under, (float) getTileBounds (chain.columns[c].nodes.back()).getBottom() + linkDetourGap);

    const Point<float> down { (float) chain.busX[a->column], under };
    const Point<float> up { (float) chain.busX[b->column - 1], under };
    const auto downBend = (start.x + down.x) / 2.0f;
    const auto upBend = (up.x + end.x) / 2.0f;

    Path path;
    path.startNewSubPath (start);
    path.cubicTo ({ downBend, start.y }, { downBend, under }, down);
    path.lineTo (up);
    path.cubicTo ({ upBend, under }, { upBend, end.y }, end);
    return path;
}

std::optional<GraphEditorPanel::Link> GraphEditorPanel::findLinkAt (Point<float> position) const
{
    std::optional<Link> nearest;
    auto nearestDistance = linkTouchDistance;

    for (auto& link : links)
    {
        Point<float> pointOnLink;
        getLinkPath (link).getNearestPoint (position, pointOnLink);

        if (const auto distance = pointOnLink.getDistanceFrom (position); distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = link;
        }
    }

    return nearest;
}

bool GraphEditorPanel::beginLink (PluginComponent& tile, Point<int> positionOnTile)
{
    const auto place = findBlock (tile.pluginID);

    if (! place.has_value())
        return false;

    const auto numColumns = chains[place->chain].columns.size();
    const auto touch = getLocalPoint (&tile, positionOnTile).toFloat();
    const auto reach = jmax (minLinkReach, (float) tile.getWidth() * 0.22f);

    for (const bool isOutput : { true, false })
    {
        const bool hasEnd = isOutput ? place->column + 1 < numColumns : place->column > 0;

        if (hasEnd && touch.getDistanceFrom (getEnd (tile.pluginID, isOutput)) <= reach)
        {
            linkDrag = LinkDrag { tile.pluginID, isOutput, touch.toInt(), {} };
            repaint();
            return true;
        }
    }

    return false;
}

void GraphEditorPanel::dragLink (Point<int> finger)
{
    if (! linkDrag.has_value())
        return;

    linkDrag->finger = finger;
    linkDrag->target = {};

    // The block under the finger, or else the nearest one close to it
    auto nearestDistance = linkDropDistance;

    for (auto& chain : chains)
    {
        for (auto& column : chain.columns)
        {
            for (auto id : column.nodes)
            {
                if (! (linkDrag->fromOutput ? canLink (linkDrag->block, id) : canLink (id, linkDrag->block)))
                    continue;

                const auto tile = getTileBounds (id).toFloat();
                const auto distance = tile.getConstrainedPoint (finger.toFloat()).getDistanceFrom (finger.toFloat());

                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    linkDrag->target = id;
                }
            }
        }
    }

    repaint();
}

void GraphEditorPanel::endLink()
{
    if (linkDrag.has_value() && linkDrag->target != NodeID())
    {
        const auto from = linkDrag->fromOutput ? linkDrag->block : linkDrag->target;
        const auto to = linkDrag->fromOutput ? linkDrag->target : linkDrag->block;

        graph.connectBlocks (getOutputOf (from), to);
    }

    linkDrag.reset();
    repaint();
}

bool GraphEditorPanel::canAddParallel (const Chain& chain, size_t column) const
{
    // Any column, inputs and outputs included, as long as there's a column for it to connect to
    return chain.columns.size() >= 2 && chain.columns[column].nodes.size() < maxParallelBlocks;
}

void GraphEditorPanel::layoutChains()
{
    chains = findChains();
    links = findLinks();

    // Hide the nodes that aren't part of a chain (the audio device's, and the MIDI ones)
    std::set<uint32> blocksInChains;

    for (auto& chain : chains)
        for (auto& column : chain.columns)
            for (auto id : column.nodes)
                blocksInChains.insert (id.uid);

    for (auto* fc : nodes)
        fc->setVisible (blocksInChains.count (fc->pluginID.uid) > 0);

    // A + in each gap of each chain, and one under each column, shown while it has room for more
    seriesTargets.clear();
    parallelTargets.clear();

    for (size_t k = 0; k < chains.size(); ++k)
    {
        for (size_t gap = 0; gap + 1 < chains[k].columns.size(); ++gap)
            seriesTargets.push_back ({ k, gap });

        for (size_t c = 0; c < chains[k].columns.size(); ++c)
            parallelTargets.push_back ({ k, c });
    }

    const auto resizeButtons = [this] (OwnedArray<InsertButton>& buttons, size_t size, std::function<void (size_t)> onClick)
    {
        while ((size_t) buttons.size() < size)
        {
            auto* button = buttons.add (new InsertButton (*this));
            const auto index = (size_t) buttons.size() - 1;
            button->onClick = [onClick, index] { onClick (index); };
            addChildComponent (button);
        }

        while ((size_t) buttons.size() > size)
            buttons.removeLast();
    };

    // Where a plugin goes is worked out when the + is tapped, as the chains may have changed by the
    // time one's been picked
    resizeButtons (seriesButtons, seriesTargets.size(), [this] (size_t index)
    {
        const auto [chain, gap] = seriesTargets[index];

        showPluginPicker ([this, point = getSeriesInsertPoint (chains[chain], gap)] (const PluginChoice& choice)
        {
            insertPlugin (choice, point);
        });
    });

    resizeButtons (parallelButtons, parallelTargets.size(), [this] (size_t index)
    {
        const auto [chain, column] = parallelTargets[index];

        // Another input or output is added straight away, and its channel picked on its own controls
        if (isEndColumn (chains[chain].columns[column]))
        {
            addParallelEnd (chains[chain], column);
            return;
        }

        showPluginPicker ([this, point = getParallelInsertPoint (chains[chain], column)] (const PluginChoice& choice)
        {
            insertPlugin (choice, point);
        });
    });

    // Each chain's height at full size: its tallest column, with the lane under it if it has room for more
    std::vector<int> chainHeights;
    int fullWidth = tileSize, fullHeight = 0;

    for (auto& chain : chains)
    {
        int height = tileSize;

        for (size_t c = 0; c < chain.columns.size(); ++c)
        {
            const int numRows = (int) chain.columns[c].nodes.size();
            int columnHeight = numRows * tileSize + (numRows - 1) * rowGap;

            if (canAddParallel (chain, c))
                columnHeight += rowGap + insertButtonSize;

            height = jmax (height, columnHeight);
        }

        const int numColumns = (int) chain.columns.size();
        fullWidth = jmax (fullWidth, numColumns * tileSize + jmax (0, numColumns - 1) * gapWidth);
        fullHeight += height;
        chainHeights.push_back (height);
    }

    fullHeight += jmax (0, (int) chains.size() - 1) * chainGap;

    // Shrink everything together when the chains are too long or too tall for the space they're shown
    // in, down to a size that can still be tapped. Past that, this panel grows bigger than the space,
    // which is a viewport, to scroll.
    const auto visible = [this]
    {
        if (auto* viewport = findParentComponentOfClass<Viewport>())
            return Point<int> { viewport->getWidth(), viewport->getHeight() };

        return Point<int> { getWidth(), getHeight() };
    }();

    const float scale = jlimit (minScale, 1.0f,
                                jmin ((float) (visible.x - 2 * chainMargin) / (float) fullWidth,
                                      (float) (visible.y - 2 * chainMargin) / (float) jmax (1, fullHeight)));
    const int scaledWidth = roundToInt ((float) fullWidth * scale);
    const int scaledHeight = roundToInt ((float) fullHeight * scale);
    const Point<int> size { jmax (visible.x, scaledWidth + 2 * chainMargin), jmax (visible.y, scaledHeight + 2 * chainMargin) };

    // Resizing lays it all out again
    if (getWidth() != size.x || getHeight() != size.y)
    {
        setSize (size.x, size.y);
        return;
    }

    const int tile = roundToInt ((float) tileSize * scale);
    const int gap = roundToInt ((float) gapWidth * scale);
    const int rowSpacing = roundToInt ((float) rowGap * scale);
    const int chainSpacing = roundToInt ((float) chainGap * scale);

    // Smaller too, so the rails either side of a + still show
    const int buttonSize = jmax (28, jmin (insertButtonSize, gap - 16));

    // The chains go top to bottom, all starting at the same x so their inputs line up
    const int left = (getWidth() - scaledWidth) / 2;
    int top = (getHeight() - scaledHeight) / 2;
    int seriesIndex = 0, parallelIndex = 0;

    for (size_t k = 0; k < chains.size(); ++k)
    {
        auto& chain = chains[k];
        const int numColumns = (int) chain.columns.size();
        const int numGaps = jmax (0, numColumns - 1);
        int x = left;

        chain.top = top;
        chain.busX.assign ((size_t) numGaps, 0);
        chain.laneY.assign ((size_t) numColumns, -1);
        chain.columnX.assign ((size_t) numColumns, 0);

        for (int c = 0; c < numColumns; ++c)
        {
            const auto& column = chain.columns[(size_t) c];

            // Plugins sharing a column get a mix bar for their lane's level
            const bool isParallel = column.nodes.size() > 1;

            for (size_t row = 0; row < column.nodes.size(); ++row)
            {
                if (auto* comp = getComponentForPlugin (column.nodes[row]))
                {
                    const Rectangle<int> bounds (x, top + (int) row * (tile + rowSpacing), tile, tile);

                    // A tile that's being dragged stays under the finger
                    if (column.nodes[row] == draggedBlock)
                        draggedBlockHome = bounds;
                    else
                        comp->setBounds (bounds);

                    comp->setLaneLevel (isParallel ? graph.graph.getNodeForId (column.levels[row]) : nullptr);
                }
            }

            chain.columnX[(size_t) c] = x + tile / 2;

            auto* laneButton = parallelButtons[parallelIndex++];
            const bool hasLane = canAddParallel (chain, (size_t) c);
            laneButton->setVisible (hasLane);

            if (hasLane)
            {
                const int laneTop = top + (int) column.nodes.size() * (tile + rowSpacing);
                chain.laneY[(size_t) c] = laneTop + buttonSize / 2;
                laneButton->setBounds (Rectangle<int> (x, laneTop, tile, buttonSize).withSizeKeepingCentre (buttonSize, buttonSize));
            }
            else
            {
                // Out of the way, so where it last was doesn't count as somewhere (see isEmptySpace())
                laneButton->setBounds ({});
            }

            x += tile;

            if (c < numGaps)
            {
                auto* seriesButton = seriesButtons[seriesIndex++];
                chain.busX[(size_t) c] = x + gap / 2;
                seriesButton->setBounds (Rectangle<int> (x, top, gap, tile).withSizeKeepingCentre (buttonSize, buttonSize));
                seriesButton->setVisible (true);
                x += gap;
            }
        }

        top += roundToInt ((float) chainHeights[k] * scale);
        chain.bottom = top;
        top += chainSpacing;
    }

    if (draggedBlock != NodeID())
    {
        showDropTargets (draggedBlock, litTarget);
    }
    else
    {
        for (auto* button : seriesButtons)     button->setLit (false);
        for (auto* button : parallelButtons)   button->setLit (false);
    }

    repaint();
}

//==============================================================================
std::optional<GraphEditorPanel::BlockPlace> GraphEditorPanel::findBlock (NodeID nodeID) const
{
    for (size_t k = 0; k < chains.size(); ++k)
    {
        for (size_t c = 0; c < chains[k].columns.size(); ++c)
        {
            const auto& blocks = chains[k].columns[c].nodes;
            const auto it = std::find (blocks.begin(), blocks.end(), nodeID);

            if (it != blocks.end())
                return BlockPlace { k, c, (size_t) std::distance (blocks.begin(), it) };
        }
    }

    return {};
}

bool GraphEditorPanel::canMoveBlock (NodeID nodeID) const
{
    return findBlock (nodeID).has_value() && getChainEnd (nodeID) == nullptr;
}

std::vector<GraphEditorPanel::DropTarget> GraphEditorPanel::getDropTargets() const
{
    std::vector<DropTarget> targets;

    for (auto [chain, gap] : seriesTargets)
        targets.push_back ({ chain, gap, true });

    for (auto [chain, column] : parallelTargets)
        targets.push_back ({ chain, column, false });

    return targets;
}

GraphEditorPanel::InsertButton* GraphEditorPanel::getButtonFor (const DropTarget& target) const
{
    const auto& targets = target.inSeries ? seriesTargets : parallelTargets;
    const auto& buttons = target.inSeries ? seriesButtons : parallelButtons;
    const auto it = std::find (targets.begin(), targets.end(), std::make_pair (target.chain, target.index));

    return it != targets.end() ? buttons[(int) std::distance (targets.begin(), it)] : nullptr;
}

bool GraphEditorPanel::canDrop (NodeID nodeID, const DropTarget& target) const
{
    const auto place = findBlock (nodeID);

    if (! place.has_value() || target.chain >= chains.size())
        return false;

    const auto& chain = chains[target.chain];
    const bool isSameChain = target.chain == place->chain;
    const bool isAlone = chains[place->chain].columns[place->column].nodes.size() == 1;

    // In series anywhere, except either side of a block that's on its own, which is where it is already
    if (target.inSeries)
        return target.index + 1 < chain.columns.size()
                && ! (isSameChain && isAlone && (target.index + 1 == place->column || target.index == place->column));

    // Alongside another column's blocks while it has room, but not alongside a chain's inputs or outputs
    return target.index < chain.columns.size()
            && ! isEndColumn (chain.columns[target.index])
            && canAddParallel (chain, target.index)
            && ! (isSameChain && target.index == place->column);
}

void GraphEditorPanel::showDropTargets (NodeID nodeID, std::optional<DropTarget> lit)
{
    for (auto& target : getDropTargets())
    {
        if (auto* button = getButtonFor (target))
        {
            const bool isLit = lit.has_value() && *lit == target;
            button->setVisible (canDrop (nodeID, target));
            button->setLit (isLit);

            // Above the dragged tile, which is usually over it
            if (isLit)
                button->toFront (false);
        }
    }
}

void GraphEditorPanel::dragBlock (PluginComponent& tile, Point<int> finger, Point<int> grabOffset)
{
    if (draggedBlock != tile.pluginID)
    {
        draggedBlock = tile.pluginID;
        draggedBlockHome = tile.getBounds();
    }

    // See-through, with the lit + on top of it (see showDropTargets()), so it shows where it'll go
    tile.setTopLeftPosition (finger - grabOffset);
    tile.setAlpha (0.7f);
    tile.toFront (false);

    // The + nearest the finger that it can go to, if the finger's close enough
    litTarget.reset();
    auto nearestDistance = dropDistance;

    for (auto& target : getDropTargets())
    {
        if (auto* button = getButtonFor (target); button != nullptr && canDrop (draggedBlock, target))
        {
            const auto distance = button->getBounds().getCentre().toFloat().getDistanceFrom (finger.toFloat());

            if (distance < nearestDistance)
            {
                litTarget = target;
                nearestDistance = distance;
            }
        }
    }

    showDropTargets (draggedBlock, litTarget);

    isOverEmptySpace = ! litTarget.has_value() && isEmptySpace (finger);
    tile.setWillBeDeleted (isOverEmptySpace);

    // Near the edge of the view, it scrolls to show more of the chains, if there are more
    if (auto* viewport = findParentComponentOfClass<Viewport>())
    {
        const auto inViewport = viewport->getLocalPoint (this, finger);
        viewport->autoScroll (inViewport.x, inViewport.y, 40, 12);
    }

    repaint();
}

void GraphEditorPanel::dropBlock (PluginComponent& tile)
{
    const auto nodeID = draggedBlock;
    const auto target = litTarget;
    const bool deletes = isOverEmptySpace && nodeID == tile.pluginID;

    draggedBlock = {};
    litTarget.reset();
    isOverEmptySpace = false;
    tile.setAlpha (1.0f);
    tile.setWillBeDeleted (false);

    if (deletes)
    {
        // The tile goes when the chain view's next updated, not now, as this is called from the
        // tile's own mouseUp(). It's hidden until then.
        tile.setVisible (false);
        removeFromChain (nodeID);

        MessageManager::callAsync ([safeThis = SafePointer<GraphEditorPanel> (this)]
        {
            if (safeThis != nullptr)
                safeThis->updateComponents();
        });

        return;
    }

    if (nodeID == tile.pluginID && target.has_value())
        moveBlock (nodeID, *target);

    // Laid out again either way: where the block's gone, or back where it was
    updateComponents();
}

void GraphEditorPanel::moveBlock (NodeID nodeID, const DropTarget& target)
{
    const auto place = findBlock (nodeID);

    if (! place.has_value() || ! canDrop (nodeID, target))
        return;

    const auto& columns = chains[place->chain].columns;

    // A plugin is never in a chain's first or last column, which hold its inputs and outputs
    if (place->column == 0 || place->column + 1 >= columns.size())
        return;

    const auto& column = columns[place->column];
    const auto level = column.levels[place->row];
    const bool isAlone = column.nodes.size() == 1;
    const auto [oldSources, oldDestinations] = getNeighbours (nodeID);

    auto point = target.inSeries ? getSeriesInsertPoint (chains[target.chain], target.index)
                                 : getParallelInsertPoint (chains[target.chain], target.index);

    // It may be going next to where it is, so it's taken out of what's to feed it and what it's to
    // feed. If it's on its own in its column, the blocks either side, which are joined up when it
    // goes, take its place.
    const auto takeOut = [&] (std::vector<NodeID>& ids, const std::vector<NodeID>& neighbours)
    {
        const auto end = std::remove_if (ids.begin(), ids.end(), [&] (NodeID id)
        {
            return id == nodeID || (level != NodeID() && id == level);
        });

        if (end == ids.end())
            return;

        ids.erase (end, ids.end());

        if (isAlone)
            ids.insert (ids.end(), neighbours.begin(), neighbours.end());
    };

    takeOut (point.sources, oldSources);
    takeOut (point.destinations, oldDestinations);

    // A block that's on its own has no mix bar to show its level, so one leaving a pair, and the one
    // it leaves, are put back to full (as removeFromChain() does for the one left)
    const auto setFullLevel = [this] (NodeID levelID)
    {
        if (auto* levelNode = graph.graph.getNodeForId (levelID))
            if (auto* levelStage = dynamic_cast<LaneLevelProcessor*> (levelNode->getProcessor()))
                levelStage->getLevel().setValueNotifyingHost (1.0f);
    };

    if (column.nodes.size() == 2)
        setFullLevel (column.levels[1 - place->row]);

    if (target.inSeries)
        setFullLevel (level);

    // What fed it then feeds what it fed, where that isn't reached some other way
    graph.moveInChain (nodeID, level, point.chain, point.position, oldSources, oldDestinations, true,
                       point.sources, point.destinations, point.inSeries);
}

bool GraphEditorPanel::isEmptySpace (Point<int> position) const
{
    // Clear of every tile, the dragged block's own place included...
    for (auto* comp : nodes)
        if (comp->isVisible() && getTileBounds (comp->pluginID).expanded (emptySpaceMargin).contains (position))
            return false;

    // ...and of every +, even those hidden because the dragged block can't go there (such as either
    // side of where it is), so letting go near where it was puts it back
    for (auto* buttons : { &seriesButtons, &parallelButtons })
        for (auto* button : *buttons)
            if (! button->getBounds().isEmpty()
                 && button->getBounds().getCentre().toFloat().getDistanceFrom (position.toFloat()) < dropDistance)
                return false;

    return true;
}

Rectangle<int> GraphEditorPanel::getTileBounds (NodeID nodeID) const
{
    // A tile that's being dragged is drawn joined to the chain where it was
    if (draggedBlock != NodeID() && nodeID == draggedBlock)
        return draggedBlockHome;

    auto* comp = getComponentForPlugin (nodeID);
    return comp != nullptr ? comp->getBounds() : Rectangle<int>();
}

Component& GraphEditorPanel::getPopupParent()
{
    // The whole window, title bar included, rather than just this panel
    if (auto* window = findParentComponentOfClass<ResizableWindow>())
        if (auto* content = window->getContentComponent())
            return *content;

    return *this;
}

void GraphEditorPanel::showPluginPicker (std::function<void (const PluginChoice&)> onChosen)
{
    picker->show (getPopupParent(), std::move (onChosen));
}

void GraphEditorPanel::renameChainEnd (NodeID nodeID)
{
    auto* end = getChainEnd (nodeID);

    if (end == nullptr)
        return;

    const auto title = dynamic_cast<ChainInputProcessor*> (end) != nullptr ? "Rename input" : "Rename output";

    textEntry.show (getPopupParent(), title, end->getBlockName(), [this, nodeID] (const String& newName)
    {
        // Leaving it blank, or as its channel, goes back to calling it by its channel
        if (auto* renamed = getChainEnd (nodeID))
        {
            const auto name = newName.trim();
            renamed->setCustomName (name == renamed->getChannelName() ? String() : name);
        }
    });
}

void GraphEditorPanel::showPopupMenu (Point<int> mousePos)
{
    menu.reset (new PopupMenu);

    if (auto* mainWindow = findParentComponentOfClass<MainHostWindow>())
    {
        mainWindow->addPluginsToMenu (*menu);

        menu->showMenuAsync ({},
                             ModalCallbackFunction::create ([this, mousePos] (int r)
                                                            {
                                                                if (auto* mainWin = findParentComponentOfClass<MainHostWindow>())
                                                                    if (const auto chosen = mainWin->getChosenType (r))
                                                                        createNewPlugin (*chosen, mousePos);
                                                            }));
    }
}

void GraphEditorPanel::beginConnectorDrag (AudioProcessorGraph::NodeAndChannel source,
                                           AudioProcessorGraph::NodeAndChannel dest,
                                           const MouseEvent& e)
{
    auto* c = dynamic_cast<ConnectorComponent*> (e.originalComponent);
    connectors.removeObject (c, false);
    draggingConnector.reset (c);

    if (draggingConnector == nullptr)
        draggingConnector.reset (new ConnectorComponent (*this));

    draggingConnector->setInput (source);
    draggingConnector->setOutput (dest);

    addAndMakeVisible (draggingConnector.get());
    draggingConnector->toFront (false);

    dragConnector (e);
}

void GraphEditorPanel::dragConnector (const MouseEvent& e)
{
    auto e2 = e.getEventRelativeTo (this);

    if (draggingConnector != nullptr)
    {
        draggingConnector->setTooltip ({});

        auto pos = e2.position;

        if (auto* pin = findPinAt (pos))
        {
            auto connection = draggingConnector->connection;

            if (connection.source.nodeID == AudioProcessorGraph::NodeID() && ! pin->isInput)
            {
                connection.source = pin->pin;
            }
            else if (connection.destination.nodeID == AudioProcessorGraph::NodeID() && pin->isInput)
            {
                connection.destination = pin->pin;
            }

            if (graph.graph.canConnect (connection))
            {
                pos = (pin->getParentComponent()->getPosition() + pin->getBounds().getCentre()).toFloat();
                //draggingConnector->setTooltip (pin->getTooltip());
            }
        }

        if (draggingConnector->connection.source.nodeID == AudioProcessorGraph::NodeID())
            draggingConnector->dragStart (pos);
        else
            draggingConnector->dragEnd (pos);
    }
}

void GraphEditorPanel::endDraggingConnector (const MouseEvent& e)
{
    if (draggingConnector == nullptr)
        return;

    draggingConnector->setTooltip ({});

    auto e2 = e.getEventRelativeTo (this);
    auto connection = draggingConnector->connection;

    draggingConnector = nullptr;

    if (auto* pin = findPinAt (e2.position))
    {
        if (connection.source.nodeID == AudioProcessorGraph::NodeID())
        {
            if (pin->isInput)
                return;

            connection.source = pin->pin;
        }
        else
        {
            if (! pin->isInput)
                return;

            connection.destination = pin->pin;
        }

        graph.graph.addConnection (connection);
    }
}

void GraphEditorPanel::timerCallback()
{
    stopTimer();

    // A link that's being held lights up, and once it's been held long enough, it's removed
    if (pendingLink.has_value())
    {
        heldLink = pendingLink;
        pendingLink.reset();
        startTimer (linkHoldMilliseconds - linkLightUpMilliseconds);
        repaint();
    }
    else if (heldLink.has_value())
    {
        graph.disconnectBlocks (heldLink->source, heldLink->to);
        heldLink.reset();
        removedHeldLink = true;
        repaint();
    }
}

void GraphEditorPanel::activeComponentChanged(PluginComponent* newActiveComponent) {
    if (activeComponent)
        activeComponent->setActive(false);

    activeComponent = newActiveComponent;

    Component::BailOutChecker checker(this);
    listeners.callChecked(checker, &Listener::onActiveComponentChange, activeComponent);
}

//==============================================================================
void GraphEditorPanel::addListener(Listener* l) { listeners.add(l); }
void GraphEditorPanel::removeListener(Listener* l) { listeners.remove(l); }
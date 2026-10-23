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
#include "PluginControls/ControlXmlParser.h"
#include "ThemeManager.h"

namespace
{
    // Sizes at full scale; tiles and gaps shrink together when the chain gets too long or tall
    constexpr int tileSize = 120;
    constexpr int gapWidth = 72;
    constexpr int rowGap = 24;
    constexpr int insertButtonSize = 44;
    constexpr int chainMargin = 16;

    // More rows than this won't fit legibly on the 400 px tall screen
    constexpr size_t maxParallelPlugins = 3;
}

//==============================================================================
// A round "+" on the signal path, for inserting a plugin at that point
class GraphEditorPanel::InsertButton final : public Button
{
public:
    InsertButton() : Button ("Insert plugin") {}

    void paintButton (Graphics& g, bool, bool isButtonDown) override
    {
        auto area = getLocalBounds().toFloat().reduced (2.0f);
        auto colour = findColour (AppColourIds::signalPath);

        // Fill the circle so the signal line doesn't show through it
        g.setColour (findColour (ResizableWindow::backgroundColourId));
        g.fillEllipse (area);

        g.setColour (isButtonDown ? colour.brighter() : colour);
        g.drawEllipse (area, 2.5f);

        auto plus = area.reduced (area.getWidth() * 0.28f);
        g.fillRect (plus.withSizeKeepingCentre (plus.getWidth(), 3.0f));
        g.fillRect (plus.withSizeKeepingCentre (3.0f, plus.getHeight()));
    }
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
    {
        for (auto& [name, path] : ControlXmlParser::listPluginControlsInDirectory (folder.getFullPathName().toStdString()))
            pluginControlPaths.emplace (name, path);

        for (auto& [name, set] : PluginModuleSet::loadAll (folder))
            moduleSets.emplace (name, std::move (set));
    }

    picker = std::make_unique<PluginPicker> (knownPlugins, moduleSets);
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

    const auto centreY = [this] (AudioProcessorGraph::NodeID id)
    {
        auto* comp = getComponentForPlugin (id);
        return comp != nullptr ? (float) comp->getBounds().getCentreY() : 0.0f;
    };

    // Each column's empty lane, running between the rails either side of it
    for (size_t c = 1; c + 1 < columns.size() && c < busX.size(); ++c)
    {
        if (laneY[c] < 0)
            continue;

        const auto y = (float) laneY[c];
        const auto left = (float) busX[c - 1];
        const auto right = (float) busX[c];
        const auto lowestTile = centreY (columns[c].nodes.back());

        drawPath ({ left, lowestTile, left, y }, false);
        drawPath ({ left, y, right, y }, false);
        drawPath ({ right, lowestTile, right, y }, false);
    }

    // The rail in each gap: every tile in the column before it feeds in, and every tile in the
    // column after it is fed from it, so parallel plugins split and merge here
    for (size_t gap = 0; gap < busX.size() && gap + 1 < columns.size(); ++gap)
    {
        const auto x = (float) busX[gap];
        const bool connected = isAudioConnected (columns[gap], columns[gap + 1]);

        // Tiles are drawn for the plugins; their level stages sit invisibly just after them
        auto top = std::numeric_limits<float>::max();
        auto bottom = std::numeric_limits<float>::lowest();

        for (auto id : columns[gap].nodes)
        {
            if (auto* comp = getComponentForPlugin (id))
            {
                const auto y = centreY (id);
                drawPath ({ (float) comp->getRight(), y, x, y }, connected);
                top = jmin (top, y);
                bottom = jmax (bottom, y);
            }
        }

        for (auto id : columns[gap + 1].nodes)
        {
            if (auto* comp = getComponentForPlugin (id))
            {
                const auto y = centreY (id);
                drawPath ({ x, y, (float) comp->getX(), y }, connected);
                top = jmin (top, y);
                bottom = jmax (bottom, y);
            }
        }

        if (bottom > top)
            drawPath ({ x, top, x, bottom }, connected);
    }
}

void GraphEditorPanel::mouseDown (const MouseEvent& e)
{
    if (true || isOnTouchDevice())
    {
        originalTouchPos = e.position.toInt();
        startTimer (750);
    }

    if (e.mods.isPopupMenu())
        showPopupMenu (e.position.toInt());
}

void GraphEditorPanel::mouseUp (const MouseEvent&)
{
    if (true || isOnTouchDevice())
    {
        stopTimer();
    }

    activeComponentChanged(nullptr);
}

void GraphEditorPanel::mouseDrag (const MouseEvent& e)
{
    if (true || isOnTouchDevice() && e.getDistanceFromDragStart() > 5)
        stopTimer();
}

void GraphEditorPanel::createNewPlugin (const PluginDescriptionAndPreference& desc, Point<int> position)
{
    size_t nearestGap = 0;
    int nearestDistance = std::numeric_limits<int>::max();

    for (size_t gap = 0; gap < busX.size(); ++gap)
    {
        const int distance = std::abs (busX[gap] - position.x);

        if (distance < nearestDistance)
        {
            nearestGap = gap;
            nearestDistance = distance;
        }
    }

    if (! busX.empty())
        insertInSeries ({ desc }, nearestGap);
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

void GraphEditorPanel::insertInSeries (const PluginChoice& choice, size_t gap)
{
    if (gap + 1 >= columns.size())
        return;

    // A new column halfway between its neighbours' positions, so it sorts between them
    const auto position = (columns[gap].position + columns[gap + 1].position) / 2.0;

    graph.insertPlugin (choice.description, columns[gap].getOutputs(), columns[gap + 1].getInputs(), { position, 0.0 }, true,
                        getModuleSetUp (choice.moduleSet, choice.module));
}

void GraphEditorPanel::insertInParallel (const PluginChoice& choice, size_t column)
{
    if (! canAddParallel (column))
        return;

    // Same x as the rest of the column, and a row below its lowest plugin
    const auto row = graph.getNodePosition (columns[column].nodes.back()).y + 1.0;

    graph.insertPlugin (choice.description, columns[column - 1].getOutputs(), columns[column + 1].getInputs(),
                        { columns[column].position, row }, false,
                        getModuleSetUp (choice.moduleSet, choice.module));
}

void GraphEditorPanel::removeFromChain (AudioProcessorGraph::NodeID nodeID)
{
    // The input and output nodes at either end stay put
    for (size_t c = 1; c + 1 < columns.size(); ++c)
    {
        const auto& column = columns[c];
        const auto it = std::find (column.nodes.begin(), column.nodes.end(), nodeID);

        if (it == column.nodes.end())
            continue;

        const auto index = (size_t) std::distance (column.nodes.begin(), it);
        std::vector<AudioProcessorGraph::NodeID> toRemove { nodeID };

        if (column.levels[index] != AudioProcessorGraph::NodeID())
            toRemove.push_back (column.levels[index]);

        // When one of two parallel plugins goes, the other is on its own again, so put its level
        // back to full rather than leave it quietly turned down with no mix bar to show it
        if (column.nodes.size() == 2)
            if (auto* remaining = graph.graph.getNodeForId (column.levels[1 - index]))
                if (auto* level = dynamic_cast<LaneLevelProcessor*> (remaining->getProcessor()))
                    level->getLevel().setValueNotifyingHost (1.0f);

        // If it was the last plugin in its column, join the columns either side so the signal still flows
        graph.removeFromChain (toRemove, columns[c - 1].getOutputs(), columns[c + 1].getInputs(), column.nodes.size() == 1);
        return;
    }
}

BlockIcon GraphEditorPanel::getBlockIcon (AudioProcessorGraph::NodeID nodeID) const
{
    auto* node = graph.graph.getNodeForId (nodeID);

    if (node == nullptr)
        return BlockIcon::plugin;

    if (auto* io = dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor*> (node->getProcessor()))
        return io->isInput() ? BlockIcon::input : BlockIcon::output;

    auto* plugin = dynamic_cast<AudioPluginInstance*> (node->getProcessor());
    const auto setIt = plugin != nullptr ? moduleSets.find (plugin->getPluginDescription().name) : moduleSets.end();

    if (setIt == moduleSets.end())
        return BlockIcon::plugin;

    // One of a plugin's modules shows what kind of block it is; the whole plugin shows it's a suite
    const auto moduleName = node->properties["module"].toString();

    if (moduleName.isEmpty())
        return BlockIcon::suite;

    if (auto* module = setIt->second.findModule (moduleName))
        return blockIconFromName (module->icon);

    return BlockIcon::plugin;
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
    for (int i = nodes.size(); --i >= 0;)
        if (graph.graph.getNodeForId(nodes.getUnchecked(i)->pluginID) == nullptr) {
            Component::BailOutChecker checker(this);
            listeners.callChecked(checker, &Listener::onNodeDeleted, nodes[i]);

            if (activeComponent == nodes[i]) {
                activeComponent = nullptr;
            }

            nodes.remove(i);
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

    layoutChain();
}

std::vector<GraphEditorPanel::Column> GraphEditorPanel::findColumns() const
{
    using IOProcessor = AudioProcessorGraph::AudioGraphIOProcessor;

    const auto findIONode = [this] (IOProcessor::IODeviceType type) -> AudioProcessorGraph::Node*
    {
        for (auto* node : graph.graph.getNodes())
            if (auto* io = dynamic_cast<IOProcessor*> (node->getProcessor()))
                if (io->getType() == type)
                    return node;

        return nullptr;
    };

    auto* input = findIONode (IOProcessor::audioInputNode);
    auto* output = findIONode (IOProcessor::audioOutputNode);

    std::vector<Column> result;

    if (input == nullptr || output == nullptr)
        return result;

    // The plugins go between the input and output, ordered by their stored x position. Plugins with
    // the same x run in parallel, top to bottom by their stored y. The order doesn't come from the
    // connections, so a plugin stays in place even when no audio reaches it (e.g. with no input
    // device). The I/O nodes, including the MIDI ones, aren't plugins.
    std::vector<AudioProcessorGraph::Node*> plugins;
    std::map<std::pair<double, double>, AudioProcessorGraph::NodeID> levelsByPosition;

    const auto positionOf = [] (const AudioProcessorGraph::Node* node)
    {
        return std::make_pair ((double) node->properties["x"], (double) node->properties["y"]);
    };

    for (auto* node : graph.graph.getNodes())
    {
        // Level stages aren't shown; each one belongs to the plugin at the same position
        if (dynamic_cast<LaneLevelProcessor*> (node->getProcessor()) != nullptr)
            levelsByPosition[positionOf (node)] = node->nodeID;
        else if (dynamic_cast<IOProcessor*> (node->getProcessor()) == nullptr)
            plugins.push_back (node);
    }

    std::sort (plugins.begin(), plugins.end(), [] (const auto* a, const auto* b)
    {
        const auto ax = (double) a->properties["x"], bx = (double) b->properties["x"];
        const auto ay = (double) a->properties["y"], by = (double) b->properties["y"];

        if (ax != bx)   return ax < bx;
        if (ay != by)   return ay < by;
        return a->nodeID.uid < b->nodeID.uid;
    });

    result.push_back ({ { input->nodeID }, 0.0, { {} } });

    for (auto* node : plugins)
    {
        const auto x = (double) node->properties["x"];
        const auto levelIt = levelsByPosition.find (positionOf (node));
        const auto level = levelIt != levelsByPosition.end() ? levelIt->second : AudioProcessorGraph::NodeID();

        if (result.size() > 1 && result.back().position == x)
        {
            result.back().nodes.push_back (node->nodeID);
            result.back().levels.push_back (level);
        }
        else
        {
            result.push_back ({ { node->nodeID }, x, { level } });
        }
    }

    result.push_back ({ { output->nodeID }, 1.0, { {} } });
    return result;
}

std::vector<AudioProcessorGraph::NodeID> GraphEditorPanel::Column::getOutputs() const
{
    auto outputs = nodes;

    for (size_t i = 0; i < outputs.size() && i < levels.size(); ++i)
        if (levels[i] != AudioProcessorGraph::NodeID())
            outputs[i] = levels[i];

    return outputs;
}

bool GraphEditorPanel::isAudioConnected (const Column& sources, const Column& destinations) const
{
    const auto from = sources.getOutputs();
    const auto& to = destinations.getInputs();

    for (auto& connection : graph.graph.getConnections())
    {
        if (connection.source.isMIDI())
            continue;

        if (std::find (from.begin(), from.end(), connection.source.nodeID) != from.end()
             && std::find (to.begin(), to.end(), connection.destination.nodeID) != to.end())
            return true;
    }

    return false;
}

bool GraphEditorPanel::canAddParallel (size_t column) const
{
    // Only plugin columns, not the input and output at either end
    return column > 0 && column + 1 < columns.size()
            && columns[column].nodes.size() < maxParallelPlugins;
}

void GraphEditorPanel::layoutChain()
{
    columns = findColumns();

    // Hide the nodes that aren't part of the chain (the MIDI I/O nodes)
    for (auto* fc : nodes)
    {
        bool inChain = false;

        for (auto& column : columns)
            inChain = inChain || std::find (column.nodes.begin(), column.nodes.end(), fc->pluginID) != column.nodes.end();

        fc->setVisible (inChain);
    }

    const int numColumns = (int) columns.size();
    const int numGaps = jmax (0, numColumns - 1);

    const auto resizeButtons = [this] (OwnedArray<InsertButton>& buttons, int size, std::function<void (int)> onClick)
    {
        while (buttons.size() < size)
        {
            auto* button = buttons.add (new InsertButton());
            const int index = buttons.size() - 1;
            button->onClick = [onClick, index] { onClick (index); };
            addChildComponent (button);
        }

        while (buttons.size() > size)
            buttons.removeLast();
    };

    resizeButtons (seriesButtons, numGaps, [this] (int gap)
    {
        showPluginPicker ([this, gap] (const PluginChoice& choice)
        {
            insertInSeries (choice, (size_t) gap);
        });
    });

    resizeButtons (parallelButtons, numColumns, [this] (int column)
    {
        showPluginPicker ([this, column] (const PluginChoice& choice)
        {
            insertInParallel (choice, (size_t) column);
        });
    });

    // The height each column needs at full size: its tiles, plus its lane if it has room for more
    int fullHeight = tileSize;

    for (size_t c = 0; c < columns.size(); ++c)
    {
        const int numRows = (int) columns[c].nodes.size();
        int height = numRows * tileSize + (numRows - 1) * rowGap;

        if (canAddParallel (c))
            height += rowGap + insertButtonSize;

        fullHeight = jmax (fullHeight, height);
    }

    // Shrink everything together when the chain is too long or too tall for the panel
    const int fullWidth = numColumns * tileSize + numGaps * gapWidth;
    const float scale = jmin (1.0f,
                              (float) (getWidth() - 2 * chainMargin) / (float) jmax (1, fullWidth),
                              (float) (getHeight() - 2 * chainMargin) / (float) fullHeight);
    const int tile = roundToInt ((float) tileSize * scale);
    const int gap = roundToInt ((float) gapWidth * scale);
    const int rowSpacing = roundToInt ((float) rowGap * scale);
    const int buttonSize = jmin (insertButtonSize, gap - 4);

    int x = (getWidth() - (numColumns * tile + numGaps * gap)) / 2;
    const int top = (getHeight() - roundToInt ((float) fullHeight * scale)) / 2;

    busX.assign ((size_t) numGaps, 0);
    laneY.assign ((size_t) numColumns, -1);

    for (int c = 0; c < numColumns; ++c)
    {
        const auto& column = columns[(size_t) c];

        // Plugins sharing a column get a mix bar for their lane's level
        const bool isParallel = column.nodes.size() > 1;

        for (size_t row = 0; row < column.nodes.size(); ++row)
        {
            if (auto* comp = getComponentForPlugin (column.nodes[row]))
            {
                comp->setBounds (x, top + (int) row * (tile + rowSpacing), tile, tile);
                comp->setLaneLevel (isParallel ? graph.graph.getNodeForId (column.levels[row]) : nullptr);
            }
        }

        const bool hasLane = canAddParallel ((size_t) c);
        parallelButtons[c]->setVisible (hasLane);

        if (hasLane)
        {
            const int laneTop = top + (int) column.nodes.size() * (tile + rowSpacing);
            laneY[(size_t) c] = laneTop + buttonSize / 2;
            parallelButtons[c]->setBounds (Rectangle<int> (x, laneTop, tile, buttonSize).withSizeKeepingCentre (buttonSize, buttonSize));
        }

        x += tile;

        if (c < numGaps)
        {
            busX[(size_t) c] = x + gap / 2;
            seriesButtons[c]->setBounds (Rectangle<int> (x, top, gap, tile).withSizeKeepingCentre (buttonSize, buttonSize));
            seriesButtons[c]->setVisible (true);
            x += gap;
        }
    }

    repaint();
}

void GraphEditorPanel::showPluginPicker (std::function<void (const PluginChoice&)> onChosen)
{
    // Over the whole window, title bar included, rather than just this panel
    Component* parent = this;

    if (auto* window = findParentComponentOfClass<ResizableWindow>())
        if (auto* content = window->getContentComponent())
            parent = content;

    picker->show (*parent, std::move (onChosen));
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
    // this should only be called on touch devices
    jassert (true || isOnTouchDevice());

    stopTimer();
    //showPopupMenu (originalTouchPos);
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
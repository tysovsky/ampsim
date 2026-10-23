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
    // Sizes along the chain at full scale; tiles and gaps shrink together when the chain gets long
    constexpr int tileSize = 120;
    constexpr int gapWidth = 72;
    constexpr int insertButtonSize = 44;
    constexpr int chainMargin = 16;
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

    pluginControlPaths = ControlXmlParser::listPluginControlsInDirectory("C:\\Program Files\\StudioHost\\PluginControls");
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

    // The signal path between the tiles: solid where audio is connected, dashed where it isn't
    g.setColour (findColour (AppColourIds::signalPath));

    for (size_t i = 0; i + 1 < chain.size(); ++i)
    {
        auto* from = getComponentForPlugin (chain[i]);
        auto* to = getComponentForPlugin (chain[i + 1]);

        if (from == nullptr || to == nullptr)
            continue;

        const Line<float> line ((float) from->getRight(), (float) from->getBounds().getCentreY(),
                                (float) to->getX(),       (float) to->getBounds().getCentreY());

        if (isAudioConnected (chain[i], chain[i + 1]))
        {
            g.drawLine (line, 3.0f);
        }
        else
        {
            const float dashes[] = { 6.0f, 6.0f };
            g.drawDashedLine (line, dashes, numElementsInArray (dashes), 3.0f);
        }
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
    int nearestGap = -1;
    int nearestDistance = std::numeric_limits<int>::max();

    for (int i = 0; i < insertButtons.size(); ++i)
    {
        const int distance = std::abs (insertButtons[i]->getBounds().getCentreX() - position.x);

        if (distance < nearestDistance)
        {
            nearestGap = i;
            nearestDistance = distance;
        }
    }

    if (nearestGap >= 0)
        insertIntoChain (desc, nearestGap);
}

void GraphEditorPanel::insertIntoChain (const PluginDescriptionAndPreference& desc, int gapIndex)
{
    const auto index = (size_t) gapIndex;

    if (index + 1 >= chain.size())
        return;

    // Halfway between its neighbours' positions, so it sorts between them
    const auto position = (getChainPosition (index) + getChainPosition (index + 1)) / 2.0;

    graph.insertPlugin (desc, chain[index], chain[index + 1], { position, 0.5 });
}

void GraphEditorPanel::removeFromChain (AudioProcessorGraph::NodeID nodeID)
{
    const auto it = std::find (chain.begin(), chain.end(), nodeID);

    // The input and output nodes at either end stay put
    if (it == chain.end() || it == chain.begin() || it + 1 == chain.end())
        return;

    graph.removeFromChain (nodeID, *(it - 1), *(it + 1));
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

std::vector<AudioProcessorGraph::NodeID> GraphEditorPanel::findChain() const
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

    std::vector<AudioProcessorGraph::NodeID> result;

    if (input == nullptr || output == nullptr)
        return result;

    // The plugins go between the input and output, ordered by their stored x position. The order
    // doesn't come from the connections, so a plugin stays in place even when no audio reaches it
    // (e.g. with no input device). The I/O nodes, including the MIDI ones, aren't plugins.
    std::vector<AudioProcessorGraph::Node*> plugins;

    for (auto* node : graph.graph.getNodes())
        if (dynamic_cast<IOProcessor*> (node->getProcessor()) == nullptr)
            plugins.push_back (node);

    std::sort (plugins.begin(), plugins.end(), [] (const auto* a, const auto* b)
    {
        const auto ax = (double) a->properties["x"];
        const auto bx = (double) b->properties["x"];
        return ax != bx ? ax < bx : a->nodeID.uid < b->nodeID.uid;
    });

    result.push_back (input->nodeID);

    for (auto* node : plugins)
        result.push_back (node->nodeID);

    result.push_back (output->nodeID);
    return result;
}

double GraphEditorPanel::getChainPosition (size_t index) const
{
    // The input and output sit at either end; plugins in between keep their stored x position
    if (index == 0)
        return 0.0;

    if (index + 1 >= chain.size())
        return 1.0;

    return graph.getNodePosition (chain[index]).x;
}

bool GraphEditorPanel::isAudioConnected (AudioProcessorGraph::NodeID source, AudioProcessorGraph::NodeID destination) const
{
    for (auto& connection : graph.graph.getConnections())
        if (connection.source.nodeID == source && connection.destination.nodeID == destination
             && ! connection.source.isMIDI())
            return true;

    return false;
}

void GraphEditorPanel::layoutChain()
{
    chain = findChain();

    for (auto* fc : nodes)
        fc->setVisible (std::find (chain.begin(), chain.end(), fc->pluginID) != chain.end());

    const int numTiles = (int) chain.size();
    const int numGaps = jmax (0, numTiles - 1);

    while (insertButtons.size() < numGaps)
    {
        auto* button = insertButtons.add (new InsertButton());
        const int gapIndex = insertButtons.size() - 1;
        button->onClick = [this, gapIndex] { showPluginPicker (gapIndex); };
        addAndMakeVisible (button);
    }

    while (insertButtons.size() > numGaps)
        insertButtons.removeLast();

    // Shrink everything together when the chain is too long for the panel
    const int fullWidth = numTiles * tileSize + numGaps * gapWidth;
    const float scale = fullWidth > 0 ? jmin (1.0f, (float) (getWidth() - 2 * chainMargin) / (float) fullWidth) : 1.0f;
    const int tile = roundToInt ((float) tileSize * scale);
    const int gap = roundToInt ((float) gapWidth * scale);
    const int buttonSize = jmin (insertButtonSize, gap - 4);

    int x = (getWidth() - (numTiles * tile + numGaps * gap)) / 2;
    const int y = (getHeight() - tile) / 2;

    for (int i = 0; i < numTiles; ++i)
    {
        if (auto* comp = getComponentForPlugin (chain[(size_t) i]))
            comp->setBounds (x, y, tile, tile);

        x += tile;

        if (i < numGaps)
        {
            insertButtons[i]->setBounds (Rectangle<int> (x, y, gap, tile).withSizeKeepingCentre (buttonSize, buttonSize));
            x += gap;
        }
    }

    repaint();
}

void GraphEditorPanel::showPluginPicker (int gapIndex)
{
    if (! isPositiveAndBelow (gapIndex, insertButtons.size()))
        return;

    // Everything that's been scanned, apart from the input and output nodes
    pickerTypes.clear();

    for (auto& type : knownPlugins.getTypes())
        if (type.category != "I/O devices")
            pickerTypes.add (type);

    std::sort (pickerTypes.begin(), pickerTypes.end(),
               [] (const PluginDescription& a, const PluginDescription& b) { return a.name.compareNatural (b.name) < 0; });

    PopupMenu picker;
    picker.setLookAndFeel (&touchLookAndFeel);

    if (pickerTypes.isEmpty())
        picker.addItem (-1, "No plugins found - scan for plugins first", false);

    for (int i = 0; i < pickerTypes.size(); ++i)
        picker.addItem (i + 1, pickerTypes[i].name);

    picker.showMenuAsync (TouchLookAndFeel::getMenuOptions (*insertButtons[gapIndex], jmax (1, pickerTypes.size()), insertButtonSize)
                            .withMinimumWidth (300),
                        [safeThis = SafePointer<GraphEditorPanel> (this), gapIndex] (int result)
                        {
                            if (safeThis != nullptr && isPositiveAndBelow (result - 1, safeThis->pickerTypes.size()))
                                safeThis->insertIntoChain (PluginDescriptionAndPreference { safeThis->pickerTypes[result - 1] },
                                                           gapIndex);
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
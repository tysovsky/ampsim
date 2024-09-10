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

//==============================================================================
GraphEditorPanel::GraphEditorPanel (PluginGraph& g)  : graph (g)
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
    graph.addPlugin (desc, position.toDouble() / Point<double> ((double) getWidth(), (double) getHeight()));
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

    for (int i = connectors.size(); --i >= 0;)
        if (! graph.graph.isConnected (connectors.getUnchecked (i)->connection))
            connectors.remove (i);

    for (auto* fc : nodes)
        fc->update();

    for (auto* cc : connectors)
        cc->update();

    for (auto* f : graph.graph.getNodes())
    {
        if (getComponentForPlugin (f->nodeID) == nullptr)
        {
            auto* comp = nodes.add (new PluginComponent (*this, f->nodeID));
            addAndMakeVisible (comp);
            comp->update();
        }
    }

    for (auto& c : graph.graph.getConnections())
    {
        if (getComponentForConnection (c) == nullptr)
        {
            auto* comp = connectors.add (new ConnectorComponent(*this));
            addAndMakeVisible (comp);

            comp->setInput (c.source);
            comp->setOutput (c.destination);
        }
    }
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
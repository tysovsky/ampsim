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

#pragma once

#include "../Plugins/PluginGraph.h"
#include "PluginComponent.h"
#include "ConnectorComponent.h"
#include "PinComponent.h"
#include "TouchLookAndFeel.h"

class MainHostWindow;


//==============================================================================
/**
    A panel that displays and edits a PluginGraph as a signal chain running left
    to right, from the audio input to the audio output, like a row of pedals.
    A + button between each pair of nodes inserts a plugin at that point.
*/
class GraphEditorPanel final : public Component,
                               public ChangeListener,
                               private Timer
{
public:
    struct Listener {
        virtual ~Listener() {}

        virtual void onActiveComponentChange(PluginComponent* activeComponent) = 0;

        virtual void onNodeDeleted(PluginComponent* removedComponent) = 0;
    };

    //==============================================================================
    GraphEditorPanel (PluginGraph& graph, KnownPluginList& knownPlugins);
    ~GraphEditorPanel() override;

    /** Inserts a plugin into the chain at the + button nearest to the given position. */
    void createNewPlugin (const PluginDescriptionAndPreference&, Point<int> position);

    /** Removes a plugin from the chain, joining up the nodes either side of it. */
    void removeFromChain (AudioProcessorGraph::NodeID);

    void paint (Graphics&) override;
    void resized() override;

    void mouseDown (const MouseEvent&) override;
    void mouseUp   (const MouseEvent&) override;
    void mouseDrag (const MouseEvent&) override;

    void changeListenerCallback (ChangeBroadcaster*) override;

    //==============================================================================
    void updateComponents();

    //==============================================================================
    void showPopupMenu (Point<int> position);

    //==============================================================================
    void beginConnectorDrag (AudioProcessorGraph::NodeAndChannel source,
                             AudioProcessorGraph::NodeAndChannel dest,
                             const MouseEvent&);
    void dragConnector (const MouseEvent&);
    void endDraggingConnector (const MouseEvent&);
    
    //==============================================================================
    void activeComponentChanged(PluginComponent* activeComponent);

    //==============================================================================
    void addListener(Listener* newListener);
    void removeListener(Listener* listener);
    
    //==============================================================================


    PluginComponent* getComponentForPlugin(AudioProcessorGraph::NodeID) const;
    ConnectorComponent* getComponentForConnection(const AudioProcessorGraph::Connection&) const;
    PinComponent* findPinAt(Point<float>) const;

    PluginGraph& graph;
    PluginComponent* activeComponent;
    std::unordered_map<std::string, std::string> pluginControlPaths;

private:
    class InsertButton;

    std::vector<AudioProcessorGraph::NodeID> findChain() const;
    double getChainPosition (size_t index) const;
    void insertIntoChain (const PluginDescriptionAndPreference&, int gapIndex);
    bool isAudioConnected (AudioProcessorGraph::NodeID source, AudioProcessorGraph::NodeID destination) const;
    void layoutChain();
    void showPluginPicker (int gapIndex);

    KnownPluginList& knownPlugins;
    TouchLookAndFeel touchLookAndFeel;
    std::vector<AudioProcessorGraph::NodeID> chain;
    OwnedArray<InsertButton> insertButtons;
    Array<PluginDescription> pickerTypes;

    OwnedArray<PluginComponent> nodes;
    OwnedArray<ConnectorComponent> connectors;
    std::unique_ptr<ConnectorComponent> draggingConnector;
    std::unique_ptr<PopupMenu> menu;
    std::unique_ptr<AlertWindow> nodeActionWindow;
    ListenerList<Listener> listeners;

    //==============================================================================
    Point<int> originalTouchPos;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
};

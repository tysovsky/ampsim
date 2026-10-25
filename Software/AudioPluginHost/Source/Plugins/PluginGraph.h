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

#include "../UI/PluginWindow.h"

//==============================================================================
/** A type that encapsulates a PluginDescription and some preferences regarding
    how plugins of that description should be instantiated.
*/
struct PluginDescriptionAndPreference
{
    enum class UseARA { no, yes };

    PluginDescriptionAndPreference() = default;

    explicit PluginDescriptionAndPreference (PluginDescription pd)
        : pluginDescription (std::move (pd)),
          useARA (pluginDescription.hasARAExtension ? PluginDescriptionAndPreference::UseARA::yes
                                                    : PluginDescriptionAndPreference::UseARA::no)
    {}

    PluginDescriptionAndPreference (PluginDescription pd, UseARA ara)
        : pluginDescription (std::move (pd)), useARA (ara)
    {}

    PluginDescription pluginDescription;
    UseARA useARA = UseARA::no;
};

//==============================================================================
/**
    A collection of plugins and some connections between them.

    Every change is saved to the graph's file as it happens: a change to what's in
    the graph or how it's connected, to any plugin's parameters or state, or to a
    node's bypass. Saving waits until there's been a moment without changes, so
    turning a knob saves once it stops. A graph without a file of its own is saved
    to getDefaultGraphFile().
*/
class PluginGraph final : public FileBasedDocument,
                          public AudioProcessorListener,
                          private ChangeListener,
                          private Timer
{
public:
    //==============================================================================
    PluginGraph (AudioPluginFormatManager&, KnownPluginList&);
    ~PluginGraph() override;

    //==============================================================================
    using NodeID = AudioProcessorGraph::NodeID;

    void addPlugin (const PluginDescriptionAndPreference&, Point<double>,
                    std::function<void (NodeID)> onAdded = nullptr);

    /** Adds a plugin, followed by a lane level stage, fed by every node in sources and feeding
        every node in destinations. In series, the direct connections from sources to destinations
        are removed so the audio goes through the new plugin; otherwise it runs in parallel with
        whatever is already there. The level stage shares the plugin's position and chain, which is
        how the chain view pairs them up. If given, setUp is called with the new plugin's node
        before it's connected, e.g. to configure it as a module or say which chain it's in.
    */
    void insertPlugin (const PluginDescriptionAndPreference&,
                       std::vector<NodeID> sources, std::vector<NodeID> destinations,
                       Point<double> position, bool inSeries,
                       std::function<void (AudioProcessorGraph::Node&)> setUp = nullptr);

    /** Removes nodes from the signal chain (a plugin and its level stage). If joinNeighbours is
        true, every source is then connected to every destination so the signal still flows past the gap.
    */
    void removeFromChain (const std::vector<NodeID>& nodes, const std::vector<NodeID>& sources,
                          const std::vector<NodeID>& destinations, bool joinNeighbours);

    /** Moves a plugin and its level stage (if it has one) to another place in a chain, as they are.
        They're disconnected from where they were, and if joinOldNeighbours is true, every node in
        oldSources is then connected to every node in oldDestinations so the signal still flows past
        the gap. Then they're given the new position and chain, and connected as insertPlugin()
        connects a new plugin.
    */
    void moveInChain (NodeID plugin, NodeID level, int chain, Point<double> position,
                      const std::vector<NodeID>& oldSources, const std::vector<NodeID>& oldDestinations,
                      bool joinOldNeighbours, const std::vector<NodeID>& sources,
                      const std::vector<NodeID>& destinations, bool inSeries);

    /** Adds one end of a chain: a ChainInputProcessor, or a ChainOutputProcessor if isInput is
        false, set to a channel choice, in a chain (its "chain" property), fed by every node in
        sources and feeding every node in destinations, and connected to the audio device.
    */
    NodeID addChainEnd (bool isInput, int chain, Point<double> position, int channelChoice,
                        const std::vector<NodeID>& sources, const std::vector<NodeID>& destinations);

    /** Connects all the audio device's input channels to every chain input, and every chain output
        to all the device's output channels, as far as each goes, and the MIDI input to every
        instrument. Call it again after the device changes, as channels it didn't have before can't
        be connected until it has them.
    */
    void connectDeviceChannels();

    /** Removes the connections between the audio device and the plugins, which is how graphs from
        before chains had inputs and outputs of their own were wired.
    */
    void disconnectAudioDevice();

    AudioProcessorGraph::Node::Ptr getNodeForName (const String& name) const;

    void setNodePosition (NodeID, Point<double>);
    Point<double> getNodePosition (NodeID) const;

    /** Bypasses a node or brings it back, as a change to save. */
    void setNodeBypassed (NodeID, bool shouldBeBypassed);

    //==============================================================================
    void clear();

    PluginWindow* getOrCreateWindowFor (AudioProcessorGraph::Node*, PluginWindow::Type);
    void closeCurrentlyOpenWindowsFor (AudioProcessorGraph::NodeID);
    bool closeAnyOpenPluginWindows();

    //==============================================================================
    // Called for the graph and for every plugin in it, possibly on the audio thread
    void audioProcessorParameterChanged (AudioProcessor*, int, float) override  { markChanged(); }
    void audioProcessorChanged (AudioProcessor*, const ChangeDetails&) override { changed(); markChanged(); }

    /** Saves any changes that are waiting to be saved now, e.g. before quitting. */
    void saveChanges();

    /** Marks the graph as changed, to be saved soon, e.g. after a block's properties changed (what
        its controls are showing), which nothing else tells it about. */
    void markChanged() noexcept;

    //==============================================================================
    std::unique_ptr<XmlElement> createXml() const;
    void restoreFromXml (const XmlElement&);

    static const char* getFilenameSuffix()      { return ".filtergraph"; }
    static const char* getFilenameWildcard()    { return "*.filtergraph"; }

    //==============================================================================
    void newDocument();
    String getDocumentTitle() override;
    Result loadDocument (const File& file) override;
    Result saveDocument (const File& file) override;
    File getLastDocumentOpened() override;
    void setLastDocumentOpened (const File& file) override;

    static File getDefaultGraphDocumentOnMobile();

    /** Where a graph that hasn't been given a file is saved: next to the app's settings. */
    static File getDefaultGraphFile();

    //==============================================================================
    AudioProcessorGraph graph;

private:
    //==============================================================================
    AudioPluginFormatManager& formatManager;
    KnownPluginList& knownPlugins;
    OwnedArray<PluginWindow> activePluginWindows;
    ScopedMessageBox messageBox;

    NodeID lastUID;
    NodeID getNextUID() noexcept;

    std::atomic<bool> hasUnsavedChanges { false };
    std::atomic<uint32> lastChangeTime { 0 };
    String lastSavedText;     // what was last written, so changes that don't change anything aren't

    void listenToNodes();
    void timerCallback() override;

    void createNodeFromXml (const XmlElement&);
    void addPluginCallback (std::unique_ptr<AudioPluginInstance>,
                            const String& error,
                            Point<double>,
                            PluginDescriptionAndPreference::UseARA useARA,
                            std::function<void (NodeID)> onAdded);
    void connectAudio (NodeID source, NodeID destination);
    void disconnectAudio (NodeID source, NodeID destination);
    void changeListenerCallback (ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginGraph)
};

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
#include "../Plugins/PluginModules.h"
#include "PluginComponent.h"
#include "ConnectorComponent.h"
#include "PinComponent.h"
#include "PluginPicker.h"
#include "TextEntryPopup.h"

class MainHostWindow;
class ChainEndProcessor;


//==============================================================================
/**
    A panel that displays and edits a PluginGraph as signal chains running left to
    right, like rows of pedals, each from its own input blocks to its own output
    blocks. Each chain's input and output blocks pick channels of the audio device,
    so a guitar on one input and a mic on another can have chains of their own.

    Each step of a chain is a column of blocks running in parallel. A + on a chain's
    main line inserts a new step in series; a + in the lane under a column adds a
    plugin in parallel to that column, or another input or output at either end.
    A plugin's tile can be dragged onto a + to move the plugin there, as it is, or
    off into empty space to delete it.

    Each column feeds the whole of the next, through a rail in the gap between them.
    With free links turned on (see setLinksEnabled()), the audio goes along links
    instead, drawn from one tile's right-hand end to another's left: any block can
    feed any block after it in its chain, so the columns are where blocks sit rather
    than how they're wired. Dragging from the dot at either end of a tile to another
    block links them, and holding a link removes it.
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

        /** Called after the tiles have been updated, e.g. when one has been renamed. */
        virtual void onComponentsUpdated() {}
    };

    //==============================================================================
    GraphEditorPanel (PluginGraph& graph, KnownPluginList& knownPlugins);
    ~GraphEditorPanel() override;

    /** Inserts a plugin in series at the main-line + nearest to the given position. */
    void createNewPlugin (const PluginDescriptionAndPreference&, Point<int> position);

    /** Adds a chain underneath the others: an input and an output, connected to each other. */
    void addChain();

    /** Removes a block from its chain. A plugin that was the last in its column leaves the steps
        either side joined up; a chain's last input or output takes the whole chain with it.
    */
    void removeFromChain (AudioProcessorGraph::NodeID);

    /** True for a chain's only input or only output, which removeFromChain() removes the chain with. */
    bool removesWholeChain (AudioProcessorGraph::NodeID) const;

    /** True for a block that can be dragged somewhere else: a plugin, not a chain's input or output. */
    bool canMoveBlock (AudioProcessorGraph::NodeID) const;

    /** Called as a block's tile is dragged, with the finger's position in this panel and where on
        the tile it took hold. The tile follows the finger, only the +s it can go to are shown, and
        the one it would go to if it were let go now is lit up. Over empty space, away from every
        tile and +, the tile shows that letting go there would delete the block.
    */
    void dragBlock (PluginComponent&, Point<int> finger, Point<int> grabOffset);

    /** Called when a dragged tile is let go: the block moves to the lit-up +, is deleted over empty
        space, or otherwise goes back.
    */
    void dropBlock (PluginComponent&);

    /** Turns free links on or off (see the class description). Off, blocks can't be linked by
        dragging or unlinked by holding, and adding, moving and removing blocks join whole columns,
        as before there were links. Turning them on or off doesn't change how anything's connected.
    */
    void setLinksEnabled (bool);

    /** Whether a touch on a block's tile is on the dot at either end of it, where dragging links
        it to another block. If it is, the link starts being dragged out from there.
    */
    bool beginLink (PluginComponent&, Point<int> positionOnTile);

    /** Called as a link is dragged out, with the finger's position in this panel. The blocks it can
        link to are outlined, and the one it would link to if it were let go now is lit up.
    */
    void dragLink (Point<int> finger);

    /** Called when a link being dragged out is let go: it links the blocks if it was over one it can. */
    void endLink();

    /** Lets a name be typed for one of the chains' inputs or outputs, on the on-screen keyboard. */
    void renameChainEnd (AudioProcessorGraph::NodeID);

    /** What kind of block a node is, for the icon on its tile. */
    BlockIcon getBlockIcon (AudioProcessorGraph::NodeID) const;

    /** The <Controls> from a block's definition file: its module's, or the whole plugin's. */
    const XmlElement* getBlockControls (AudioProcessorGraph::NodeID) const;

    /** The <Editor> from a block's definition file, for its full-screen editor, or nullptr if it has none. */
    const XmlElement* getBlockEditor (AudioProcessorGraph::NodeID) const;

    /** The lane level stage that follows a block, which has its sound, or nullptr if it has none. */
    AudioProcessorGraph::Node::Ptr getLevelStage (AudioProcessorGraph::NodeID) const;

    void paint (Graphics&) override;
    void paintOverChildren (Graphics&) override;
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

private:
    class InsertButton;

    using NodeID = AudioProcessorGraph::NodeID;

    /** A step of a chain: one or more blocks running in parallel. */
    struct Column
    {
        std::vector<NodeID> nodes;      // top to bottom
        double position = 0.0;          // the nodes' shared x, which orders the columns
        std::vector<NodeID> levels;     // each node's lane level stage, if it has one

        /** Where audio from the previous column goes in: the blocks themselves. */
        const std::vector<NodeID>& getInputs() const    { return nodes; }

        /** Where audio leaves for the next column: each block's level stage, or the block if it has none. */
        std::vector<NodeID> getOutputs() const;
    };

    /** One chain: a column of its inputs, its plugins' columns, then a column of its outputs. */
    struct Chain
    {
        int id = 0;                     // its nodes' "chain" property
        std::vector<Column> columns;

        // Where layoutChains() put it
        std::vector<int> busX;          // x of the split/merge rail in each gap between columns
        std::vector<int> laneY;         // y of each column's empty parallel lane, or -1 if it has none
        std::vector<int> columnX;       // x of the middle of each column
        int top = 0, bottom = 0;
    };

    /** Audio going from one block to another. */
    struct Link
    {
        NodeID from, to;                // the blocks, as their tiles show them
        NodeID source;                  // where the audio leaves the first: its level stage, or the block itself
    };

    /** A link being dragged out from one end of a block, to link it to another. */
    struct LinkDrag
    {
        NodeID block;
        bool fromOutput = true;         // from its right-hand end, to a block after it, or its left, to one before
        Point<int> finger;
        NodeID target;                  // the block it would link to if it were let go now, if any
    };

    /** Where a + adds a block: in which chain, between what, and at what position. */
    struct InsertPoint
    {
        int chain = 0;
        std::vector<NodeID> sources, destinations;
        Point<double> position;
        bool inSeries = true;
    };

    /** Where a block is: its chain and column in chains, and its row in the column. */
    struct BlockPlace
    {
        size_t chain = 0, column = 0, row = 0;
    };

    /** A + that a dragged block could be dropped on: one in a chain's gap (in series), or under a column. */
    struct DropTarget
    {
        size_t chain = 0, index = 0;    // the gap, or the column
        bool inSeries = true;

        bool operator== (const DropTarget& other) const
        {
            return chain == other.chain && index == other.index && inSeries == other.inSeries;
        }
    };

    const PluginModuleSet* findModuleSet (const AudioProcessorGraph::Node&) const;
    ChainEndProcessor* getChainEnd (NodeID) const;
    std::vector<Chain> findChains() const;
    bool isEndColumn (const Column&) const;
    bool isAudioConnected (const Column& sources, const Column& destinations) const;
    std::vector<Link> findLinks() const;
    NodeID getOutputOf (NodeID block) const;
    std::pair<std::vector<NodeID>, std::vector<NodeID>> getNeighbours (NodeID block) const;
    bool canLink (NodeID from, NodeID to) const;
    Point<float> getEnd (NodeID block, bool isOutput) const;
    static Path getLinkPath (Point<float> start, Point<float> end);
    Path getLinkPath (const Link&) const;
    std::optional<Link> findLinkAt (Point<float>) const;
    void beginHold (Point<float>);
    void cancelHold();
    bool endHold();     // true if the hold removed a link
    bool canAddParallel (const Chain&, size_t column) const;
    void layoutChains();
    void addChainEndsToOldGraph();
    Component& getPopupParent();
    void showPluginPicker (std::function<void (const PluginChoice&)> onChosen);
    InsertPoint getSeriesInsertPoint (const Chain&, size_t gap) const;
    InsertPoint getParallelInsertPoint (const Chain&, size_t column) const;
    void insertPlugin (const PluginChoice&, const InsertPoint&);
    void addParallelEnd (const Chain&, size_t column);
    std::optional<BlockPlace> findBlock (NodeID) const;
    std::vector<DropTarget> getDropTargets() const;
    InsertButton* getButtonFor (const DropTarget&) const;
    bool canDrop (NodeID, const DropTarget&) const;
    void showDropTargets (NodeID, std::optional<DropTarget> lit);
    void moveBlock (NodeID, const DropTarget&);
    bool isEmptySpace (Point<int>) const;
    Rectangle<int> getTileBounds (NodeID) const;

    KnownPluginList& knownPlugins;
    std::map<String, PluginModuleSet> moduleSets;   // by plugin name
    std::unique_ptr<PluginPicker> picker;
    TextEntryPopup textEntry;
    std::unique_ptr<XmlElement> chainEndControls;   // the inputs' and outputs' channel choice
    std::vector<Chain> chains;
    bool linksEnabled = false;
    std::vector<Link> links;                // with free links on; none while they're off
    std::optional<LinkDrag> linkDrag;
    std::optional<Link> pendingLink;        // touched, and lit up as heldLink if it's still held a moment later
    std::optional<Link> heldLink;           // being held, to remove it
    bool removedHeldLink = false;
    OwnedArray<InsertButton> seriesButtons, parallelButtons;
    bool isUpdatingComponents = false, needsAnotherUpdate = false;
    std::vector<std::pair<size_t, size_t>> seriesTargets, parallelTargets;  // each button's chain, and gap or column

    // The block being dragged, where its tile was laid out, and the + it would be dropped on now,
    // or whether it's over empty space, where it would be deleted
    NodeID draggedBlock;
    Rectangle<int> draggedBlockHome;
    std::optional<DropTarget> litTarget;
    bool isOverEmptySpace = false;

    OwnedArray<PluginComponent> nodes;
    OwnedArray<ConnectorComponent> connectors;
    std::unique_ptr<ConnectorComponent> draggingConnector;
    std::unique_ptr<PopupMenu> menu;
    std::unique_ptr<AlertWindow> nodeActionWindow;
    ListenerList<Listener> listeners;

    //==============================================================================
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
};

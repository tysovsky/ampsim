#pragma once

#include <JuceHeader.h>
#include "PinComponent.h"
#include "BlockIcons.h"

class GraphEditorPanel;

class PluginComponent final : public Component,
                              public Timer,
                              private AudioProcessorParameter::Listener,
                              private AsyncUpdater
{
public:
    PluginComponent(GraphEditorPanel& p, AudioProcessorGraph::NodeID id);
    PluginComponent(const PluginComponent&) = delete;
    PluginComponent& operator= (const PluginComponent&) = delete;
    ~PluginComponent() override;

    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void paint(Graphics& g) override;
    Point<float> getPinPos(int index, bool isInput) const;
    void update();
    AudioProcessor* getProcessor() const;
    bool isNodeUsingARA() const;
    void showPopupMenu();
    void testStateSaveLoad();
    void showWindow(PluginWindow::Type type);
    void timerCallback() override;
    void parameterValueChanged(int, float) override;
    void parameterGestureChanged(int, bool) override {};
    void handleAsyncUpdate() override { repaint(); };
    void savePluginState();
    void loadPluginState();
    void setActive(bool active);

    /** Shows a mix bar for the lane level stage that follows this plugin, or hides it if nullptr. */
    void setLaneLevel(AudioProcessorGraph::Node::Ptr levelNode);

    /** Shows the tile red with a bin, while it's dragged where letting go would delete it. */
    void setWillBeDeleted(bool shouldShowDeleting);

    /** The plugin's name, when the tile shows one of its modules. */
    const String& getSubtitle() const noexcept { return subtitle; }

    /** What kind of block it is. */
    BlockIcon getIcon() const noexcept { return icon; }

    const AudioProcessorGraph::NodeID pluginID;


private:
    AudioParameterFloat* getLaneLevelParameter() const;
    Rectangle<int> getMixBarArea() const;
    void setLaneLevelFromPosition(int x);

    // Held so the level stage outlives this component, which keeps its parameter listener valid
    AudioProcessorGraph::Node::Ptr laneLevelNode;

    // The same for the plugin itself: a plugin that outlived its tile (e.g. while its window was
    // still open) would otherwise be left calling a deleted tile when it was bypassed
    AudioProcessorGraph::Node::Ptr pluginNode;
    bool isAdjustingLevel = false;

    // Dragging a plugin's tile further than this picks it up, to drop on a + somewhere else
    static constexpr int dragDistance = 12;
    bool canBeDragged = false, isBeingDragged = false, willBeDeleted = false;
    Point<int> grabOffset;     // where on the tile it was picked up

    GraphEditorPanel& panel;
    PluginGraph& graph;
    OwnedArray<PinComponent> pins;
    int numInputs = 0, numOutputs = 0;
    int pinSize = 32;
    int labelPadding = 6;
    Point<int> originalPos;
    Font font{ 16.0f, Font::bold };
    String subtitle;    // the plugin's name, when the tile shows one of its modules
    BlockIcon icon = BlockIcon::plugin;
    int numIns = 0, numOuts = 0;
    DropShadowEffect shadow;
    std::unique_ptr<PopupMenu> menu;
    std::unique_ptr<AlertWindow> nodeActionWindow;
    std::unique_ptr<FileChooser> fileChooser;
    const String formatSuffix = getFormatSuffix(getProcessor());
    bool isActive = false;

};
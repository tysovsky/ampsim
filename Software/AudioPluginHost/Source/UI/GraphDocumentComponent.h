#pragma once

#include <JuceHeader.h>
#include "../Plugins/PluginGraph.h"
#include "GraphEditorPanel.h"
#include "PluginComponent.h"
#include "PluginControlsPanel.h"
#include "AudioSettingsPopup.h"

//==============================================================================
/**
    A panel that embeds a GraphEditorPanel with a midi keyboard at the bottom.

    It also manages the graph itself, and plays it.
*/
class GraphDocumentComponent final : public Component,
    public DragAndDropTarget,
    public DragAndDropContainer,
    private ChangeListener,
    private GraphEditorPanel::Listener
{
public:
    GraphDocumentComponent(AudioPluginFormatManager& formatManager,
        AudioDeviceManager& deviceManager,
        KnownPluginList& pluginList);

    ~GraphDocumentComponent() override;

    //==============================================================================
    void createNewPlugin(const PluginDescriptionAndPreference&, Point<int> position);
    void setDoublePrecision(bool doublePrecision);
    bool closeAnyOpenPluginWindows();

    //==============================================================================
    std::unique_ptr<PluginGraph> graph;

    void resized() override;
    void releaseGraph();

    //==============================================================================
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;

    //==============================================================================
    void activeComponentChanged(PluginComponent* activeComponent);

    //==============================================================================
    std::unique_ptr<GraphEditorPanel> graphPanel;
    std::unique_ptr<MidiKeyboardComponent> keyboardComp;

    //==============================================================================
    void showSettings();
    void showPluginList();
    void hideLastSidePanel();

    BurgerMenuComponent burgerMenu;

private:
    //==============================================================================
    AudioDeviceManager& deviceManager;
    KnownPluginList& pluginList;

    AudioProcessorPlayer graphPlayer;
    MidiKeyboardState keyState;
    MidiOutput* midiOutput = nullptr;

    struct TooltipBar;
    std::unique_ptr<TooltipBar> statusBar;

    class TitleBarComponent;
    std::unique_ptr<TitleBarComponent> titleBarComponent;

    //==============================================================================
    struct PluginListBoxModel;
    std::unique_ptr<PluginListBoxModel> pluginListBoxModel;

    ListBox pluginListBox;

    SidePanel pluginListSidePanel{ "Plugins", 250, false };

    PluginComponent* activePlugin = nullptr;

    std::unique_ptr<PluginControlsPanel> topPluginControlPanel;
    std::unique_ptr<PluginControlsPanel> bottomPluginControlPanel;

    std::unique_ptr<AudioSettingsPopup> settingsPopup;

    //==============================================================================
    void changeListenerCallback(ChangeBroadcaster*) override;

    //==============================================================================
    void onActiveComponentChange(PluginComponent* activeComponent) override;
    void onNodeDeleted(PluginComponent* removedComponent) override;

    void init();
    void updateMidiOutput();
    void saveAudioDeviceState();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphDocumentComponent)
};

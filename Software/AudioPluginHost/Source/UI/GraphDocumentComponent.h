#pragma once

#include <JuceHeader.h>
#include "../Plugins/PluginGraph.h"
#include "GraphEditorPanel.h"
#include "PluginComponent.h"
#include "BlockControlsPanel.h"
#include "BlockEditor.h"
#include "AudioSettingsPopup.h"
#include "RecordingsPopup.h"
#include "../Audio/Recorder.h"

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
    void showRecordings();
    void hideLastSidePanel();

    /** Starts recording each output the chains send to, or stops recording. */
    void toggleRecording();

    BurgerMenuComponent burgerMenu;

private:
    //==============================================================================
    AudioDeviceManager& deviceManager;
    KnownPluginList& pluginList;

    AudioProcessorPlayer graphPlayer;
    Recorder recorder{ graphPlayer };   // plays the graph to the device, recording what it sends
    MidiKeyboardState keyState;         // the on-screen keyboard's notes, which go to the instruments

    struct InstrumentMidi;
    std::unique_ptr<InstrumentMidi> instrumentMidi;
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

    std::unique_ptr<BlockControlsPanel> blockControls;

    // The chains scroll, when there are too many to fit at a size that can be tapped
    Viewport chainViewport;

    std::unique_ptr<AudioSettingsPopup> settingsPopup;
    std::unique_ptr<RecordingsPopup> recordingsPopup;
    std::unique_ptr<BlockEditor> blockEditor;     // the selected block's full-screen editor, if it has one
    ScopedMessageBox messageBox;

    //==============================================================================
    void changeListenerCallback(ChangeBroadcaster*) override;

    //==============================================================================
    void onActiveComponentChange(PluginComponent* activeComponent) override;
    void onNodeDeleted(PluginComponent* removedComponent) override;
    void onComponentsUpdated() override;

    void init();
    void showBlockControls();
    void updateMidiOutput();
    void saveAudioDeviceState();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphDocumentComponent)
};

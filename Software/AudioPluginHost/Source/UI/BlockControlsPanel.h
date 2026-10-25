#pragma once

#include <JuceHeader.h>
#include "BlockIcons.h"
#include "ControlStrip.h"
#include "ControllerConfig.h"
#include "TouchLookAndFeel.h"

//==============================================================================
/**
    The selected block's controls, along the bottom of the window: what the block
    is, on the left, then its controls (see ControlStrip), set out in the
    <Controls> of its module (or of the whole plugin) in its definition file.

    The controls follow the MIDI controller's knobs and switches, numbered as in
    Controller.xml. A switch that none of the block's controls use turns the block
    itself on and off instead (see onSwitchBlock). A block with a full-screen
    editor (see BlockEditor) has an Edit button to open it.
*/
class BlockControlsPanel final : public Component,
                                 private MidiInputCallback
{
public:
    explicit BlockControlsPanel (AudioDeviceManager&);
    ~BlockControlsPanel() override;

    /** Shows a block's controls, from a <Controls> element, with an Edit button if it has an editor. */
    void showBlock (AudioProcessorGraph::Node::Ptr, const XmlElement& controls,
                    const String& name, const String& subtitle, BlockIcon, bool hasEditor = false);

    /** Takes the controls down. */
    void clear();

    /** Changes the name shown for the block, e.g. after an input has been given another channel. */
    void setBlockName (const String& name, const String& subtitle);

    bool isShowingBlock() const noexcept    { return node != nullptr; }

    /** Called when the close button is tapped. */
    std::function<void()> onClose;

    /** Called when the Edit button is tapped, to open the block's editor. */
    std::function<void()> onEdit;

    /** Called after one of the controls' <View>s has been changed (see ControlStrip). */
    std::function<void()> onViewChange;

    /** Called when the controller's switch is used on a block none of whose controls use it, to
        turn the block on or off (by bypassing it). A toggling switch asks for the opposite of
        whether the block is on now. */
    std::function<void (bool shouldBeOn)> onSwitchBlock;

    void paint (Graphics&) override;
    void resized() override;

private:
    Rectangle<int> getInfoArea() const;

    void handleIncomingMidiMessage (MidiInput*, const MidiMessage&) override;
    void handleControllerMessage (const MidiMessage&);

    AudioDeviceManager& deviceManager;
    const ControllerConfig controller;
    TouchLookAndFeel lookAndFeel;

    AudioProcessorGraph::Node::Ptr node;    // held so the plugin outlives its controls
    String name, subtitle;
    BlockIcon icon = BlockIcon::plugin;
    ControlStrip strip;
    TextButton closeButton, editButton { "Edit" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockControlsPanel)
};

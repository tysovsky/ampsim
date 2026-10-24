#pragma once

#include <JuceHeader.h>
#include "BlockIcons.h"
#include "ControllerConfig.h"
#include "TouchLookAndFeel.h"

//==============================================================================
/**
    The selected block's controls, along the bottom of the window. They're set out
    in the block's definition file, in the <Controls> of its module (or of the
    whole plugin), and laid out left to right:

      <Knob param="Amp Gain" label="Gain" knob="1"/>            a knob
      <Fader param="EQ 250 Hz" label="250"/>                     a fader, e.g. for a graphic EQ's bands
      <Switch param="Boost Active" label="Boost" switch="1"/>    on and off
      <Choice param="Amp Type" label="Amp"/>                     a button for each of a parameter's steps,
                                                                 or a list with style="list" (the default
                                                                 for more than four); labels="A|B|C" replaces
                                                                 the plugin's own names for the steps
      <Show param="Amp Type" value="Roses"> ... </Show>          controls only shown while a parameter has a
                                                                 value, or a step with step="0", "1", ...

    param is the plugin's name for the parameter, and label is what's shown, which
    is the parameter's name if there isn't one. knob and switch connect a control
    to a knob or switch on the MIDI controller, numbered as in Controller.xml. A
    knob also turns a Choice through its steps, and a switch picks a Choice's
    first or last step. Controls inside a hidden <Show> don't follow the
    controller, so the same knob can do different things for, say, each amp.
    A switch that none of the block's controls use turns the block itself on
    and off instead (see onSwitchBlock).
*/
class BlockControlsPanel final : public Component,
                                 private MidiInputCallback,
                                 private Timer
{
public:
    explicit BlockControlsPanel (AudioDeviceManager&);
    ~BlockControlsPanel() override;

    /** Shows a block's controls, from a <Controls> element. */
    void showBlock (AudioProcessorGraph::Node::Ptr, const XmlElement& controls,
                    const String& name, const String& subtitle, BlockIcon);

    /** Takes the controls down. */
    void clear();

    /** Changes the name shown for the block, e.g. after an input has been given another channel. */
    void setBlockName (const String& name, const String& subtitle);

    bool isShowingBlock() const noexcept    { return node != nullptr; }

    /** Called when the close button is tapped. */
    std::function<void()> onClose;

    /** Called when the controller's switch is used on a block none of whose controls use it, to
        turn the block on or off (by bypassing it). A toggling switch asks for the opposite of
        whether the block is on now. */
    std::function<void (bool shouldBeOn)> onSwitchBlock;

    void paint (Graphics&) override;
    void resized() override;

private:
    struct Condition;
    class Control;
    class KnobControl;
    class SwitchControl;
    class ChoiceControl;

    void addControls (const XmlElement&, const std::vector<Condition>&);
    void updateVisibility();
    Rectangle<int> getInfoArea() const;

    void handleIncomingMidiMessage (MidiInput*, const MidiMessage&) override;
    void handleControllerMessage (const MidiMessage&);
    void timerCallback() override;

    AudioDeviceManager& deviceManager;
    const ControllerConfig controller;
    TouchLookAndFeel lookAndFeel;

    AudioProcessorGraph::Node::Ptr node;    // held so the plugin outlives its controls
    String name, subtitle;
    BlockIcon icon = BlockIcon::plugin;
    OwnedArray<Control> controls;
    TextButton closeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockControlsPanel)
};

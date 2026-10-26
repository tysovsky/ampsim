#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    A row of a block's controls, as set out in its definition file, laid out left
    to right across the strip and kept up to date with the plugin:

      <Knob param="Amp Gain" label="Gain" knob="1"/>            a knob
      <Fader param="EQ 250 Hz" label="250"/>                     a fader, e.g. for a graphic EQ's bands
      <Switch param="Amp Bright" label="Bright" switch="1"/>     on and off
      <Choice param="Amp Type" label="Amp"/>                     a button for each of a parameter's steps,
                                                                 or a list with style="list" (the default
                                                                 for more than four, unless style="buttons");
                                                                 labels="A|B|C" replaces the plugin's own
                                                                 names for the steps
      <Trigger param="Next Preset" text="Next"/>                 a button that pulses a parameter, e.g. one
                                                                 that steps through the plugin's presets
      <Sounds previous="Previous Preset" next="Next Preset"/>    the sound the block is set to, from the ones
                                                                 saved for its plugin, like a dropdown list
                                                                 (see SoundsPopup); a switch steps through
                                                                 them, and arrows pulse previous and next, if
                                                                 given, to step through the plugin's presets
      <View name="engine1" label="Type" options="A|B"/>          picks which of the <Show view="..."> controls
                                                                 are shown, for something the plugin doesn't
                                                                 say, such as which kind of engine a preset
                                                                 uses; remembered with the block
      <Show param="Amp Type" value="Roses"> ... </Show>          controls only shown while a parameter has a
                                                                 value, or a step with step="0", "1", ...
      <Show view="engine1" value="B"> ... </Show>                or while a <View> is showing one of its options

    param is the plugin's name for the parameter, and label is what's shown, which
    is the parameter's name if there isn't one. Where a plugin gives two parameters
    the same name, occurrence="2" means the second. knob and switch connect a
    control to a knob or switch on the MIDI controller, numbered as in
    Controller.xml (see handleKnob() and handleSwitch()). Controls inside a hidden
    <Show> don't follow the controller, so the same knob can do different things
    for, say, each amp.
*/
class ControlStrip final : public Component,
                           private Timer
{
public:
    ControlStrip();
    ~ControlStrip() override;

    /** Builds the controls for a block, from a <Controls> element or one of an editor's rows. */
    void setControls (AudioProcessorGraph::Node::Ptr, const XmlElement& controls);

    /** Takes the controls down. */
    void clear();

    /** A controller knob has moved: the shown controls on it follow it. Returns true if there were any. */
    bool handleKnob (int number, float position);

    /** A controller switch has changed, or been pressed if it toggles. Returns true if any shown
        control is on it. */
    bool handleSwitch (int number, bool toggles, bool isOn);

    /** Called after a <View> has been changed, or a <Sounds> has set the block to another sound,
        which changes a property of the block, not the plugin. */
    std::function<void()> onViewChange;

    void resized() override;

private:
    struct Condition;
    class Control;
    class KnobControl;
    class SwitchControl;
    class ChoiceControl;
    class TriggerControl;
    class SoundsControl;

    void addControls (const XmlElement&, const std::vector<Condition>&);
    void updateVisibility();
    void timerCallback() override;

    AudioProcessorGraph::Node::Ptr node;    // held so the plugin outlives its controls
    OwnedArray<Control> controls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlStrip)
};

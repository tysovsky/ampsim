#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    The hardware knobs and switches, and the MIDI each one sends, as set out in
    Controller.xml in the plugin definitions folder. Block controls refer to them
    by number: <Knob ... knob="1"/>, <Switch ... switch="1"/>.

    <Controller device="Seeed XIAO M0">
      <Knob number="1" cc="10"/>              knobs send a controller (0-127)
      <Switch number="1" note="64"/>          switches send a note, or a controller (on at 64 and over)
      <Switch number="2" cc="80" mode="toggle"/>
    </Controller>

    Each can have a channel="1" to "16"; without one, any channel is accepted. A
    switch is on while it's down (note on) unless it has mode="toggle", where each
    press turns it on or off. With a device, only that MIDI input is listened to,
    and it's turned on at startup; without one, every MIDI input that's turned on
    in the settings is.
*/
class ControllerConfig
{
public:
    /** Reads Controller.xml from the first definition folder that has one. */
    static ControllerConfig load();
    static ControllerConfig load (const File&);

    /** The MIDI input the controller is on, or empty for any. */
    const String& getDeviceName() const noexcept    { return deviceName; }

    struct KnobMove     { int number; float position; };         // position from 0 to 1
    struct SwitchChange { int number; bool isOn; bool toggles; };

    std::optional<KnobMove> findKnob (const MidiMessage&) const;
    std::optional<SwitchChange> findSwitch (const MidiMessage&) const;

private:
    struct Mapping
    {
        int number = 0, channel = 0, cc = -1, note = -1;
        bool toggles = false;

        bool matchesChannel (const MidiMessage& m) const    { return channel == 0 || m.getChannel() == channel; }
    };

    String deviceName;
    std::vector<Mapping> knobs, switches;
};

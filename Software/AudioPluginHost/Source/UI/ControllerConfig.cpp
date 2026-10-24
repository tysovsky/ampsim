#include "ControllerConfig.h"
#include "../Plugins/PluginModules.h"

ControllerConfig ControllerConfig::load()
{
    for (auto& folder : getPluginDefinitionFolders())
    {
        const auto file = folder.getChildFile ("Controller.xml");

        if (file.existsAsFile())
            return load (file);
    }

    return {};
}

ControllerConfig ControllerConfig::load (const File& file)
{
    ControllerConfig result;
    const auto xml = XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName ("Controller"))
    {
        DBG ("Controller config: can't read " << file.getFullPathName());
        return result;
    }

    result.deviceName = xml->getStringAttribute ("device");

    const auto readMapping = [] (const XmlElement& e) -> Mapping
    {
        return { e.getIntAttribute ("number"), e.getIntAttribute ("channel"), e.getIntAttribute ("cc", -1),
                 e.getIntAttribute ("note", -1), e.getStringAttribute ("mode").equalsIgnoreCase ("toggle") };
    };

    for (auto* knob : xml->getChildWithTagNameIterator ("Knob"))
        result.knobs.push_back (readMapping (*knob));

    for (auto* button : xml->getChildWithTagNameIterator ("Switch"))
        result.switches.push_back (readMapping (*button));

    return result;
}

std::optional<ControllerConfig::KnobMove> ControllerConfig::findKnob (const MidiMessage& message) const
{
    if (message.isController())
        for (auto& knob : knobs)
            if (knob.cc == message.getControllerNumber() && knob.matchesChannel (message))
                return KnobMove { knob.number, (float) message.getControllerValue() / 127.0f };

    return {};
}

std::optional<ControllerConfig::SwitchChange> ControllerConfig::findSwitch (const MidiMessage& message) const
{
    for (auto& button : switches)
    {
        if (! button.matchesChannel (message))
            continue;

        if (button.note >= 0 && (message.isNoteOn() || message.isNoteOff()) && message.getNoteNumber() == button.note)
            return SwitchChange { button.number, message.isNoteOn(), button.toggles };

        if (button.cc >= 0 && message.isController() && message.getControllerNumber() == button.cc)
            return SwitchChange { button.number, message.getControllerValue() >= 64, button.toggles };
    }

    return {};
}

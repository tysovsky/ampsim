#include "PluginModules.h"

namespace
{
    PluginModule::Settings readSettings (const XmlElement& element)
    {
        PluginModule::Settings settings;

        for (auto* set : element.getChildWithTagNameIterator ("Set"))
            settings.emplace_back (set->getStringAttribute ("param"), set->getStringAttribute ("value"));

        return settings;
    }

    PluginModule readModule (const XmlElement& element, const String& group)
    {
        return { group, element.getStringAttribute ("name"), readSettings (element) };
    }

    AudioProcessorParameter* findParameter (AudioProcessor& processor, const String& name)
    {
        for (auto* parameter : processor.getParameters())
            if (parameter->getName (128) == name)
                return parameter;

        return nullptr;
    }

    // Finds the normalised value for a value's text: one of the parameter's steps ("On", "Roses"),
    // or failing that, a number given directly as a normalised value
    std::optional<float> findValue (const AudioProcessorParameter& parameter, const String& text)
    {
        const int numSteps = parameter.getNumSteps();

        if (parameter.isDiscrete() && numSteps > 1 && numSteps <= 1024)
        {
            for (int step = 0; step < numSteps; ++step)
            {
                const auto value = (float) step / (float) (numSteps - 1);

                if (parameter.getText (value, 128).equalsIgnoreCase (text))
                    return value;
            }
        }

        if (text.containsOnly ("0123456789.") && text.isNotEmpty())
            return jlimit (0.0f, 1.0f, text.getFloatValue());

        return {};
    }
}

//==============================================================================
PluginModuleSet PluginModuleSet::load (const File& definitionFile)
{
    PluginModuleSet result;

    const auto xml = XmlDocument::parse (definitionFile);

    if (xml == nullptr)
        return result;

    result.pluginName = xml->getStringAttribute ("name");
    const auto* modulesElement = xml->getChildByName ("Modules");

    if (modulesElement == nullptr)
        return result;

    if (const auto* off = modulesElement->getChildByName ("Off"))
        result.offSettings = readSettings (*off);

    for (auto* child : modulesElement->getChildIterator())
    {
        if (child->hasTagName ("Module"))
            result.modules.push_back (readModule (*child, {}));
        else if (child->hasTagName ("Group"))
            for (auto* module : child->getChildWithTagNameIterator ("Module"))
                result.modules.push_back (readModule (*module, child->getStringAttribute ("name")));
    }

    return result;
}

std::map<String, PluginModuleSet> PluginModuleSet::loadAll (const File& folder)
{
    std::map<String, PluginModuleSet> result;

    for (const auto& file : folder.findChildFiles (File::findFiles, false, "*.xml"))
    {
        auto set = load (file);

        if (! set.isEmpty())
            result.emplace (set.pluginName, std::move (set));
    }

    return result;
}

const PluginModule* PluginModuleSet::findModule (const String& name) const
{
    for (auto& module : modules)
        if (module.name == name)
            return &module;

    return nullptr;
}

void PluginModuleSet::apply (AudioProcessor& processor, const PluginModule& module) const
{
    applySettings (processor, offSettings);
    applySettings (processor, module.settings);
}

void PluginModuleSet::applySettings (AudioProcessor& processor, const PluginModule::Settings& settings)
{
    for (const auto& [name, text] : settings)
    {
        auto* parameter = findParameter (processor, name);
        const auto value = parameter != nullptr ? findValue (*parameter, text) : std::nullopt;

        // A definition file that doesn't match the plugin (e.g. after the plugin was updated)
        if (! value.has_value())
        {
            DBG ("Plugin module: can't set " << name << " to " << text);
            continue;
        }

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (*value);
        parameter->endChangeGesture();
    }
}

//==============================================================================
Array<File> getPluginDefinitionFolders()
{
    return { File::getSpecialLocation (File::currentExecutableFile).getSiblingFile ("PluginUIDefinitions"),
             File ("C:\\Program Files\\StudioHost\\PluginControls") };
}

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

#include <JuceHeader.h>
#include "../UI/MainHostWindow.h"
#include "PluginGraph.h"
#include "InternalPlugins.h"
#include "../UI/GraphEditorPanel.h"

static std::unique_ptr<ScopedDPIAwarenessDisabler> makeDPIAwarenessDisablerForPlugin (const PluginDescription& desc)
{
    return shouldAutoScalePlugin (desc) ? std::make_unique<ScopedDPIAwarenessDisabler>()
                                        : nullptr;
}

//==============================================================================
PluginGraph::PluginGraph (AudioPluginFormatManager& fm, KnownPluginList& kpl)
    : FileBasedDocument (getFilenameSuffix(),
                         getFilenameWildcard(),
                         "Load a graph",
                         "Save a graph"),
      formatManager (fm),
      knownPlugins (kpl)
{
    newDocument();
    graph.addListener (this);
    startTimer (250);
}

PluginGraph::~PluginGraph()
{
    stopTimer();

    for (auto* node : graph.getNodes())
        node->getProcessor()->removeListener (this);

    graph.removeListener (this);
    graph.removeChangeListener (this);
    graph.clear();
}

PluginGraph::NodeID PluginGraph::getNextUID() noexcept
{
    return PluginGraph::NodeID (++(lastUID.uid));
}

//==============================================================================
void PluginGraph::changeListenerCallback (ChangeBroadcaster*)
{
    changed();
    markChanged();
    listenToNodes();

    for (int i = activePluginWindows.size(); --i >= 0;)
        if (! graph.getNodes().contains (activePluginWindows.getUnchecked (i)->node))
            activePluginWindows.remove (i);
}

AudioProcessorGraph::Node::Ptr PluginGraph::getNodeForName (const String& name) const
{
    for (auto* node : graph.getNodes())
        if (auto p = node->getProcessor())
            if (p->getName().equalsIgnoreCase (name))
                return node;

    return nullptr;
}

void PluginGraph::addPlugin (const PluginDescriptionAndPreference& desc, Point<double> pos,
                             std::function<void (NodeID)> onAdded)
{
    std::shared_ptr<ScopedDPIAwarenessDisabler> dpiDisabler = makeDPIAwarenessDisablerForPlugin (desc.pluginDescription);

    formatManager.createPluginInstanceAsync (desc.pluginDescription,
                                             graph.getSampleRate(),
                                             graph.getBlockSize(),
                                             [this, pos, dpiDisabler, useARA = desc.useARA, onAdded] (std::unique_ptr<AudioPluginInstance> instance, const String& error)
                                             {
                                                 addPluginCallback (std::move (instance), error, pos, useARA, onAdded);
                                             });
}

void PluginGraph::insertPlugin (const PluginDescriptionAndPreference& desc,
                                std::vector<NodeID> sources, std::vector<NodeID> destinations,
                                Point<double> position, bool inSeries,
                                std::function<void (AudioProcessorGraph::Node&)> setUp)
{
    addPlugin (desc, position, [this, sources, destinations, inSeries, position, setUp] (NodeID added)
    {
        if (setUp != nullptr)
            if (auto* node = graph.getNodeForId (added))
                setUp (*node);

        auto output = added;

        if (auto levelNode = graph.addNode (std::make_unique<LaneLevelProcessor>()))
        {
            levelNode->properties.set ("x", position.x);
            levelNode->properties.set ("y", position.y);

            if (auto* node = graph.getNodeForId (added))
                levelNode->properties.set ("chain", node->properties["chain"]);

            connectAudio (added, levelNode->nodeID);
            output = levelNode->nodeID;
        }

        for (auto source : sources)
        {
            if (inSeries)
                for (auto destination : destinations)
                    disconnectAudio (source, destination);

            connectAudio (source, added);
        }

        for (auto destination : destinations)
            connectAudio (output, destination);
    });
}

void PluginGraph::removeFromChain (const std::vector<NodeID>& nodes, const std::vector<NodeID>& sources,
                                   const std::vector<NodeID>& destinations, bool joinNeighbours)
{
    for (auto node : nodes)
        graph.removeNode (node);

    if (joinNeighbours)
        joinAcrossGap (sources, destinations);
}

bool PluginGraph::connectBlocks (NodeID source, NodeID destination)
{
    // Not round in a loop: the destination mustn't already feed the source, however indirectly
    if (source == destination || graph.isAnInputTo (destination, source))
        return false;

    connectAudio (source, destination);
    return true;
}

void PluginGraph::disconnectBlocks (NodeID source, NodeID destination)
{
    disconnectAudio (source, destination);
}

void PluginGraph::joinAcrossGap (const std::vector<NodeID>& sources, const std::vector<NodeID>& destinations)
{
    // Only where a source no longer reaches a destination some other way, so taking one of two
    // parallel blocks out doesn't add a path straight past the other
    for (auto source : sources)
        for (auto destination : destinations)
            if (! graph.isAnInputTo (source, destination))
                connectBlocks (source, destination);
}

void PluginGraph::moveInChain (NodeID plugin, NodeID level, int chain, Point<double> position,
                               const std::vector<NodeID>& oldSources, const std::vector<NodeID>& oldDestinations,
                               bool joinOldNeighbours, const std::vector<NodeID>& sources,
                               const std::vector<NodeID>& destinations, bool inSeries)
{
    const auto output = graph.getNodeForId (level) != nullptr ? level : plugin;

    // Out of where it was: the audio into the plugin, and out of its level stage, but not between the two
    for (auto& connection : graph.getConnections())
        if (! connection.source.isMIDI() && (connection.destination.nodeID == plugin || connection.source.nodeID == output))
            graph.removeConnection (connection);

    if (joinOldNeighbours)
        joinAcrossGap (oldSources, oldDestinations);

    for (auto id : { plugin, level })
    {
        if (auto* node = graph.getNodeForId (id))
        {
            node->properties.set ("x", position.x);
            node->properties.set ("y", position.y);
            node->properties.set ("chain", chain);
        }
    }

    for (auto source : sources)
    {
        if (inSeries)
            for (auto destination : destinations)
                disconnectAudio (source, destination);

        connectAudio (source, plugin);
    }

    for (auto destination : destinations)
        connectAudio (output, destination);

    // The new position on its own isn't a change to the graph, so the chain view's told
    changed();
    markChanged();
}

PluginGraph::NodeID PluginGraph::addChainEnd (bool isInput, int chain, Point<double> position, int channelChoice,
                                              const std::vector<NodeID>& sources, const std::vector<NodeID>& destinations)
{
    std::unique_ptr<ChainEndProcessor> processor;

    if (isInput)
        processor = std::make_unique<ChainInputProcessor>();
    else
        processor = std::make_unique<ChainOutputProcessor>();

    processor->getChannel() = channelChoice;

    auto node = graph.addNode (std::move (processor));

    if (node == nullptr)
        return {};

    node->properties.set ("x", position.x);
    node->properties.set ("y", position.y);
    node->properties.set ("chain", chain);

    for (auto source : sources)
        connectAudio (source, node->nodeID);

    for (auto destination : destinations)
        connectAudio (node->nodeID, destination);

    connectDeviceChannels();
    return node->nodeID;
}

void PluginGraph::connectDeviceChannels()
{
    using IOProcessor = AudioProcessorGraph::AudioGraphIOProcessor;

    AudioProcessorGraph::Node* deviceInput = nullptr;
    AudioProcessorGraph::Node* deviceOutput = nullptr;
    AudioProcessorGraph::Node* midiInput = nullptr;

    for (auto* node : graph.getNodes())
    {
        if (auto* io = dynamic_cast<IOProcessor*> (node->getProcessor()))
        {
            if (io->getType() == IOProcessor::audioInputNode)   deviceInput = node;
            if (io->getType() == IOProcessor::audioOutputNode)  deviceOutput = node;
            if (io->getType() == IOProcessor::midiInputNode)    midiInput = node;
        }
    }

    // Connections to channels a new device doesn't have any more. Not with removeIllegalConnections(),
    // which reports a change even when there's nothing to remove, and this is called after every change.
    for (auto& connection : graph.getConnections())
        if (! graph.isConnectionLegal (connection))
            graph.removeConnection (connection);

    const auto connect = [this] (AudioProcessorGraph::Connection connection)
    {
        if (! graph.isConnected (connection))
            graph.addConnection (connection);
    };

    for (auto* node : graph.getNodes())
    {
        auto* processor = node->getProcessor();

        if (deviceInput != nullptr && dynamic_cast<ChainInputProcessor*> (processor) != nullptr)
            for (int channel = 0; channel < jmin (deviceInput->getProcessor()->getTotalNumOutputChannels(),
                                                  processor->getTotalNumInputChannels()); ++channel)
                connect ({ { deviceInput->nodeID, channel }, { node->nodeID, channel } });

        if (deviceOutput != nullptr && dynamic_cast<ChainOutputProcessor*> (processor) != nullptr)
            for (int channel = 0; channel < jmin (processor->getTotalNumOutputChannels(),
                                                  deviceOutput->getProcessor()->getTotalNumInputChannels()); ++channel)
                connect ({ { node->nodeID, channel }, { deviceOutput->nodeID, channel } });

        // Instruments play the MIDI from the keyboards (effects don't get it, so notes can't set
        // off whatever they've mapped to MIDI)
        if (midiInput != nullptr && processor->acceptsMidi())
            if (auto* plugin = dynamic_cast<AudioPluginInstance*> (processor))
                if (plugin->getPluginDescription().isInstrument)
                    connect ({ { midiInput->nodeID, AudioProcessorGraph::midiChannelIndex },
                               { node->nodeID, AudioProcessorGraph::midiChannelIndex } });
    }
}

void PluginGraph::disconnectAudioDevice()
{
    using IOProcessor = AudioProcessorGraph::AudioGraphIOProcessor;

    const auto isDevice = [this] (NodeID id, IOProcessor::IODeviceType type)
    {
        auto* node = graph.getNodeForId (id);
        auto* io = node != nullptr ? dynamic_cast<IOProcessor*> (node->getProcessor()) : nullptr;
        return io != nullptr && io->getType() == type;
    };

    for (auto& connection : graph.getConnections())
        if (! connection.source.isMIDI()
             && (isDevice (connection.source.nodeID, IOProcessor::audioInputNode)
                  || isDevice (connection.destination.nodeID, IOProcessor::audioOutputNode)))
            graph.removeConnection (connection);
}

void PluginGraph::connectAudio (NodeID source, NodeID destination)
{
    auto* sourceNode = graph.getNodeForId (source);
    auto* destinationNode = graph.getNodeForId (destination);

    if (sourceNode == nullptr || destinationNode == nullptr)
        return;

    const int numOutputs = sourceNode->getProcessor()->getTotalNumOutputChannels();
    const int numInputs  = destinationNode->getProcessor()->getTotalNumInputChannels();

    // Connect matching channels. A mono source feeds every input, so a guitar on a single
    // channel reaches both sides of a stereo plugin.
    for (int channel = 0; channel < numInputs; ++channel)
    {
        const int sourceChannel = numOutputs == 1 ? 0 : channel;

        if (sourceChannel < numOutputs)
            graph.addConnection ({ { source, sourceChannel }, { destination, channel } });
    }
}

void PluginGraph::disconnectAudio (NodeID source, NodeID destination)
{
    for (auto& connection : graph.getConnections())
        if (connection.source.nodeID == source && connection.destination.nodeID == destination
             && ! connection.source.isMIDI())
            graph.removeConnection (connection);
}

void PluginGraph::addPluginCallback (std::unique_ptr<AudioPluginInstance> instance,
                                     const String& error,
                                     Point<double> pos,
                                     PluginDescriptionAndPreference::UseARA useARA,
                                     std::function<void (NodeID)> onAdded)
{
    if (instance == nullptr)
    {
        auto options = MessageBoxOptions::makeOptionsOk (MessageBoxIconType::WarningIcon,
                                                         TRANS ("Couldn't create plugin"),
                                                         error);
        messageBox = AlertWindow::showScopedAsync (options, nullptr);
    }
    else
    {
       #if JUCE_PLUGINHOST_ARA && (JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX)
        if (useARA == PluginDescriptionAndPreference::UseARA::yes
            && instance->getPluginDescription().hasARAExtension)
        {
            instance = std::make_unique<ARAPluginInstanceWrapper> (std::move (instance));
        }
       #endif

        instance->enableAllBuses();

        if (auto node = graph.addNode (std::move (instance)))
        {
            node->properties.set ("x", pos.x);
            node->properties.set ("y", pos.y);
            node->properties.set ("useARA", useARA == PluginDescriptionAndPreference::UseARA::yes);
            changed();

            if (onAdded != nullptr)
                onAdded (node->nodeID);
        }
    }
}

void PluginGraph::setNodePosition (NodeID nodeID, Point<double> pos)
{
    if (auto* n = graph.getNodeForId (nodeID))
    {
        n->properties.set ("x", jlimit (0.0, 1.0, pos.x));
        n->properties.set ("y", jlimit (0.0, 1.0, pos.y));
    }
}

void PluginGraph::setNodeBypassed (NodeID nodeID, bool shouldBeBypassed)
{
    if (auto* n = graph.getNodeForId (nodeID))
    {
        n->setBypassed (shouldBeBypassed);
        changed();
        markChanged();
    }
}

Point<double> PluginGraph::getNodePosition (NodeID nodeID) const
{
    if (auto* n = graph.getNodeForId (nodeID))
        return { static_cast<double> (n->properties ["x"]),
                 static_cast<double> (n->properties ["y"]) };

    return {};
}

//==============================================================================
void PluginGraph::clear()
{
    closeAnyOpenPluginWindows();
    graph.clear();
    changed();
}

PluginWindow* PluginGraph::getOrCreateWindowFor (AudioProcessorGraph::Node* node, PluginWindow::Type type)
{
    jassert (node != nullptr);

   #if JUCE_IOS || JUCE_ANDROID
    closeAnyOpenPluginWindows();
   #else
    for (auto* w : activePluginWindows)
        if (w->node.get() == node && w->type == type)
            return w;
   #endif

    if (auto* processor = node->getProcessor())
    {
        if (auto* plugin = dynamic_cast<AudioPluginInstance*> (processor))
        {
            auto description = plugin->getPluginDescription();

            if (! plugin->hasEditor() && description.pluginFormatName == "Internal")
            {
                getCommandManager().invokeDirectly (CommandIDs::showAudioSettings, false);
                return nullptr;
            }

            auto localDpiDisabler = makeDPIAwarenessDisablerForPlugin (description);
            return activePluginWindows.add (new PluginWindow (node, type, activePluginWindows));
        }
    }

    return nullptr;
}

bool PluginGraph::closeAnyOpenPluginWindows()
{
    bool wasEmpty = activePluginWindows.isEmpty();
    activePluginWindows.clear();
    return ! wasEmpty;
}

//==============================================================================
String PluginGraph::getDocumentTitle()
{
    if (! getFile().exists())
        return "Unnamed";

    return getFile().getFileNameWithoutExtension();
}

void PluginGraph::newDocument()
{
    clear();
    setFile ({});

    graph.removeChangeListener (this);

    InternalPluginFormat internalFormat;

    jassert (internalFormat.getAllTypes().size() > 3);

    addPlugin (PluginDescriptionAndPreference { internalFormat.getAllTypes()[0] }, { 0.5,  0.1 });
    addPlugin (PluginDescriptionAndPreference { internalFormat.getAllTypes()[1] }, { 0.25, 0.1 });
    addPlugin (PluginDescriptionAndPreference { internalFormat.getAllTypes()[2] }, { 0.5,  0.9 });
    addPlugin (PluginDescriptionAndPreference { internalFormat.getAllTypes()[3] }, { 0.25, 0.9 });

    MessageManager::callAsync ([this]
    {
        setChangedFlag (false);
        graph.addChangeListener (this);
        listenToNodes();
    });
}

Result PluginGraph::loadDocument (const File& file)
{
    if (auto xml = parseXMLIfTagMatches (file, "FILTERGRAPH"))
    {
        graph.removeChangeListener (this);
        restoreFromXml (*xml);

        MessageManager::callAsync ([this]
        {
            setChangedFlag (false);
            graph.addChangeListener (this);
            listenToNodes();
        });

        return Result::ok();
    }

    return Result::fail ("Not a valid graph file");
}

Result PluginGraph::saveDocument (const File& file)
{
    auto xml = createXml();

    if (! xml->writeTo (file, {}))
        return Result::fail ("Couldn't write to the file");

    return Result::ok();
}

File PluginGraph::getDefaultGraphFile()
{
    return getAppProperties().getUserSettings()->getFile().getSiblingFile ("Chain").withFileExtension (getFilenameSuffix());
}

//==============================================================================
void PluginGraph::markChanged() noexcept
{
    lastChangeTime = Time::getMillisecondCounter();
    hasUnsavedChanges = true;
}

void PluginGraph::listenToNodes()
{
    // Each plugin reports its own parameter and state changes. Adding a listener that's already
    // there does nothing, so this can be called after any change to what's in the graph.
    for (auto* node : graph.getNodes())
        node->getProcessor()->addListener (this);
}

void PluginGraph::timerCallback()
{
    constexpr uint32 quietTimeBeforeSaving = 1000;    // milliseconds

    if (hasUnsavedChanges && Time::getMillisecondCounter() - lastChangeTime >= quietTimeBeforeSaving)
        saveChanges();
}

void PluginGraph::saveChanges()
{
    if (! hasUnsavedChanges.exchange (false) && ! hasChangedSinceSaved())
        return;

    if (getFile() == File())
        setFile (getDefaultGraphFile());

    const auto text = createXml()->toString();

    // Plugins sometimes report changes that don't change their saved state
    if (text != lastSavedText)
    {
        if (! getFile().replaceWithText (text))
        {
            DBG ("Couldn't save the graph to " << getFile().getFullPathName());
            return;
        }

        lastSavedText = text;
    }

    setChangedFlag (false);

    // So it's the graph that's opened next time
    if (getLastDocumentOpened() != getFile())
        setLastDocumentOpened (getFile());
}

File PluginGraph::getLastDocumentOpened()
{
    RecentlyOpenedFilesList recentFiles;
    recentFiles.restoreFromString (getAppProperties().getUserSettings()
                                        ->getValue ("recentFilterGraphFiles"));

    return recentFiles.getFile (0);
}

void PluginGraph::setLastDocumentOpened (const File& file)
{
    RecentlyOpenedFilesList recentFiles;
    recentFiles.restoreFromString (getAppProperties().getUserSettings()
                                        ->getValue ("recentFilterGraphFiles"));

    recentFiles.addFile (file);

    getAppProperties().getUserSettings()
        ->setValue ("recentFilterGraphFiles", recentFiles.toString());
}

//==============================================================================
static void readBusLayoutFromXml (AudioProcessor::BusesLayout& busesLayout, AudioProcessor& plugin,
                                  const XmlElement& xml, bool isInput)
{
    auto& targetBuses = (isInput ? busesLayout.inputBuses
                                 : busesLayout.outputBuses);
    int maxNumBuses = 0;

    if (auto* buses = xml.getChildByName (isInput ? "INPUTS" : "OUTPUTS"))
    {
        for (auto* e : buses->getChildWithTagNameIterator ("BUS"))
        {
            const int busIdx = e->getIntAttribute ("index");
            maxNumBuses = jmax (maxNumBuses, busIdx + 1);

            // the number of buses on busesLayout may not be in sync with the plugin after adding buses
            // because adding an input bus could also add an output bus
            for (int actualIdx = plugin.getBusCount (isInput) - 1; actualIdx < busIdx; ++actualIdx)
                if (! plugin.addBus (isInput))
                    return;

            for (int actualIdx = targetBuses.size() - 1; actualIdx < busIdx; ++actualIdx)
                targetBuses.add (plugin.getChannelLayoutOfBus (isInput, busIdx));

            auto layout = e->getStringAttribute ("layout");

            if (layout.isNotEmpty())
                targetBuses.getReference (busIdx) = AudioChannelSet::fromAbbreviatedString (layout);
        }
    }

    // if the plugin has more buses than specified in the xml, then try to remove them!
    while (maxNumBuses < targetBuses.size())
    {
        if (! plugin.removeBus (isInput))
            return;

        targetBuses.removeLast();
    }
}

//==============================================================================
static XmlElement* createBusLayoutXml (const AudioProcessor::BusesLayout& layout, const bool isInput)
{
    auto& buses = isInput ? layout.inputBuses
                          : layout.outputBuses;

    auto* xml = new XmlElement (isInput ? "INPUTS" : "OUTPUTS");

    for (int busIdx = 0; busIdx < buses.size(); ++busIdx)
    {
        auto& set = buses.getReference (busIdx);

        auto* bus = xml->createNewChildElement ("BUS");
        bus->setAttribute ("index", busIdx);
        bus->setAttribute ("layout", set.isDisabled() ? "disabled" : set.getSpeakerArrangementAsString());
    }

    return xml;
}

static XmlElement* createNodeXml (AudioProcessorGraph::Node* const node) noexcept
{
    if (auto* plugin = dynamic_cast<AudioPluginInstance*> (node->getProcessor()))
    {
        auto e = new XmlElement ("FILTER");

        e->setAttribute ("uid",      (int) node->nodeID.uid);
        e->setAttribute ("x",        node->properties ["x"].toString());
        e->setAttribute ("y",        node->properties ["y"].toString());
        e->setAttribute ("useARA",   node->properties ["useARA"].toString());
        e->setAttribute ("bypassed", node->isBypassed());

        // The part of the plugin this node was added as, e.g. one amp of an amp suite
        if (node->properties.contains ("module"))
            e->setAttribute ("module", node->properties ["module"].toString());

        // Which of the chain view's chains it's in
        if (node->properties.contains ("chain"))
            e->setAttribute ("chain", (int) node->properties ["chain"]);

        // What its controls are showing, e.g. "view.engine1" for which engine's controls (see ControlStrip)
        for (const auto& property : node->properties)
            if (property.name.toString().startsWith ("view."))
                e->setAttribute (property.name, property.value.toString());

        for (int i = 0; i < (int) PluginWindow::Type::numTypes; ++i)
        {
            auto type = (PluginWindow::Type) i;

            if (node->properties.contains (PluginWindow::getOpenProp (type)))
            {
                e->setAttribute (PluginWindow::getLastXProp (type), node->properties[PluginWindow::getLastXProp (type)].toString());
                e->setAttribute (PluginWindow::getLastYProp (type), node->properties[PluginWindow::getLastYProp (type)].toString());
                e->setAttribute (PluginWindow::getOpenProp (type),  node->properties[PluginWindow::getOpenProp (type)].toString());
            }
        }

        {
            PluginDescription pd;
            plugin->fillInPluginDescription (pd);
            e->addChildElement (pd.createXml().release());
        }

        {
            MemoryBlock m;
            node->getProcessor()->getStateInformation (m);
            e->createNewChildElement ("STATE")->addTextElement (m.toBase64Encoding());
        }

        auto layout = plugin->getBusesLayout();

        auto layouts = e->createNewChildElement ("LAYOUT");
        layouts->addChildElement (createBusLayoutXml (layout, true));
        layouts->addChildElement (createBusLayoutXml (layout, false));

        return e;
    }

    jassertfalse;
    return nullptr;
}

void PluginGraph::createNodeFromXml (const XmlElement& xml)
{
    PluginDescriptionAndPreference pd;
    const auto nodeUsesARA = xml.getBoolAttribute ("useARA");

    for (auto* e : xml.getChildIterator())
    {
        if (pd.pluginDescription.loadFromXml (*e))
        {
            pd.useARA = nodeUsesARA ? PluginDescriptionAndPreference::UseARA::yes
                                    : PluginDescriptionAndPreference::UseARA::no;
            break;
        }
    }

    auto createInstanceWithFallback = [&]() -> std::unique_ptr<AudioPluginInstance>
    {
        auto createInstance = [this] (const PluginDescriptionAndPreference& description) -> std::unique_ptr<AudioPluginInstance>
        {
            String errorMessage;

            auto localDpiDisabler = makeDPIAwarenessDisablerForPlugin (description.pluginDescription);

            auto instance = formatManager.createPluginInstance (description.pluginDescription,
                                                                graph.getSampleRate(),
                                                                graph.getBlockSize(),
                                                                errorMessage);

           #if JUCE_PLUGINHOST_ARA && (JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX)
            if (instance
                && description.useARA == PluginDescriptionAndPreference::UseARA::yes
                && description.pluginDescription.hasARAExtension)
            {
                return std::make_unique<ARAPluginInstanceWrapper> (std::move (instance));
            }
           #endif

            return instance;
        };

        if (auto instance = createInstance (pd))
            return instance;

        const auto allFormats = formatManager.getFormats();
        const auto matchingFormat = std::find_if (allFormats.begin(), allFormats.end(),
                                                  [&] (const AudioPluginFormat* f) { return f->getName() == pd.pluginDescription.pluginFormatName; });

        if (matchingFormat == allFormats.end())
            return nullptr;

        const auto plugins = knownPlugins.getTypesForFormat (**matchingFormat);
        const auto matchingPlugin = std::find_if (plugins.begin(), plugins.end(),
                                                  [&] (const PluginDescription& desc) { return pd.pluginDescription.uniqueId == desc.uniqueId; });

        if (matchingPlugin == plugins.end())
            return nullptr;

        return createInstance (PluginDescriptionAndPreference { *matchingPlugin });
    };

    if (auto instance = createInstanceWithFallback())
    {
        if (auto* layoutEntity = xml.getChildByName ("LAYOUT"))
        {
            auto layout = instance->getBusesLayout();

            readBusLayoutFromXml (layout, *instance, *layoutEntity, true);
            readBusLayoutFromXml (layout, *instance, *layoutEntity, false);

            instance->setBusesLayout (layout);
        }

        if (auto node = graph.addNode (std::move (instance), NodeID ((uint32) xml.getIntAttribute ("uid"))))
        {
            if (auto* state = xml.getChildByName ("STATE"))
            {
                MemoryBlock m;
                m.fromBase64Encoding (state->getAllSubText());

                node->getProcessor()->setStateInformation (m.getData(), (int) m.getSize());
            }

            node->properties.set ("x", xml.getDoubleAttribute ("x"));
            node->properties.set ("y", xml.getDoubleAttribute ("y"));
            node->properties.set ("useARA", xml.getBoolAttribute ("useARA"));
            node->setBypassed (xml.getBoolAttribute ("bypassed"));

            if (xml.hasAttribute ("module"))
                node->properties.set ("module", xml.getStringAttribute ("module"));

            if (xml.hasAttribute ("chain"))
                node->properties.set ("chain", xml.getIntAttribute ("chain"));

            for (int i = 0; i < xml.getNumAttributes(); ++i)
                if (xml.getAttributeName (i).startsWith ("view."))
                    node->properties.set (xml.getAttributeName (i), xml.getAttributeValue (i));

            for (int i = 0; i < (int) PluginWindow::Type::numTypes; ++i)
            {
                auto type = (PluginWindow::Type) i;

                if (xml.hasAttribute (PluginWindow::getOpenProp (type)))
                {
                    node->properties.set (PluginWindow::getLastXProp (type), xml.getIntAttribute (PluginWindow::getLastXProp (type)));
                    node->properties.set (PluginWindow::getLastYProp (type), xml.getIntAttribute (PluginWindow::getLastYProp (type)));
                    node->properties.set (PluginWindow::getOpenProp  (type), xml.getIntAttribute (PluginWindow::getOpenProp (type)));

                    if (node->properties[PluginWindow::getOpenProp (type)])
                    {
                        jassert (node->getProcessor() != nullptr);

                        if (auto w = getOrCreateWindowFor (node, type))
                            w->toFront (true);
                    }
                }
            }
        }
    }
}

std::unique_ptr<XmlElement> PluginGraph::createXml() const
{
    auto xml = std::make_unique<XmlElement> ("FILTERGRAPH");

    for (auto* node : graph.getNodes())
        xml->addChildElement (createNodeXml (node));

    for (auto& connection : graph.getConnections())
    {
        auto e = xml->createNewChildElement ("CONNECTION");

        e->setAttribute ("srcFilter", (int) connection.source.nodeID.uid);
        e->setAttribute ("srcChannel", connection.source.channelIndex);
        e->setAttribute ("dstFilter", (int) connection.destination.nodeID.uid);
        e->setAttribute ("dstChannel", connection.destination.channelIndex);
    }

    return xml;
}

void PluginGraph::restoreFromXml (const XmlElement& xml)
{
    clear();

    for (auto* e : xml.getChildWithTagNameIterator ("FILTER"))
    {
        createNodeFromXml (*e);
        changed();
    }

    for (auto* e : xml.getChildWithTagNameIterator ("CONNECTION"))
    {
        graph.addConnection ({ { NodeID ((uint32) e->getIntAttribute ("srcFilter")), e->getIntAttribute ("srcChannel") },
                               { NodeID ((uint32) e->getIntAttribute ("dstFilter")), e->getIntAttribute ("dstChannel") } });
    }

    graph.removeIllegalConnections();
}

File PluginGraph::getDefaultGraphDocumentOnMobile()
{
    auto persistantStorageLocation = File::getSpecialLocation (File::userApplicationDataDirectory);
    return persistantStorageLocation.getChildFile ("state.filtergraph");
}

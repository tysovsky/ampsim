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

#include <juce_audio_plugin_client/juce_audio_plugin_client.h>

#include "InternalPlugins.h"
#include "PluginGraph.h"

#define PIP_DEMO_UTILITIES_INCLUDED 1

// An alternative version of createAssetInputStream from the demo utilities header
// that fetches resources from embedded binary data instead of files
static std::unique_ptr<InputStream> createAssetInputStream (const char* resourcePath)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        if (String (BinaryData::originalFilenames[i]) == String (resourcePath))
        {
            int dataSizeInBytes;
            auto* resource = BinaryData::getNamedResource (BinaryData::namedResourceList[i], dataSizeInBytes);
            return std::make_unique<MemoryInputStream> (resource, dataSizeInBytes, false);
        }
    }

    return {};
}

//#include "../../../../examples/Plugins/AUv3SynthPluginDemo.h"
//#include "../../../../examples/Plugins/ArpeggiatorPluginDemo.h"
//#include "../../../../examples/Plugins/AudioPluginDemo.h"
//#include "../../../../examples/Plugins/DSPModulePluginDemo.h"
//#include "../../../../examples/Plugins/GainPluginDemo.h"
//#include "../../../../examples/Plugins/MidiLoggerPluginDemo.h"
//#include "../../../../examples/Plugins/MultiOutSynthPluginDemo.h"
//#include "../../../../examples/Plugins/NoiseGatePluginDemo.h"
//#include "../../../../examples/Plugins/SamplerPluginDemo.h"
//#include "../../../../examples/Plugins/SurroundPluginDemo.h"

//==============================================================================
class InternalPlugin final : public AudioPluginInstance
{
public:
    explicit InternalPlugin (std::unique_ptr<AudioProcessor> innerIn)
        : inner (std::move (innerIn))
    {
        jassert (inner != nullptr);

        for (auto isInput : { true, false })
            matchChannels (isInput);

        setBusesLayout (inner->getBusesLayout());
    }

    //==============================================================================
    const String getName() const override                                         { return inner->getName(); }
    StringArray getAlternateDisplayNames() const override                         { return inner->getAlternateDisplayNames(); }
    double getTailLengthSeconds() const override                                  { return inner->getTailLengthSeconds(); }
    bool acceptsMidi() const override                                             { return inner->acceptsMidi(); }
    bool producesMidi() const override                                            { return inner->producesMidi(); }
    AudioProcessorEditor* createEditor() override                                 { return inner->createEditor(); }
    bool hasEditor() const override                                               { return inner->hasEditor(); }
    int getNumPrograms() override                                                 { return inner->getNumPrograms(); }
    int getCurrentProgram() override                                              { return inner->getCurrentProgram(); }
    void setCurrentProgram (int i) override                                       { inner->setCurrentProgram (i); }
    const String getProgramName (int i) override                                  { return inner->getProgramName (i); }
    void changeProgramName (int i, const String& n) override                      { inner->changeProgramName (i, n); }
    void getStateInformation (juce::MemoryBlock& b) override                      { inner->getStateInformation (b); }
    void setStateInformation (const void* d, int s) override                      { inner->setStateInformation (d, s); }
    void getCurrentProgramStateInformation (juce::MemoryBlock& b) override        { inner->getCurrentProgramStateInformation (b); }
    void setCurrentProgramStateInformation (const void* d, int s) override        { inner->setCurrentProgramStateInformation (d, s); }
    void prepareToPlay (double sr, int bs) override                               { inner->setRateAndBufferSizeDetails (sr, bs); inner->prepareToPlay (sr, bs); }
    void releaseResources() override                                              { inner->releaseResources(); }
    void memoryWarningReceived() override                                         { inner->memoryWarningReceived(); }
    void processBlock (AudioBuffer<float>& a, MidiBuffer& m) override             { inner->processBlock (a, m); }
    void processBlock (AudioBuffer<double>& a, MidiBuffer& m) override            { inner->processBlock (a, m); }
    void processBlockBypassed (AudioBuffer<float>& a, MidiBuffer& m) override     { inner->processBlockBypassed (a, m); }
    void processBlockBypassed (AudioBuffer<double>& a, MidiBuffer& m) override    { inner->processBlockBypassed (a, m); }
    bool supportsDoublePrecisionProcessing() const override                       { return inner->supportsDoublePrecisionProcessing(); }
    bool supportsMPE() const override                                             { return inner->supportsMPE(); }
    bool isMidiEffect() const override                                            { return inner->isMidiEffect(); }
    void reset() override                                                         { inner->reset(); }
    void setNonRealtime (bool b) noexcept override                                { inner->setNonRealtime (b); }
    void refreshParameterList() override                                          { inner->refreshParameterList(); }
    void numChannelsChanged() override                                            { inner->numChannelsChanged(); }
    void numBusesChanged() override                                               { inner->numBusesChanged(); }
    void processorLayoutsChanged() override                                       { inner->processorLayoutsChanged(); }
    void setPlayHead (AudioPlayHead* p) override                                  { inner->setPlayHead (p); }
    void updateTrackProperties (const TrackProperties& p) override                { inner->updateTrackProperties (p); }
    bool isBusesLayoutSupported (const BusesLayout& layout) const override        { return inner->checkBusesLayoutSupported (layout); }
    bool applyBusLayouts (const BusesLayout& layouts) override                    { return inner->setBusesLayout (layouts) && AudioPluginInstance::applyBusLayouts (layouts); }

    bool canAddBus (bool) const override                                          { return true; }
    bool canRemoveBus (bool) const override                                       { return true; }

    //==============================================================================
    void fillInPluginDescription (PluginDescription& description) const override
    {
        description = getPluginDescription (*inner);
    }

private:
    static PluginDescription getPluginDescription (const AudioProcessor& proc)
    {
        const auto ins                  = proc.getTotalNumInputChannels();
        const auto outs                 = proc.getTotalNumOutputChannels();
        const auto identifier           = proc.getName();
        const auto registerAsGenerator  = ins == 0;
        const auto acceptsMidi          = proc.acceptsMidi();

        PluginDescription descr;

        descr.name              = identifier;
        descr.descriptiveName   = identifier;
        descr.pluginFormatName  = InternalPluginFormat::getIdentifier();
        descr.category          = (registerAsGenerator ? (acceptsMidi ? "Synth" : "Generator") : "Effect");
        descr.manufacturerName  = "JUCE";
        descr.version           = ProjectInfo::versionString;
        descr.fileOrIdentifier  = identifier;
        descr.isInstrument      = (acceptsMidi && registerAsGenerator);
        descr.numInputChannels  = ins;
        descr.numOutputChannels = outs;

        descr.uniqueId = descr.deprecatedUid = identifier.hashCode();

        return descr;
    }

    void matchChannels (bool isInput)
    {
        const auto inBuses = inner->getBusCount (isInput);

        while (getBusCount (isInput) < inBuses)
            addBus (isInput);

        while (inBuses < getBusCount (isInput))
            removeBus (isInput);
    }

    std::unique_ptr<AudioProcessor> inner;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InternalPlugin)
};

//==============================================================================
class SineWaveSynth final : public AudioProcessor
{
public:
    SineWaveSynth()
        : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo()))
    {
        const int numVoices = 8;

        // Add some voices...
        for (int i = numVoices; --i >= 0;)
            synth.addVoice (new SineWaveVoice());

        // ..and give the synth a sound to play
        synth.addSound (new SineWaveSound());
    }

    static String getIdentifier()
    {
        return "Sine Wave Synth";
    }

    //==============================================================================
    void prepareToPlay (double newSampleRate, int) override
    {
        synth.setCurrentPlaybackSampleRate (newSampleRate);
    }

    void releaseResources() override {}

    //==============================================================================
    void processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override
    {
        const int numSamples = buffer.getNumSamples();

        buffer.clear();
        synth.renderNextBlock (buffer, midiMessages, 0, numSamples);
        buffer.applyGain (0.8f);
    }

    using AudioProcessor::processBlock;

    const String getName() const override                                           { return getIdentifier(); }
    double getTailLengthSeconds() const override                                    { return 0.0; }
    bool acceptsMidi() const override                                               { return true; }
    bool producesMidi() const override                                              { return true; }
    AudioProcessorEditor* createEditor() override                                   { return nullptr; }
    bool hasEditor() const override                                                 { return false; }
    int getNumPrograms() override                                                   { return 1; }
    int getCurrentProgram() override                                                { return 0; }
    void setCurrentProgram (int) override                                           {}
    const String getProgramName (int) override                                      { return {}; }
    void changeProgramName (int, const String&) override                            {}
    void getStateInformation (juce::MemoryBlock&) override                          {}
    void setStateInformation (const void*, int) override                            {}

private:
    //==============================================================================
    struct SineWaveSound final : public SynthesiserSound
    {
        SineWaveSound() = default;

        bool appliesToNote (int /*midiNoteNumber*/) override    { return true; }
        bool appliesToChannel (int /*midiChannel*/) override    { return true; }
    };

    struct SineWaveVoice final : public SynthesiserVoice
    {
        SineWaveVoice() = default;

        bool canPlaySound (SynthesiserSound* sound) override
        {
            return dynamic_cast<SineWaveSound*> (sound) != nullptr;
        }

        void startNote (int midiNoteNumber, float velocity,
                        SynthesiserSound* /*sound*/,
                        int /*currentPitchWheelPosition*/) override
        {
            currentAngle = 0.0;
            level = velocity * 0.15;
            tailOff = 0.0;

            double cyclesPerSecond = MidiMessage::getMidiNoteInHertz (midiNoteNumber);
            double cyclesPerSample = cyclesPerSecond / getSampleRate();

            angleDelta = cyclesPerSample * 2.0 * MathConstants<double>::pi;
        }

        void stopNote (float /*velocity*/, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                // start a tail-off by setting this flag. The render callback will pick up on
                // this and do a fade out, calling clearCurrentNote() when it's finished.

                if (approximatelyEqual (tailOff, 0.0)) // we only need to begin a tail-off if it's not already doing so - the
                                                       // stopNote method could be called more than once.
                    tailOff = 1.0;
            }
            else
            {
                // we're being told to stop playing immediately, so reset everything..

                clearCurrentNote();
                angleDelta = 0.0;
            }
        }

        void pitchWheelMoved (int /*newValue*/) override
        {
            // not implemented for the purposes of this demo!
        }

        void controllerMoved (int /*controllerNumber*/, int /*newValue*/) override
        {
            // not implemented for the purposes of this demo!
        }

        void renderNextBlock (AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
        {
            if (! approximatelyEqual (angleDelta, 0.0))
            {
                if (tailOff > 0)
                {
                    while (--numSamples >= 0)
                    {
                        const float currentSample = (float) (sin (currentAngle) * level * tailOff);

                        for (int i = outputBuffer.getNumChannels(); --i >= 0;)
                            outputBuffer.addSample (i, startSample, currentSample);

                        currentAngle += angleDelta;
                        ++startSample;

                        tailOff *= 0.99;

                        if (tailOff <= 0.005)
                        {
                            // tells the synth that this voice has stopped
                            clearCurrentNote();

                            angleDelta = 0.0;
                            break;
                        }
                    }
                }
                else
                {
                    while (--numSamples >= 0)
                    {
                        const float currentSample = (float) (sin (currentAngle) * level);

                        for (int i = outputBuffer.getNumChannels(); --i >= 0;)
                            outputBuffer.addSample (i, startSample, currentSample);

                        currentAngle += angleDelta;
                        ++startSample;
                    }
                }
            }
        }

        using SynthesiserVoice::renderNextBlock;

    private:
        double currentAngle = 0, angleDelta = 0, level = 0, tailOff = 0;
    };

    //==============================================================================
    Synthesiser synth;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SineWaveSynth)
};

//==============================================================================
class ReverbPlugin final : public AudioProcessor
{
public:
    ReverbPlugin()
        : AudioProcessor (BusesProperties().withInput  ("Input",  AudioChannelSet::stereo())
                                           .withOutput ("Output", AudioChannelSet::stereo()))
    {}

    static String getIdentifier()
    {
        return "Reverb";
    }

    void prepareToPlay (double newSampleRate, int) override
    {
        reverb.setSampleRate (newSampleRate);
    }

    void reset() override
    {
        reverb.reset();
    }

    void releaseResources() override {}

    void processBlock (AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        auto numChannels = buffer.getNumChannels();

        if (numChannels == 1)
            reverb.processMono (buffer.getWritePointer (0), buffer.getNumSamples());
        else
            reverb.processStereo (buffer.getWritePointer (0),
                                  buffer.getWritePointer (1),
                                  buffer.getNumSamples());

        for (int ch = 2; ch < numChannels; ++ch)
            buffer.clear (ch, 0, buffer.getNumSamples());
    }

    using AudioProcessor::processBlock;

    const String getName() const override                                           { return getIdentifier(); }
    double getTailLengthSeconds() const override                                    { return 0.0; }
    bool acceptsMidi() const override                                               { return false; }
    bool producesMidi() const override                                              { return false; }
    AudioProcessorEditor* createEditor() override                                   { return nullptr; }
    bool hasEditor() const override                                                 { return false; }
    int getNumPrograms() override                                                   { return 1; }
    int getCurrentProgram() override                                                { return 0; }
    void setCurrentProgram (int) override                                           {}
    const String getProgramName (int) override                                      { return {}; }
    void changeProgramName (int, const String&) override                            {}
    void getStateInformation (juce::MemoryBlock&) override                          {}
    void setStateInformation (const void*, int) override                            {}

private:
    Reverb reverb;
};

//==============================================================================

InternalPluginFormat::InternalPluginFactory::InternalPluginFactory (const std::initializer_list<Constructor>& constructorsIn)
    : constructors (constructorsIn),
      descriptions ([&]
      {
          std::vector<PluginDescription> result;

          for (const auto& constructor : constructors)
              result.push_back (constructor()->getPluginDescription());

          return result;
      }())
{}

std::unique_ptr<AudioPluginInstance> InternalPluginFormat::InternalPluginFactory::createInstance (const String& name) const
{
    const auto begin = descriptions.begin();
    const auto it = std::find_if (begin,
                                  descriptions.end(),
                                  [&] (const PluginDescription& desc) { return name.equalsIgnoreCase (desc.name); });

    if (it == descriptions.end())
        return nullptr;

    const auto index = (size_t) std::distance (begin, it);
    return constructors[index]();
}

InternalPluginFormat::InternalPluginFormat()
    : factory {
        [] { return std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor> (AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode); },
        [] { return std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor> (AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode); },
        [] { return std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor> (AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode); },
        [] { return std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor> (AudioProcessorGraph::AudioGraphIOProcessor::midiOutputNode); },
        [] { return std::make_unique<LaneLevelProcessor>(); },
        [] { return std::make_unique<ChainInputProcessor>(); },
        [] { return std::make_unique<ChainOutputProcessor>(); },

        //[] { return std::make_unique<InternalPlugin> (std::make_unique<SineWaveSynth>()); },
        //[] { return std::make_unique<InternalPlugin> (std::make_unique<ReverbPlugin>()); },

        //[] { return std::make_unique<InternalPlugin> (std::make_unique<AUv3SynthProcessor>()); },
        //[] { return std::make_unique<InternalPlugin> (std::make_unique<Arpeggiator>()); },
        //[] { return std::make_unique<InternalPlugin> (std::make_unique<DspModulePluginDemoAudioProcessor>()); },
        //[] { return std::make_unique<InternalPlugin> (std::make_unique<GainProcessor>()); },
        //[] { return std::make_unique<InternalPlugin> (std::make_unique<JuceDemoPluginAudioProcessor>()); },
        //[] { return std::make_unique<InternalPlugin> (std::make_unique<MidiLoggerPluginDemoProcessor>()); },
        //[] { return std::make_unique<InternalPlugin> (std::make_unique<MultiOutSynth>()); },
        //[] { return std::make_unique<InternalPlugin> (std::make_unique<NoiseGate>()); },
        //[] { return std::make_unique<InternalPlugin> (std::make_unique<SamplerAudioProcessor>()); },
        //[] { return std::make_unique<InternalPlugin> (std::make_unique<SurroundProcessor>()); }
    }
{
}

std::unique_ptr<AudioPluginInstance> InternalPluginFormat::createInstance (const String& name)
{
    return factory.createInstance (name);
}

void InternalPluginFormat::createPluginInstance (const PluginDescription& desc,
                                                 double /*initialSampleRate*/, int /*initialBufferSize*/,
                                                 PluginCreationCallback callback)
{
    if (auto p = createInstance (desc.name))
        callback (std::move (p), {});
    else
        callback (nullptr, NEEDS_TRANS ("Invalid internal plugin name"));
}

bool InternalPluginFormat::requiresUnblockedMessageThreadDuringCreation (const PluginDescription&) const
{
    return false;
}

const std::vector<PluginDescription>& InternalPluginFormat::getAllTypes() const
{
    return factory.getDescriptions();
}

//==============================================================================
LaneLevelProcessor::LaneLevelProcessor()
    : AudioPluginInstance (BusesProperties().withInput  ("Input",  AudioChannelSet::stereo())
                                            .withOutput ("Output", AudioChannelSet::stereo()))
{
    // AudioPluginInstance hides addParameter because hosted plugins must use HostedParameters,
    // but this processor is our own, so a plain parameter is fine
    AudioProcessor::addParameter (level = new AudioParameterFloat ({ "level", 1 }, "Level", 0.0f, 1.0f, 1.0f));
}

void LaneLevelProcessor::fillInPluginDescription (PluginDescription& description) const
{
    description.name              = getIdentifier();
    description.descriptiveName   = getIdentifier();
    description.pluginFormatName  = InternalPluginFormat::getIdentifier();
    description.category          = "Utility";
    description.manufacturerName  = "JUCE";
    description.version           = ProjectInfo::versionString;
    description.fileOrIdentifier  = getIdentifier();
    description.numInputChannels  = 2;
    description.numOutputChannels = 2;
    description.uniqueId = description.deprecatedUid = getIdentifier().hashCode();
}

bool LaneLevelProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    const auto& main = layout.getMainOutputChannelSet();
    return (main == AudioChannelSet::mono() || main == AudioChannelSet::stereo())
            && main == layout.getMainInputChannelSet();
}

void LaneLevelProcessor::prepareToPlay (double sampleRate, int)
{
    // A short ramp so moving the level doesn't click
    smoothedLevel.reset (sampleRate, 0.05);
    smoothedLevel.setCurrentAndTargetValue (level->get());
}

void LaneLevelProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    smoothedLevel.setTargetValue (level->get());

    const auto startLevel = smoothedLevel.getCurrentValue();
    smoothedLevel.skip (buffer.getNumSamples());

    buffer.applyGainRamp (0, buffer.getNumSamples(), startLevel, smoothedLevel.getCurrentValue());
}

void LaneLevelProcessor::getStateInformation (MemoryBlock& block)
{
    MemoryOutputStream (block, false).writeFloat (level->get());
}

void LaneLevelProcessor::setStateInformation (const void* data, int size)
{
    if (size >= (int) sizeof (float))
        *level = MemoryInputStream (data, (size_t) size, false).readFloat();
}

//==============================================================================
ChainEndProcessor::ChainEndProcessor (const BusesProperties& buses, const String& prefix, int defaultChoice)
    : AudioPluginInstance (buses), blockNamePrefix (prefix)
{
    StringArray choices;

    for (int i = 1; i <= maxChannels; ++i)
        choices.add (String (i));

    for (int i = 1; i < maxChannels; i += 2)
        choices.add (String (i) + "+" + String (i + 1));

    AudioProcessor::addParameter (channel = new AudioParameterChoice ({ "channel", 1 }, "Channel", choices, defaultChoice));
    channel->addListener (this);
}

std::pair<int, int> ChainEndProcessor::getChosenChannels() const noexcept
{
    const int choice = channel->getIndex();

    if (choice < maxChannels)
        return { choice, -1 };

    const int pair = choice - maxChannels;
    return { pair * 2, pair * 2 + 1 };
}

String ChainEndProcessor::getBlockName() const
{
    return customName.isNotEmpty() ? customName : getChannelName();
}

String ChainEndProcessor::getChannelName() const
{
    return blockNamePrefix + " " + channel->getCurrentChoiceName();
}

void ChainEndProcessor::setCustomName (const String& newName)
{
    if (newName != customName)
    {
        customName = newName;
        updateHostDisplay();
    }
}

void ChainEndProcessor::parameterValueChanged (int, float)
{
    // Lets the chain view know the block's name has changed
    updateHostDisplay();
}

void ChainEndProcessor::fillInPluginDescription (PluginDescription& description) const
{
    description.name              = getName();
    description.descriptiveName   = getName();
    description.pluginFormatName  = InternalPluginFormat::getIdentifier();
    description.category          = "I/O devices";
    description.manufacturerName  = "JUCE";
    description.version           = ProjectInfo::versionString;
    description.fileOrIdentifier  = getName();
    description.numInputChannels  = getTotalNumInputChannels();
    description.numOutputChannels = getTotalNumOutputChannels();
    description.uniqueId = description.deprecatedUid = getName().hashCode();
}

void ChainEndProcessor::getStateInformation (MemoryBlock& block)
{
    MemoryOutputStream stream (block, false);
    stream.writeInt (channel->getIndex());
    stream.writeString (customName);
}

void ChainEndProcessor::setStateInformation (const void* data, int size)
{
    MemoryInputStream stream (data, (size_t) size, false);

    if (size >= (int) sizeof (int))
        *channel = jlimit (0, channel->choices.size() - 1, stream.readInt());

    // Inputs and outputs saved before they could be named stop at the channel
    setCustomName (stream.isExhausted() ? String() : stream.readString());
}

//==============================================================================
ChainInputProcessor::ChainInputProcessor()
    : ChainEndProcessor (BusesProperties().withInput  ("Device", AudioChannelSet::discreteChannels (maxChannels))
                                          .withOutput ("Chain",  AudioChannelSet::stereo()),
                         "Input", 0)
{
}

bool ChainInputProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    return layout.getMainInputChannelSet() == AudioChannelSet::discreteChannels (maxChannels)
            && layout.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void ChainInputProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    // The device's channels arrive on all the buffer's channels, and the chain's two leave on the first two
    const auto [first, second] = getChosenChannels();
    const int numSamples = buffer.getNumSamples();

    if (second < 0)
    {
        if (first != 0)
            buffer.copyFrom (0, 0, buffer, first, 0, numSamples);

        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    }
    else if (first != 0)
    {
        buffer.copyFrom (0, 0, buffer, first, 0, numSamples);
        buffer.copyFrom (1, 0, buffer, second, 0, numSamples);
    }
}

//==============================================================================
ChainOutputProcessor::ChainOutputProcessor()
    : ChainEndProcessor (BusesProperties().withInput  ("Chain",  AudioChannelSet::stereo())
                                          .withOutput ("Device", AudioChannelSet::discreteChannels (maxChannels)),
                         "Output", getChoiceFor (0, true))
{
}

bool ChainOutputProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    return layout.getMainInputChannelSet() == AudioChannelSet::stereo()
            && layout.getMainOutputChannelSet() == AudioChannelSet::discreteChannels (maxChannels);
}

void ChainOutputProcessor::prepareToPlay (double, int maximumExpectedSamplesPerBlock)
{
    chainSignal.setSize (2, maximumExpectedSamplesPerBlock);
}

void ChainOutputProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    // The chain's two channels arrive on the first two of the buffer's, which then all go to the device,
    // so they're put aside before the buffer is cleared for the chosen channels to be filled in
    const auto [first, second] = getChosenChannels();
    const int numSamples = buffer.getNumSamples();

    chainSignal.setSize (2, numSamples, false, false, true);
    chainSignal.copyFrom (0, 0, buffer, 0, 0, numSamples);
    chainSignal.copyFrom (1, 0, buffer, 1, 0, numSamples);
    buffer.clear();

    if (second < 0)
    {
        buffer.addFrom (first, 0, chainSignal, 0, 0, numSamples, 0.5f);
        buffer.addFrom (first, 0, chainSignal, 1, 0, numSamples, 0.5f);
    }
    else
    {
        buffer.copyFrom (first, 0, chainSignal, 0, 0, numSamples);
        buffer.copyFrom (second, 0, chainSignal, 1, 0, numSamples);
    }
}

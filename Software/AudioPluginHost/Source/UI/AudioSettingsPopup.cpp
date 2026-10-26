#include "AudioSettingsPopup.h"
#include "MainHostWindow.h"

namespace
{
    constexpr int margin = 12;
    constexpr int padding = 16;
    constexpr int headerHeight = 52;
    constexpr int rowHeight = 44;
    constexpr int rowGap = 8;
    constexpr int columnGap = 32;
    constexpr int labelWidth = 160;
    constexpr int freeLinksWidth = 300;

    // The user setting that turns free links on
    const char* const freeLinksSetting = "freeLinks";

    // "Output 1" + "Output 2" -> "Output 1 + 2"
    String getChannelPairName(const String& first, const String& second)
    {
        int common = 0;

        while (common < first.length() && common < second.length() && first[common] == second[common])
            ++common;

        // Only split at a space, so "Input 11" + "Input 12" doesn't become "Input 11 + 2"
        while (common > 0 && !CharacterFunctions::isWhitespace(first[common - 1]))
            --common;

        return first.trim() + " + " + second.substring(common).trim();
    }

    // Lists a device's channels as stereo pairs, with IDs starting at firstPairId, and selects the active pair
    void fillChannelBox(ComboBox& box, const StringArray& channelNames, const BigInteger& activeChannels, int firstPairId)
    {
        for (int i = 0; i < channelNames.size(); i += 2)
            box.addItem(i + 1 < channelNames.size() ? getChannelPairName(channelNames[i], channelNames[i + 1])
                                                    : channelNames[i].trim(),
                        firstPairId + i / 2);

        const int numActive = activeChannels.countNumberOfSetBits();
        const int pair = activeChannels.findNextSetBit(0) / 2;

        BigInteger pairChannels;
        pairChannels.setRange(pair * 2, jmin(2, channelNames.size() - pair * 2), true);

        if (numActive > 0 && activeChannels == pairChannels)
            box.setSelectedId(firstPairId + pair, dontSendNotification);
        else
            box.setTextWhenNothingSelected(numActive > 0 && numActive >= channelNames.size() ? "All channels" : "Custom");
    }
}

//==============================================================================
struct AudioSettingsPopup::InputLevelMeter final : public Component,
    private Timer
{
    explicit InputLevelMeter(AudioDeviceManager& m) : manager(m) {}

    void start()
    {
        levelGetter = manager.getInputLevelGetter();
        startTimerHz(20);
    }

    void stop()
    {
        stopTimer();
        levelGetter = nullptr;
        level = 0.0f;
    }

    void paint(Graphics& g) override
    {
        // The same skew JUCE's own device selector uses, so quiet signals still show
        getLookAndFeel().drawLevelMeter(g, getWidth(), getHeight(), (float)std::exp(std::log(level) / 3.0));
    }

private:
    void timerCallback() override
    {
        auto newLevel = levelGetter != nullptr ? (float)levelGetter->getCurrentLevel() : 0.0f;

        if (std::abs(level - newLevel) > 0.005f)
        {
            level = newLevel;
            repaint();
        }
    }

    AudioDeviceManager& manager;
    AudioDeviceManager::LevelMeter::Ptr levelGetter;
    float level = 0.0f;
};

//==============================================================================
// A tick box with its text beside it, big enough for a finger
struct AudioSettingsPopup::CheckBox final : public Button
{
    explicit CheckBox(const String& text) : Button(text)
    {
        setClickingTogglesState(true);
        setWantsKeyboardFocus(false);
    }

    void paintButton(Graphics& g, bool, bool isButtonDown) override
    {
        auto area = getLocalBounds().toFloat();
        const auto box = area.removeFromLeft(area.getHeight()).withSizeKeepingCentre(26.0f, 26.0f);
        const auto accent = findColour(AppColourIds::signalPath);
        const bool isTicked = getToggleState();

        g.setColour(isTicked ? accent : findColour(ComboBox::backgroundColourId));
        g.fillRoundedRectangle(box, 5.0f);
        g.setColour(isTicked || isButtonDown ? accent : findColour(ComboBox::outlineColourId));
        g.drawRoundedRectangle(box, 5.0f, 1.5f);

        if (isTicked)
        {
            Path tick;
            tick.startNewSubPath(box.getRelativePoint(0.24f, 0.52f));
            tick.lineTo(box.getRelativePoint(0.43f, 0.72f));
            tick.lineTo(box.getRelativePoint(0.78f, 0.30f));

            g.setColour(findColour(ResizableWindow::backgroundColourId));
            g.strokePath(tick, PathStrokeType(3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        g.setColour(findColour(Label::textColourId));
        g.setFont(Font(TouchLookAndFeel::fontSize));
        g.drawText(getButtonText(), area.withTrimmedLeft(6.0f), Justification::centredLeft, true);
    }
};

//==============================================================================
AudioSettingsPopup::AudioSettingsPopup(AudioDeviceManager& dm)
    : deviceManager(dm),
    inputLevelMeter(std::make_unique<InputLevelMeter>(dm))
{
    setLookAndFeel(&lookAndFeel);

    titleLabel.setText("Settings", dontSendNotification);
    titleLabel.setFont(Font(24.0f, Font::bold));
    addAndMakeVisible(titleLabel);

    doneButton.onClick = [this] { hide(); };
    addAndMakeVisible(doneButton);

    freeLinksBox = std::make_unique<CheckBox>("Free links (experimental)");
    freeLinksBox->onClick = [this]
    {
        const bool isOn = freeLinksBox->getToggleState();
        getAppProperties().getUserSettings()->setValue(freeLinksSetting, isOn);

        if (onFreeLinksChange != nullptr)
            onFreeLinksChange(isOn);
    };
    addAndMakeVisible(freeLinksBox.get());

    const std::pair<Label*, const char*> labels[] = {
        { &deviceTypeLabel, "Device type" },
        { &outputLabel, "Output" },
        { &inputLabel, "Input" },
        { &sampleRateLabel, "Sample rate" },
        { &bufferSizeLabel, "Buffer size" },
        { &outputChannelsLabel, "Output channels" },
        { &inputChannelsLabel, "Input channels" },
        { &midiInputsLabel, "MIDI inputs" },
        { &midiOutputLabel, "MIDI output" },
        { &noMidiInputsLabel, "No MIDI inputs found" },
        { &themeLabel, "Theme" }
    };

    for (auto& [label, text] : labels)
    {
        label->setText(text, dontSendNotification);
        label->setFont(Font(TouchLookAndFeel::fontSize));
        label->setJustificationType(Justification::centredLeft);
        addAndMakeVisible(label);
    }

    for (auto* box : { &deviceTypeBox, &outputDeviceBox, &inputDeviceBox, &sampleRateBox, &bufferSizeBox,
                       &outputChannelsBox, &inputChannelsBox, &midiOutputBox, &themeBox })
    {
        box->setTextWhenNoChoicesAvailable("None available");
        addAndMakeVisible(box);
    }

    const auto& themes = getThemeManager().getThemes();

    for (int i = 0; i < (int)themes.size(); ++i)
        themeBox.addItem(themes[(size_t)i].name, i + 1);

    themeBox.onChange = [this] { getThemeManager().setTheme(themeBox.getSelectedId() - 1); };

    deviceTypeBox.onChange = [this] { deviceTypeChosen(); };
    outputDeviceBox.onChange = [this] { deviceChosen(false); };
    inputDeviceBox.onChange = [this] { deviceChosen(true); };
    outputChannelsBox.onChange = [this] { channelsChosen(false); };
    inputChannelsBox.onChange = [this] { channelsChosen(true); };

    sampleRateBox.onChange = [this]
    {
        auto setup = deviceManager.getAudioDeviceSetup();
        setup.sampleRate = sampleRateBox.getSelectedId();
        applySetup(setup);
    };

    bufferSizeBox.onChange = [this]
    {
        auto setup = deviceManager.getAudioDeviceSetup();
        setup.bufferSize = bufferSizeBox.getSelectedId();
        applySetup(setup);
    };

    midiOutputBox.onChange = [this]
    {
        const int index = midiOutputBox.getSelectedId() - 2;
        deviceManager.setDefaultMidiOutputDevice(isPositiveAndBelow(index, midiOutputs.size()) ? midiOutputs[index].identifier
                                                                                              : String());
    };

    testButton.onClick = [this] { deviceManager.playTestSound(); };
    addAndMakeVisible(testButton);

    controlPanelButton.onClick = [this]
    {
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            if (device->showControlPanel())
            {
                // The driver's settings may have changed, so reopen the device to pick them up
                deviceManager.closeAudioDevice();
                deviceManager.restartLastAudioDevice();
            }
        }

        updateControls();
    };
    addChildComponent(controlPanelButton);

    addAndMakeVisible(inputLevelMeter.get());

    deviceManager.addChangeListener(this);
}

AudioSettingsPopup::~AudioSettingsPopup()
{
    deviceManager.removeChangeListener(this);
    inputLevelMeter->stop();
    setLookAndFeel(nullptr);
}

//==============================================================================
bool AudioSettingsPopup::areFreeLinksOn()
{
    return getAppProperties().getUserSettings()->getBoolValue(freeLinksSetting, false);
}

void AudioSettingsPopup::show()
{
    freeLinksBox->setToggleState(areFreeLinksOn(), dontSendNotification);
    updateControls();
    setVisible(true);
    toFront(true);
    inputLevelMeter->start();
}

void AudioSettingsPopup::hide()
{
    inputLevelMeter->stop();
    setVisible(false);
}

void AudioSettingsPopup::changeListenerCallback(ChangeBroadcaster*)
{
    if (isVisible())
        updateControls();
}

//==============================================================================
void AudioSettingsPopup::paint(Graphics& g)
{
    g.fillAll(Colours::black.withAlpha(0.6f));

    auto panel = getPanelBounds().toFloat();
    auto background = findColour(ResizableWindow::backgroundColourId);

    g.setColour(background);
    g.fillRoundedRectangle(panel, 10.0f);

    g.setColour(findColour(ComboBox::outlineColourId));
    g.drawRoundedRectangle(panel, 10.0f, 1.5f);
    g.fillRect(panel.getX() + padding, panel.getY() + headerHeight, panel.getWidth() - 2 * padding, 1.0f);
}

void AudioSettingsPopup::resized()
{
    auto panel = getPanelBounds();

    auto header = panel.removeFromTop(headerHeight).reduced(padding, 6);
    doneButton.setBounds(header.removeFromRight(140));
    header.removeFromRight(columnGap);
    freeLinksBox->setBounds(header.removeFromRight(freeLinksWidth));
    titleLabel.setBounds(header);

    auto content = panel.withTrimmedTop(rowGap).reduced(padding, 0);
    auto left = content.removeFromLeft((content.getWidth() - columnGap) / 2);
    auto right = content.withTrimmedLeft(columnGap);

    // Takes the next row (or rows) from a column, puts the label on the left and returns the rest
    auto nextRow = [](Rectangle<int>& column, Label& label, int numRows = 1)
    {
        auto row = column.removeFromTop(rowHeight * numRows + rowGap * (numRows - 1));
        column.removeFromTop(rowGap);
        label.setBounds(row.removeFromLeft(labelWidth).withHeight(rowHeight));
        return row;
    };

    deviceTypeBox.setBounds(nextRow(left, deviceTypeLabel));

    auto outputRow = nextRow(left, outputLabel);
    testButton.setBounds(outputRow.removeFromRight(100));
    outputDeviceBox.setBounds(outputRow.withTrimmedRight(rowGap));

    auto inputRow = nextRow(left, inputLabel);
    inputLevelMeter->setBounds(inputRow.removeFromRight(100).reduced(0, 12));
    inputDeviceBox.setBounds(inputRow.withTrimmedRight(rowGap));

    sampleRateBox.setBounds(nextRow(left, sampleRateLabel));
    bufferSizeBox.setBounds(nextRow(left, bufferSizeLabel));
    controlPanelButton.setBounds(left.removeFromTop(rowHeight).withTrimmedLeft(labelWidth).withWidth(220));

    outputChannelsBox.setBounds(nextRow(right, outputChannelsLabel));
    inputChannelsBox.setBounds(nextRow(right, inputChannelsLabel));

    auto midiInputArea = nextRow(right, midiInputsLabel, 2);
    noMidiInputsLabel.setBounds(midiInputArea.withHeight(rowHeight));
    layoutMidiInputButtons(midiInputArea);

    midiOutputBox.setBounds(nextRow(right, midiOutputLabel));
    themeBox.setBounds(nextRow(right, themeLabel));
}

void AudioSettingsPopup::layoutMidiInputButtons(Rectangle<int> area)
{
    // Two rows of toggle buttons, with as many columns as needed
    const int numColumns = jmax(2, (midiInputButtons.size() + 1) / 2);
    const int buttonWidth = (area.getWidth() - rowGap * (numColumns - 1)) / numColumns;

    for (int i = 0; i < midiInputButtons.size(); ++i)
        midiInputButtons[i]->setBounds(area.getX() + (i % numColumns) * (buttonWidth + rowGap),
                                       area.getY() + (i / numColumns) * (rowHeight + rowGap),
                                       buttonWidth, rowHeight);
}

void AudioSettingsPopup::mouseUp(const MouseEvent& e)
{
    // Tapping outside the panel closes the popup
    if (e.mouseWasClicked() && !getPanelBounds().contains(e.getPosition()))
        hide();
}

Rectangle<int> AudioSettingsPopup::getPanelBounds() const
{
    return getLocalBounds().reduced(margin);
}

//==============================================================================
void AudioSettingsPopup::updateControls()
{
    auto* type = deviceManager.getCurrentDeviceTypeObject();
    auto* device = deviceManager.getCurrentAudioDevice();
    const auto setup = deviceManager.getAudioDeviceSetup();
    const bool separateInputs = type != nullptr && type->hasSeparateInputsAndOutputs();

    deviceTypeBox.clear(dontSendNotification);
    auto& types = deviceManager.getAvailableDeviceTypes();

    for (int i = 0; i < types.size(); ++i)
    {
        deviceTypeBox.addItem(types[i]->getTypeName(), i + 1);

        if (types[i] == type)
            deviceTypeBox.setSelectedId(i + 1, dontSendNotification);
    }

    // Devices. Some driver types (e.g. ASIO) use one device for both directions.
    outputNames = type != nullptr ? type->getDeviceNames(false) : StringArray();
    inputNames = type != nullptr ? type->getDeviceNames(true) : StringArray();

    outputLabel.setText(separateInputs ? "Output" : "Device", dontSendNotification);
    outputDeviceBox.clear(dontSendNotification);
    outputDeviceBox.addItemList(outputNames, 1);
    outputDeviceBox.setSelectedId(outputNames.indexOf(setup.outputDeviceName) + 1, dontSendNotification);

    inputLabel.setText(separateInputs ? "Input" : "Input level", dontSendNotification);
    inputDeviceBox.setVisible(separateInputs);
    inputDeviceBox.clear(dontSendNotification);
    inputDeviceBox.addItem("None", 1);
    inputDeviceBox.addItemList(inputNames, 2);
    inputDeviceBox.setSelectedId(setup.inputDeviceName.isEmpty() ? 1 : inputNames.indexOf(setup.inputDeviceName) + 2,
                                 dontSendNotification);

    // Channels, sample rate and buffer size all depend on the open device
    outputChannelsBox.clear(dontSendNotification);
    inputChannelsBox.clear(dontSendNotification);
    sampleRateBox.clear(dontSendNotification);
    bufferSizeBox.clear(dontSendNotification);

    if (device != nullptr)
    {
        fillChannelBox(outputChannelsBox, device->getOutputChannelNames(), device->getActiveOutputChannels(), 1);

        inputChannelsBox.addItem("None", 1);
        fillChannelBox(inputChannelsBox, device->getInputChannelNames(), device->getActiveInputChannels(), 2);

        if (device->getActiveInputChannels().isZero())
            inputChannelsBox.setSelectedId(1, dontSendNotification);

        for (auto rate : device->getAvailableSampleRates())
            sampleRateBox.addItem(String(roundToInt(rate)) + " Hz", roundToInt(rate));

        sampleRateBox.setSelectedId(roundToInt(device->getCurrentSampleRate()), dontSendNotification);

        const auto currentRate = device->getCurrentSampleRate() > 0.0 ? device->getCurrentSampleRate() : 44100.0;

        for (auto size : device->getAvailableBufferSizes())
            bufferSizeBox.addItem(String(size) + " samples (" + String(size * 1000.0 / currentRate, 1) + " ms)", size);

        bufferSizeBox.setSelectedId(device->getCurrentBufferSizeSamples(), dontSendNotification);
    }

    for (auto* box : { &outputChannelsBox, &inputChannelsBox, &sampleRateBox, &bufferSizeBox })
        box->setEnabled(device != nullptr && box->getNumItems() > 0);

    testButton.setEnabled(device != nullptr);
    controlPanelButton.setVisible(device != nullptr && device->hasControlPanel());

    themeBox.setSelectedId(getThemeManager().getCurrentThemeIndex() + 1, dontSendNotification);

    updateMidiControls();
}

void AudioSettingsPopup::updateMidiControls()
{
    auto availableInputs = MidiInput::getAvailableDevices();

    if (availableInputs != midiInputs)
    {
        midiInputs = availableInputs;
        midiInputButtons.clear();

        for (auto& info : midiInputs)
        {
            auto* button = midiInputButtons.add(new TextButton(info.name));
            button->setClickingTogglesState(true);
            button->onClick = [this, button, identifier = info.identifier]
            {
                deviceManager.setMidiInputDeviceEnabled(identifier, button->getToggleState());
            };
            addAndMakeVisible(button);
        }

        noMidiInputsLabel.setVisible(midiInputs.isEmpty());
        resized();
    }

    for (int i = 0; i < midiInputs.size(); ++i)
        midiInputButtons[i]->setToggleState(deviceManager.isMidiInputDeviceEnabled(midiInputs[i].identifier),
                                            dontSendNotification);

    midiOutputs = MidiOutput::getAvailableDevices();
    midiOutputBox.clear(dontSendNotification);
    midiOutputBox.addItem("None", 1);

    for (int i = 0; i < midiOutputs.size(); ++i)
    {
        midiOutputBox.addItem(midiOutputs[i].name, i + 2);

        if (midiOutputs[i].identifier == deviceManager.getDefaultMidiOutputIdentifier())
            midiOutputBox.setSelectedId(i + 2, dontSendNotification);
    }

    if (midiOutputBox.getSelectedId() == 0)
        midiOutputBox.setSelectedId(1, dontSendNotification);
}

//==============================================================================
void AudioSettingsPopup::deviceTypeChosen()
{
    auto& types = deviceManager.getAvailableDeviceTypes();

    if (auto* type = types[deviceTypeBox.getSelectedId() - 1])
        deviceManager.setCurrentAudioDeviceType(type->getTypeName(), true);

    updateControls();
}

void AudioSettingsPopup::deviceChosen(bool isInput)
{
    auto* type = deviceManager.getCurrentDeviceTypeObject();

    if (type == nullptr)
        return;

    auto setup = deviceManager.getAudioDeviceSetup();

    if (isInput)
    {
        const int index = inputDeviceBox.getSelectedId() - 2;
        setup.inputDeviceName = isPositiveAndBelow(index, inputNames.size()) ? inputNames[index] : String();
        setup.useDefaultInputChannels = true;
    }
    else
    {
        setup.outputDeviceName = outputNames[outputDeviceBox.getSelectedId() - 1];
        setup.useDefaultOutputChannels = true;

        if (!type->hasSeparateInputsAndOutputs())
        {
            setup.inputDeviceName = setup.outputDeviceName;
            setup.useDefaultInputChannels = true;
        }
    }

    applySetup(setup);
}

void AudioSettingsPopup::channelsChosen(bool isInput)
{
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return;

    auto setup = deviceManager.getAudioDeviceSetup();
    auto& box = isInput ? inputChannelsBox : outputChannelsBox;
    auto& channels = isInput ? setup.inputChannels : setup.outputChannels;
    const int numChannels = (isInput ? device->getInputChannelNames() : device->getOutputChannelNames()).size();
    const int pair = box.getSelectedId() - (isInput ? 2 : 1);   // input has "None" before the pairs

    channels.clear();

    if (pair >= 0)
        channels.setRange(pair * 2, jmin(2, numChannels - pair * 2), true);

    (isInput ? setup.useDefaultInputChannels : setup.useDefaultOutputChannels) = false;
    applySetup(setup);
}

void AudioSettingsPopup::applySetup(const AudioDeviceManager::AudioDeviceSetup& setup)
{
    auto error = deviceManager.setAudioDeviceSetup(setup, true);

    if (error.isNotEmpty())
        AlertWindow::showMessageBoxAsync(MessageBoxIconType::WarningIcon, "Couldn't open the audio device", error);

    updateControls();
}

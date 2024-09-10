#include "PluginControlsPanel.h"
#include "PluginControls/ArchetypeGojira.h"

PluginControlsPanel::PluginControlsPanel(GraphEditorPanel* p, bool isTopPanel) : isTopPanel(isTopPanel){
    p->addListener(this);

    setMidiInput();
}

void PluginControlsPanel::resized() {
    if (topPanel != nullptr) {
        topPanel->setBounds(getLocalBounds());
    }
    else if (bottomPanel != nullptr) {
        bottomPanel->setBounds(getLocalBounds());
    }
}

void PluginControlsPanel::paint(Graphics& g)
{
    auto boxArea = getLocalBounds();
    auto boxColour = findColour(TextEditor::backgroundColourId);

    g.setColour(boxColour);
    g.fillRect(boxArea.toFloat());
}

void PluginControlsPanel::onActiveComponentChange(PluginComponent* activeComponent) {
    if (activePlugin) {
        removeAllChildren();
    }

	activePlugin = activeComponent;
    if (activeComponent) {
        if (activePlugin->getPluginControl()) {
            if (isTopPanel) {
                topPanel = activePlugin->getPluginControl()->getTopPanel();
                addAndMakeVisible(topPanel);
            }
            else {
                bottomPanel = activePlugin->getPluginControl()->getBottomPanel();
                addAndMakeVisible(bottomPanel);
            }            
        }
    }
    else {
        topPanel = nullptr;
        bottomPanel = nullptr;
    }

    resized();
}

void PluginControlsPanel::onNodeDeleted(PluginComponent* removedComponent) {
    if (activePlugin == removedComponent) {
        activePlugin = nullptr;
        topPanel = nullptr;
        bottomPanel = nullptr;

        resized();
    }
};

void PluginControlsPanel::handleIncomingMidiMessage(MidiInput* source, const MidiMessage& message)
{
    if (topPanel == nullptr && bottomPanel == nullptr) {
        return;
    }

    if (message.isNoteOn() || message.isNoteOff()) {
        auto noteNum = message.getNoteNumber();

        int toggleIndex = 0;

        switch (noteNum) {
        case 64:
            toggleIndex = 0;
            break;
        }

        if (isTopPanel)
            topPanel->toggleValueChanged(toggleIndex, message.isNoteOn());
        else
            bottomPanel->toggleValueChanged(toggleIndex, message.isNoteOn());

    }
    else if (message.isController()) {
        int knobIndex = 0;

        auto controllerNumber = message.getControllerNumber();
        switch (controllerNumber)
        {
        case 10:
            knobIndex = 0;
            break;
        case 11:
            knobIndex = 1;
            break;
        case 16:
            knobIndex = 2;
            break;
        case 15:
            knobIndex = 3;
            break;
        case 14:
            knobIndex = 4;
            break;
        case 13: 
            knobIndex = 5;
            break;
        case 12:
            knobIndex = 6;
            break;
        default:
            break;
        }

        auto controllerValue = message.getControllerValue();

        int knobValue = controllerValue / 127.0 * 100;

        if(isTopPanel)
            topPanel->knobValueChanged(knobIndex, knobValue);
        else
            bottomPanel->knobValueChanged(knobIndex, knobValue);
    }
}

void PluginControlsPanel::setMidiInput()
{
    auto midiDevices = juce::MidiInput::getAvailableDevices();

    for (auto device : midiDevices) {
        auto name = device.name.toStdString();
        if (name == "Seeed XIAO M0") {
            if (!deviceManager.isMidiInputDeviceEnabled(device.identifier))
                deviceManager.setMidiInputDeviceEnabled(device.identifier, true);

            deviceManager.addMidiInputDeviceCallback(device.identifier, this);
        }
    }
}
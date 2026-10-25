#include "BlockControlsPanel.h"
#include "ThemeManager.h"

namespace
{
    constexpr int padding = 12;
    constexpr int infoWidth = 190;
    constexpr int buttonHeight = 44;
    constexpr int editButtonWidth = 80;
}

//==============================================================================
BlockControlsPanel::BlockControlsPanel (AudioDeviceManager& dm)
    : deviceManager (dm), controller (ControllerConfig::load())
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (strip);
    strip.onViewChange = [this] { if (onViewChange != nullptr) onViewChange(); };

    closeButton.setButtonText (String (CharPointer_UTF8 ("\xc3\x97")));

    for (auto* button : { &closeButton, &editButton })
        button->setWantsKeyboardFocus (false);

    closeButton.onClick = [this]
    {
        if (onClose != nullptr)
            onClose();
    };

    editButton.onClick = [this]
    {
        if (onEdit != nullptr)
            onEdit();
    };

    addAndMakeVisible (closeButton);
    addChildComponent (editButton);

    // Turn on the controller's own MIDI input, so its knobs work without a trip to the settings
    if (controller.getDeviceName().isNotEmpty())
        for (auto& device : MidiInput::getAvailableDevices())
            if (device.name == controller.getDeviceName() && ! deviceManager.isMidiInputDeviceEnabled (device.identifier))
                deviceManager.setMidiInputDeviceEnabled (device.identifier, true);

    deviceManager.addMidiInputDeviceCallback ({}, this);
}

BlockControlsPanel::~BlockControlsPanel()
{
    deviceManager.removeMidiInputDeviceCallback ({}, this);
    clear();
    setLookAndFeel (nullptr);
}

//==============================================================================
void BlockControlsPanel::showBlock (AudioProcessorGraph::Node::Ptr newNode, const XmlElement& definition,
                                    const String& blockName, const String& blockSubtitle, BlockIcon blockIcon,
                                    bool hasEditor)
{
    clear();

    node = std::move (newNode);
    name = blockName;
    subtitle = blockSubtitle;
    icon = blockIcon;

    strip.setControls (node, definition);
    editButton.setVisible (hasEditor);
    resized();
    repaint();
}

void BlockControlsPanel::setBlockName (const String& newName, const String& newSubtitle)
{
    if (newName != name || newSubtitle != subtitle)
    {
        name = newName;
        subtitle = newSubtitle;
        repaint();
    }
}

void BlockControlsPanel::clear()
{
    strip.clear();
    editButton.setVisible (false);
    node = nullptr;
}

//==============================================================================
void BlockControlsPanel::paint (Graphics& g)
{
    // The title bar's colour, which frames the chain between the two, and which the knobs' tracks
    // show up against (they're the colour of the tiles and buttons)
    g.fillAll (findColour (AppColourIds::titleBarBackground));

    g.setColour (findColour (ComboBox::outlineColourId));
    g.fillRect (0, 0, getWidth(), 1);

    // What the block is, on the left
    auto info = getInfoArea().toFloat();
    drawBlockIcon (g, icon, info.removeFromLeft (44.0f).withSizeKeepingCentre (40.0f, 40.0f), findColour (AppColourIds::signalPath));
    info.removeFromLeft (10.0f);

    auto text = info.withSizeKeepingCentre (info.getWidth(), 48.0f).toNearestInt();
    g.setColour (findColour (Label::textColourId));
    g.setFont (Font (18.0f, Font::bold));

    if (subtitle.isEmpty())
    {
        g.drawFittedText (name, text, Justification::centredLeft, 2);
        return;
    }

    g.drawFittedText (name, text.removeFromTop (24), Justification::centredLeft, 1);
    g.setColour (findColour (Label::textColourId).withAlpha (0.6f));
    g.setFont (Font (13.0f));
    g.drawFittedText (subtitle, text, Justification::centredLeft, 1);
}

void BlockControlsPanel::resized()
{
    auto area = getLocalBounds().reduced (padding, 8);

    closeButton.setBounds (area.removeFromRight (buttonHeight).withSizeKeepingCentre (buttonHeight, buttonHeight));
    area.removeFromRight (padding);

    if (editButton.isVisible())
    {
        editButton.setBounds (area.removeFromRight (editButtonWidth).withSizeKeepingCentre (editButtonWidth, buttonHeight));
        area.removeFromRight (padding);
    }

    area.removeFromLeft (infoWidth + padding);
    strip.setBounds (area);
}

Rectangle<int> BlockControlsPanel::getInfoArea() const
{
    return getLocalBounds().reduced (padding, 8).removeFromLeft (infoWidth);
}

//==============================================================================
void BlockControlsPanel::handleIncomingMidiMessage (MidiInput* source, const MidiMessage& message)
{
    // This is called on the MIDI thread, so the work is done on the message thread
    if (controller.getDeviceName().isNotEmpty() && (source == nullptr || source->getName() != controller.getDeviceName()))
        return;

    MessageManager::callAsync ([safeThis = SafePointer<BlockControlsPanel> (this), message]
    {
        if (safeThis != nullptr)
            safeThis->handleControllerMessage (message);
    });
}

void BlockControlsPanel::handleControllerMessage (const MidiMessage& message)
{
    if (const auto move = controller.findKnob (message))
    {
        strip.handleKnob (move->number, move->position);
        return;
    }

    if (const auto change = controller.findSwitch (message))
    {
        // A toggling switch acts when it's pressed, not when it's let go
        if (change->toggles && ! change->isOn)
            return;

        // Otherwise it turns the block itself on and off
        if (! strip.handleSwitch (change->number, change->toggles, change->isOn) && node != nullptr && onSwitchBlock != nullptr)
            onSwitchBlock (change->toggles ? node->isBypassed() : change->isOn);
    }
}

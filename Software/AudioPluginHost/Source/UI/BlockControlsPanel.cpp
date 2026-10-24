#include "BlockControlsPanel.h"
#include "ThemeManager.h"
#include "../Plugins/PluginModules.h"

namespace
{
    constexpr int padding = 12;
    constexpr int infoWidth = 190;
    constexpr int controlGap = 8;
    constexpr int labelHeight = 20;
    constexpr int buttonHeight = 44;

    // The widest a switch or list gets when its control has more room than it needs
    constexpr int maxSwitchWidth = 140;
    constexpr int maxListWidth = 240;

    // The number of steps of a parameter that has them; continuous parameters report a huge number
    int getNumSteps (const AudioProcessorParameter& parameter)
    {
        const int numSteps = parameter.getNumSteps();
        return parameter.isDiscrete() && numSteps > 1 && numSteps <= 1024 ? numSteps : 0;
    }

    int getStep (const AudioProcessorParameter& parameter, int numSteps)
    {
        return numSteps > 1 ? roundToInt (parameter.getValue() * (float) (numSteps - 1)) : 0;
    }

    float getStepValue (int step, int numSteps)
    {
        return numSteps > 1 ? (float) jlimit (0, numSteps - 1, step) / (float) (numSteps - 1) : 0.0f;
    }
}

//==============================================================================
// A <Show>'s condition on a parameter's value
struct BlockControlsPanel::Condition
{
    AudioProcessorParameter* parameter;
    String value;   // the parameter's text for the value, e.g. "Roses"
    int step;       // or its step, counting from 0, if this isn't -1

    bool holds() const
    {
        if (step >= 0)
            return getStep (*parameter, getNumSteps (*parameter)) == step;

        return parameter->getCurrentValueAsText().equalsIgnoreCase (value);
    }
};

//==============================================================================
// One control on the panel, for one of the plugin's parameters
class BlockControlsPanel::Control : public Component
{
public:
    Control (AudioProcessorParameter& p, const XmlElement& e)
        : knob (e.getIntAttribute ("knob")),
          switchNumber (e.getIntAttribute ("switch")),
          parameter (p),
          label (e.getStringAttribute ("label", p.getName (32)))
    {
    }

    /** How wide it would like to be, before the panel shrinks everything to fit. */
    virtual int getPreferredWidth() const = 0;

    /** Catches up with the parameter's value, e.g. after the plugin's own window changed it. */
    virtual void refresh() = 0;

    virtual void setFromKnob (float /*position*/) {}
    virtual void setFromSwitch (bool /*isOn*/) {}
    virtual bool isOn() const     { return parameter.getValue() >= 0.5f; }

    bool isShown() const
    {
        return std::all_of (conditions.begin(), conditions.end(), [] (const Condition& c) { return c.holds(); });
    }

    const int knob, switchNumber;       // the controller's knob and switch for it, or 0
    std::vector<Condition> conditions;  // from the <Show>s it's in
    std::function<void()> onChange;     // after it changes the parameter

protected:
    void changeValue (float value)
    {
        if (value != parameter.getValue())
        {
            parameter.beginChangeGesture();
            parameter.setValueNotifyingHost (value);
            parameter.endChangeGesture();
        }

        refresh();

        if (onChange != nullptr)
            onChange();
    }

    // The label along the top, followed by the number of the knob (round) or switch (square) that
    // controls it, the two centred together
    void paintLabel (Graphics& g)
    {
        auto area = getLocalBounds().removeFromTop (labelHeight);
        const int number = knob > 0 ? knob : switchNumber;
        const Font font (15.0f, Font::bold);

        if (number > 0)
        {
            const int textWidth = jlimit (0, jmax (0, area.getWidth() - labelHeight - 4), font.getStringWidth (label));
            auto group = area.withSizeKeepingCentre (textWidth + 4 + labelHeight, labelHeight);
            const auto badge = group.removeFromRight (labelHeight).toFloat().reduced (2.0f);
            const auto accent = findColour (AppColourIds::signalPath);
            area = group.withTrimmedRight (4);

            g.setColour (accent);

            if (knob > 0)
                g.fillEllipse (badge);
            else
                g.fillRoundedRectangle (badge, 3.0f);

            g.setColour (accent.contrasting (1.0f));
            g.setFont (Font (11.0f, Font::bold));
            g.drawText (String (number), badge, Justification::centred);
        }

        g.setColour (findColour (Label::textColourId));
        g.setFont (font);
        g.drawFittedText (label, area, Justification::centred, 1);
    }

    AudioProcessorParameter& parameter;
    const String label;
};

//==============================================================================
// A knob, or a fader, with the value underneath
class BlockControlsPanel::KnobControl final : public Control
{
public:
    KnobControl (AudioProcessorParameter& p, const XmlElement& e, bool isFaderToUse)
        : Control (p, e), isFader (isFaderToUse), numSteps (getNumSteps (p))
    {
        slider.setSliderStyle (isFader ? Slider::LinearVertical : Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        slider.setRange (0.0, 1.0, numSteps > 1 ? 1.0 / (numSteps - 1) : 0.0);
        slider.setMouseDragSensitivity (200);
        slider.setDoubleClickReturnValue (true, p.getDefaultValue());
        slider.setValue (p.getValue(), dontSendNotification);

        slider.onDragStart = [this] { parameter.beginChangeGesture(); };
        slider.onDragEnd   = [this] { parameter.endChangeGesture(); };

        slider.onValueChange = [this]
        {
            parameter.setValueNotifyingHost ((float) slider.getValue());
            repaint();

            if (onChange != nullptr)
                onChange();
        };

        addAndMakeVisible (slider);
    }

    int getPreferredWidth() const override    { return isFader ? 64 : 88; }

    void refresh() override
    {
        // Not while it's being dragged, or it would fight the finger
        if (! slider.isMouseButtonDown() && std::abs ((float) slider.getValue() - parameter.getValue()) > 1.0e-4f)
        {
            slider.setValue (parameter.getValue(), dontSendNotification);
            repaint();
        }
    }

    void setFromKnob (float position) override
    {
        changeValue (numSteps > 1 ? getStepValue (roundToInt (position * (float) (numSteps - 1)), numSteps) : position);
    }

    void paint (Graphics& g) override
    {
        paintLabel (g);

        g.setColour (findColour (Label::textColourId).withAlpha (0.7f));
        g.setFont (Font (14.0f));
        g.drawFittedText (parameter.getCurrentValueAsText(), getLocalBounds().removeFromBottom (labelHeight),
                          Justification::centred, 1);
    }

    void resized() override
    {
        // The middle of the control, however wide the panel has made it
        const auto area = getLocalBounds().withTrimmedTop (labelHeight).withTrimmedBottom (labelHeight);
        slider.setBounds (area.withSizeKeepingCentre (isFader ? 40 : jmin (area.getWidth(), area.getHeight() + 24),
                                                      area.getHeight()));
    }

private:
    const bool isFader;
    const int numSteps;
    juce::Slider slider;
};

//==============================================================================
// An on/off button showing the parameter's value, e.g. "On"
class BlockControlsPanel::SwitchControl final : public Control
{
public:
    SwitchControl (AudioProcessorParameter& p, const XmlElement& e) : Control (p, e)
    {
        button.setWantsKeyboardFocus (false);
        button.onClick = [this] { setFromSwitch (! isOn()); };
        addAndMakeVisible (button);
        refresh();
    }

    int getPreferredWidth() const override    { return 80; }

    void refresh() override
    {
        button.setToggleState (isOn(), dontSendNotification);
        button.setButtonText (parameter.getCurrentValueAsText());
    }

    void setFromSwitch (bool shouldBeOn) override
    {
        changeValue (shouldBeOn ? 1.0f : 0.0f);
    }

    void paint (Graphics& g) override    { paintLabel (g); }

    void resized() override
    {
        button.setBounds (getLocalBounds().withTrimmedTop (labelHeight)
                                          .withSizeKeepingCentre (jmin (getWidth(), maxSwitchWidth), buttonHeight));
    }

private:
    TextButton button;
};

//==============================================================================
// One of a parameter's steps, from a row of buttons or a list
class BlockControlsPanel::ChoiceControl final : public Control
{
public:
    ChoiceControl (AudioProcessorParameter& p, const XmlElement& e)
        : Control (p, e), numSteps (jmax (2, getNumSteps (p)))
    {
        // The definition file's names for the steps, then the plugin's for any it doesn't name
        StringArray names;
        names.addTokens (e.getStringAttribute ("labels"), "|", {});
        names.trim();

        for (int step = names.size(); step < numSteps; ++step)
            names.add (p.getText (getStepValue (step, numSteps), 64));

        const auto style = e.getStringAttribute ("style");
        useList = style.equalsIgnoreCase ("list") || (style.isEmpty() && numSteps > 4);

        if (useList)
        {
            for (int step = 0; step < numSteps; ++step)
                list.addItem (names[step], step + 1);

            list.setWantsKeyboardFocus (false);
            list.onChange = [this] { changeValue (getStepValue (list.getSelectedId() - 1, numSteps)); };
            addAndMakeVisible (list);
        }
        else
        {
            for (int step = 0; step < numSteps; ++step)
            {
                auto* button = buttons.add (new TextButton (names[step]));
                button->setWantsKeyboardFocus (false);
                button->setConnectedEdges ((step > 0 ? Button::ConnectedOnLeft : 0)
                                            | (step + 1 < numSteps ? Button::ConnectedOnRight : 0));
                button->onClick = [this, step] { changeValue (getStepValue (step, numSteps)); };
                addAndMakeVisible (button);
            }
        }

        refresh();
    }

    int getPreferredWidth() const override
    {
        if (useList)
            return 170;

        int width = 0;

        for (auto* button : buttons)
            width += getButtonWidth (*button);

        return width;
    }

    void refresh() override
    {
        const int step = getStep (parameter, numSteps);

        if (useList)
            list.setSelectedId (step + 1, dontSendNotification);

        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == step, dontSendNotification);
    }

    void setFromKnob (float position) override
    {
        changeValue (getStepValue (roundToInt (position * (float) (numSteps - 1)), numSteps));
    }

    void setFromSwitch (bool shouldBeOn) override
    {
        changeValue (getStepValue (shouldBeOn ? numSteps - 1 : 0, numSteps));
    }

    bool isOn() const override    { return getStep (parameter, numSteps) > 0; }

    void paint (Graphics& g) override    { paintLabel (g); }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedTop (labelHeight).withSizeKeepingCentre (getWidth(), buttonHeight);
        list.setBounds (area.withSizeKeepingCentre (jmin (area.getWidth(), maxListWidth), buttonHeight));

        // The buttons grow with the room the panel gives them, up to half as wide again, and are
        // shared out in proportion to the width each would like
        area = area.withSizeKeepingCentre (jmin (area.getWidth(), getPreferredWidth() * 3 / 2), buttonHeight);
        const auto scale = (float) area.getWidth() / (float) jmax (1, getPreferredWidth());

        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setBounds (i + 1 < buttons.size() ? area.removeFromLeft (roundToInt ((float) getButtonWidth (*buttons[i]) * scale))
                                                          : area);
    }

private:
    static int getButtonWidth (const TextButton& button)
    {
        const Font font (jmin (TouchLookAndFeel::fontSize, (float) buttonHeight * 0.45f));
        return jmax (60, font.getStringWidth (button.getButtonText()) + 24);
    }

    const int numSteps;
    bool useList = false;
    ComboBox list;
    OwnedArray<TextButton> buttons;
};

//==============================================================================
BlockControlsPanel::BlockControlsPanel (AudioDeviceManager& dm)
    : deviceManager (dm), controller (ControllerConfig::load())
{
    setLookAndFeel (&lookAndFeel);

    closeButton.setButtonText (String (CharPointer_UTF8 ("\xc3\x97")));
    closeButton.setWantsKeyboardFocus (false);
    closeButton.onClick = [this]
    {
        if (onClose != nullptr)
            onClose();
    };
    addAndMakeVisible (closeButton);

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
                                    const String& blockName, const String& blockSubtitle, BlockIcon blockIcon)
{
    clear();

    node = std::move (newNode);
    name = blockName;
    subtitle = blockSubtitle;
    icon = blockIcon;

    if (node != nullptr && node->getProcessor() != nullptr)
        addControls (definition, {});

    updateVisibility();
    startTimerHz (20);
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
    stopTimer();
    controls.clear();
    node = nullptr;
}

void BlockControlsPanel::addControls (const XmlElement& element, const std::vector<Condition>& conditions)
{
    auto& processor = *node->getProcessor();

    for (auto* child : element.getChildIterator())
    {
        auto* parameter = findParameterByName (processor, child->getStringAttribute ("param"));

        // A definition file that doesn't match the plugin (e.g. after the plugin was updated)
        if (parameter == nullptr)
        {
            DBG ("Block controls: no parameter called " << child->getStringAttribute ("param"));
            continue;
        }

        if (child->hasTagName ("Show"))
        {
            auto innerConditions = conditions;
            innerConditions.push_back ({ parameter, child->getStringAttribute ("value"), child->getIntAttribute ("step", -1) });
            addControls (*child, innerConditions);
            continue;
        }

        std::unique_ptr<Control> control;

        if (child->hasTagName ("Knob"))         control = std::make_unique<KnobControl> (*parameter, *child, false);
        else if (child->hasTagName ("Fader"))   control = std::make_unique<KnobControl> (*parameter, *child, true);
        else if (child->hasTagName ("Switch"))  control = std::make_unique<SwitchControl> (*parameter, *child);
        else if (child->hasTagName ("Choice"))  control = std::make_unique<ChoiceControl> (*parameter, *child);

        if (control == nullptr)
            continue;

        control->conditions = conditions;

        // A choice can bring other controls into view, so check straight away rather than on the next tick
        control->onChange = [this] { updateVisibility(); };
        addChildComponent (control.get());
        controls.add (control.release());
    }
}

void BlockControlsPanel::updateVisibility()
{
    bool changed = false;

    for (auto* control : controls)
    {
        const bool shown = control->isShown();
        changed = changed || shown != control->isVisible();
        control->setVisible (shown);
    }

    if (changed)
        resized();
}

void BlockControlsPanel::timerCallback()
{
    for (auto* control : controls)
        control->refresh();

    updateVisibility();
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
    area.removeFromLeft (infoWidth + padding);

    // Left to right across the whole width, which is shared out in proportion to the room each would like
    Array<Control*> shown;
    int preferredWidth = 0;

    for (auto* control : controls)
    {
        if (control->isVisible())
        {
            shown.add (control);
            preferredWidth += control->getPreferredWidth();
        }
    }

    const int gaps = jmax (0, shown.size() - 1) * controlGap;
    const auto scale = (float) (area.getWidth() - gaps) / (float) jmax (1, preferredWidth);
    int x = area.getX();

    for (int i = 0; i < shown.size(); ++i)
    {
        // The last one takes whatever's left after rounding, so the row ends at the edge
        const int width = i + 1 < shown.size() ? roundToInt ((float) shown[i]->getPreferredWidth() * scale)
                                               : area.getRight() - x;
        shown[i]->setBounds (x, area.getY(), width, area.getHeight());
        x += width + controlGap;
    }
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
        for (auto* control : controls)
            if (control->knob == move->number && control->isVisible())
                control->setFromKnob (move->position);

        return;
    }

    if (const auto change = controller.findSwitch (message))
    {
        // A toggling switch acts when it's pressed, not when it's let go
        if (change->toggles && ! change->isOn)
            return;

        for (auto* control : controls)
            if (control->switchNumber == change->number && control->isVisible())
                control->setFromSwitch (change->toggles ? ! control->isOn() : change->isOn);
    }
}

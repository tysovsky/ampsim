#include "ControlStrip.h"
#include "ThemeManager.h"
#include "TouchLookAndFeel.h"
#include "../Plugins/PluginModules.h"

namespace
{
    constexpr int controlGap = 8;
    constexpr int labelHeight = 20;
    constexpr int buttonHeight = 44;

    // The widest a switch or list gets when its control has more room than it needs
    constexpr int maxSwitchWidth = 140;
    constexpr int maxListWidth = 240;

    // How long a trigger holds its parameter at the top, for the plugin to see the pulse
    constexpr int triggerMilliseconds = 200;

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

    void setParameter (AudioProcessorParameter& parameter, float value)
    {
        if (value != parameter.getValue())
        {
            parameter.beginChangeGesture();
            parameter.setValueNotifyingHost (value);
            parameter.endChangeGesture();
        }
    }

    // A block's property for what one of its <View>s is showing
    Identifier getViewProperty (const String& viewName)
    {
        return "view." + viewName;
    }

    AudioProcessorParameter* findParameterFor (AudioProcessor& processor, const XmlElement& e)
    {
        return findParameterByName (processor, e.getStringAttribute ("param"), e.getIntAttribute ("occurrence", 1));
    }
}

//==============================================================================
// A <Show>'s condition: on a parameter's value, or on what a <View> is showing
struct ControlStrip::Condition
{
    AudioProcessorParameter* parameter = nullptr;
    String value;   // the parameter's text for the value, e.g. "Roses", or the view's option
    int step = -1;  // or the parameter's step, counting from 0, if this isn't -1
    String view;    // the view's name, for a condition on a view

    bool holds (const AudioProcessorGraph::Node& node) const
    {
        if (view.isNotEmpty())
            return node.properties[getViewProperty (view)].toString().equalsIgnoreCase (value);

        if (step >= 0)
            return getStep (*parameter, getNumSteps (*parameter)) == step;

        return parameter->getCurrentValueAsText().equalsIgnoreCase (value);
    }
};

//==============================================================================
// One control on the strip
class ControlStrip::Control : public Component
{
public:
    Control (const XmlElement& e, const String& defaultLabel)
        : knob (e.getIntAttribute ("knob")),
          switchNumber (e.getIntAttribute ("switch")),
          label (e.getStringAttribute ("label", defaultLabel))
    {
    }

    /** How wide it would like to be, before the strip shares out its width. */
    virtual int getPreferredWidth() const = 0;

    /** Catches up with the parameter's value, e.g. after the plugin's own window changed it. */
    virtual void refresh() {}

    virtual void setFromKnob (float /*position*/) {}
    virtual void setFromSwitch (bool /*isOn*/) {}
    virtual bool isOn() const     { return false; }

    bool isShown (const AudioProcessorGraph::Node& node) const
    {
        return std::all_of (conditions.begin(), conditions.end(), [&] (const Condition& c) { return c.holds (node); });
    }

    const int knob, switchNumber;       // the controller's knob and switch for it, or 0
    std::vector<Condition> conditions;  // from the <Show>s it's in
    std::function<void()> onChange;     // after it changes something

protected:
    void changed()
    {
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
            auto group = area.withSizeKeepingCentre (textWidth + (label.isEmpty() ? 0 : 4) + labelHeight, labelHeight);
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

    const String label;
};

//==============================================================================
// A knob, or a fader, with the value underneath
class ControlStrip::KnobControl final : public Control
{
public:
    KnobControl (AudioProcessorParameter& p, const XmlElement& e, bool isFaderToUse)
        : Control (e, p.getName (32)), parameter (p), isFader (isFaderToUse), numSteps (getNumSteps (p))
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
        setParameter (parameter, numSteps > 1 ? getStepValue (roundToInt (position * (float) (numSteps - 1)), numSteps) : position);
        changed();
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
        // The middle of the control, however wide the strip has made it
        const auto area = getLocalBounds().withTrimmedTop (labelHeight).withTrimmedBottom (labelHeight);
        slider.setBounds (area.withSizeKeepingCentre (isFader ? 40 : jmin (area.getWidth(), area.getHeight() + 24),
                                                      area.getHeight()));
    }

private:
    AudioProcessorParameter& parameter;
    const bool isFader;
    const int numSteps;
    juce::Slider slider;
};

//==============================================================================
// An on/off button showing the parameter's value, e.g. "On"
class ControlStrip::SwitchControl final : public Control
{
public:
    SwitchControl (AudioProcessorParameter& p, const XmlElement& e)
        : Control (e, p.getName (32)), parameter (p)
    {
        button.setWantsKeyboardFocus (false);
        button.onClick = [this] { setFromSwitch (! isOn()); };
        addAndMakeVisible (button);
        refresh();
    }

    int getPreferredWidth() const override    { return 80; }
    bool isOn() const override                { return parameter.getValue() >= 0.5f; }

    void refresh() override
    {
        button.setToggleState (isOn(), dontSendNotification);
        button.setButtonText (parameter.getCurrentValueAsText());
    }

    void setFromSwitch (bool shouldBeOn) override
    {
        setParameter (parameter, shouldBeOn ? 1.0f : 0.0f);
        changed();
    }

    void paint (Graphics& g) override    { paintLabel (g); }

    void resized() override
    {
        button.setBounds (getLocalBounds().withTrimmedTop (labelHeight)
                                          .withSizeKeepingCentre (jmin (getWidth(), maxSwitchWidth), buttonHeight));
    }

private:
    AudioProcessorParameter& parameter;
    TextButton button;
};

//==============================================================================
// One of a set of options, from a row of buttons or a list: a parameter's steps, or a <View>'s
// options
class ControlStrip::ChoiceControl final : public Control
{
public:
    ChoiceControl (const XmlElement& e, const String& defaultLabel, const StringArray& names,
                   std::function<int()> getChoice, std::function<void (int)> setChoice)
        : Control (e, defaultLabel), numChoices (jmax (2, names.size())), get (std::move (getChoice)), set (std::move (setChoice))
    {
        const auto style = e.getStringAttribute ("style");
        useList = style.equalsIgnoreCase ("list") || (style.isEmpty() && numChoices > 4);

        if (useList)
        {
            for (int i = 0; i < names.size(); ++i)
                list.addItem (names[i], i + 1);

            list.setWantsKeyboardFocus (false);
            list.onChange = [this] { choose (list.getSelectedId() - 1); };
            addAndMakeVisible (list);
        }
        else
        {
            for (int i = 0; i < names.size(); ++i)
            {
                auto* button = buttons.add (new TextButton (names[i]));
                button->setWantsKeyboardFocus (false);
                button->setConnectedEdges ((i > 0 ? Button::ConnectedOnLeft : 0)
                                            | (i + 1 < names.size() ? Button::ConnectedOnRight : 0));
                button->onClick = [this, i] { choose (i); };
                addAndMakeVisible (button);
            }
        }

        refresh();
    }

    /** The steps of a parameter, named by the definition file's labels="A|B|C" where it has them */
    static std::unique_ptr<ChoiceControl> forParameter (AudioProcessorParameter& p, const XmlElement& e)
    {
        const int numSteps = jmax (2, getNumSteps (p));
        StringArray names;
        names.addTokens (e.getStringAttribute ("labels"), "|", {});
        names.trim();

        for (int step = names.size(); step < numSteps; ++step)
            names.add (p.getText (getStepValue (step, numSteps), 64));

        names.removeRange (numSteps, names.size());

        return std::make_unique<ChoiceControl> (e, p.getName (32), names,
                                                [&p, numSteps] { return getStep (p, numSteps); },
                                                [&p, numSteps] (int step) { setParameter (p, getStepValue (step, numSteps)); });
    }

    /** A <View>'s options, which pick what's shown rather than change the plugin. The first is
        shown until another is picked. */
    static std::unique_ptr<ChoiceControl> forView (AudioProcessorGraph::Node::Ptr node, const XmlElement& e)
    {
        const auto property = getViewProperty (e.getStringAttribute ("name"));
        const auto options = StringArray::fromTokens (e.getStringAttribute ("options"), "|", {});

        if (! node->properties.contains (property) && ! options.isEmpty())
            node->properties.set (property, options[0]);

        return std::make_unique<ChoiceControl> (e, String(), options,
                                                [node, property, options] { return jmax (0, options.indexOf (node->properties[property].toString(), true)); },
                                                [node, property, options] (int i) { node->properties.set (property, options[i]); });
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
        const int choice = get();

        if (useList)
            list.setSelectedId (choice + 1, dontSendNotification);

        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == choice, dontSendNotification);
    }

    // A knob turns it through its options, and a switch picks the first or last
    void setFromKnob (float position) override         { choose (roundToInt (position * (float) (numChoices - 1))); }
    void setFromSwitch (bool shouldBeOn) override      { choose (shouldBeOn ? numChoices - 1 : 0); }
    bool isOn() const override                         { return get() > 0; }

    void paint (Graphics& g) override    { paintLabel (g); }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedTop (labelHeight).withSizeKeepingCentre (getWidth(), buttonHeight);
        list.setBounds (area.withSizeKeepingCentre (jmin (area.getWidth(), maxListWidth), buttonHeight));

        // The buttons grow with the room the strip gives them, up to half as wide again, and are
        // shared out in proportion to the width each would like
        area = area.withSizeKeepingCentre (jmin (area.getWidth(), getPreferredWidth() * 3 / 2), buttonHeight);
        const auto scale = (float) area.getWidth() / (float) jmax (1, getPreferredWidth());

        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setBounds (i + 1 < buttons.size() ? area.removeFromLeft (roundToInt ((float) getButtonWidth (*buttons[i]) * scale))
                                                          : area);
    }

private:
    void choose (int choice)
    {
        set (jlimit (0, numChoices - 1, choice));
        changed();
    }

    static int getButtonWidth (const TextButton& button)
    {
        const Font font (jmin (TouchLookAndFeel::fontSize, (float) buttonHeight * 0.45f));
        return jmax (60, font.getStringWidth (button.getButtonText()) + 24);
    }

    const int numChoices;
    const std::function<int()> get;
    const std::function<void (int)> set;
    bool useList = false;
    ComboBox list;
    OwnedArray<TextButton> buttons;
};

//==============================================================================
// A button that pulses a parameter up and back down, for plugins that act on a parameter
// changing, such as stepping to the next preset
class ControlStrip::TriggerControl final : public Control,
                                           private Timer
{
public:
    TriggerControl (AudioProcessorParameter& p, const XmlElement& e)
        : Control (e, String()), parameter (p)
    {
        button.setButtonText (e.getStringAttribute ("text", p.getName (32)));
        button.setWantsKeyboardFocus (false);
        button.onClick = [this] { trigger(); };
        addAndMakeVisible (button);
    }

    ~TriggerControl() override
    {
        // Not left at the top, if it goes mid-pulse
        if (isTimerRunning())
        {
            parameter.setValueNotifyingHost (0.0f);
            parameter.endChangeGesture();
        }
    }

    int getPreferredWidth() const override    { return 96; }

    // A switch pulses it when it's switched on
    void setFromSwitch (bool shouldBeOn) override
    {
        if (shouldBeOn)
            trigger();
    }

    void paint (Graphics& g) override    { paintLabel (g); }

    void resized() override
    {
        button.setBounds (getLocalBounds().withTrimmedTop (labelHeight)
                                          .withSizeKeepingCentre (jmin (getWidth(), maxSwitchWidth), buttonHeight));
    }

private:
    void trigger()
    {
        if (isTimerRunning())
            return;

        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (1.0f);
        startTimer (triggerMilliseconds);
    }

    void timerCallback() override
    {
        stopTimer();
        parameter.setValueNotifyingHost (0.0f);
        parameter.endChangeGesture();
        changed();
    }

    AudioProcessorParameter& parameter;
    TextButton button;
};

//==============================================================================
ControlStrip::ControlStrip() = default;

ControlStrip::~ControlStrip()
{
    clear();
}

void ControlStrip::setControls (AudioProcessorGraph::Node::Ptr newNode, const XmlElement& definition)
{
    clear();
    node = std::move (newNode);

    if (node != nullptr && node->getProcessor() != nullptr)
        addControls (definition, {});

    updateVisibility();
    startTimerHz (20);
    resized();
}

void ControlStrip::clear()
{
    stopTimer();
    controls.clear();
    node = nullptr;
}

void ControlStrip::addControls (const XmlElement& element, const std::vector<Condition>& conditions)
{
    auto& processor = *node->getProcessor();

    for (auto* child : element.getChildIterator())
    {
        if (child->hasTagName ("Show"))
        {
            Condition condition;
            condition.value = child->getStringAttribute ("value");
            condition.step = child->getIntAttribute ("step", -1);
            condition.view = child->getStringAttribute ("view");

            if (condition.view.isEmpty())
                condition.parameter = findParameterFor (processor, *child);

            // A definition file that doesn't match the plugin (e.g. after the plugin was updated)
            if (condition.view.isEmpty() && condition.parameter == nullptr)
            {
                DBG ("Block controls: no parameter called " << child->getStringAttribute ("param"));
                continue;
            }

            auto innerConditions = conditions;
            innerConditions.push_back (condition);
            addControls (*child, innerConditions);
            continue;
        }

        std::unique_ptr<Control> control;

        if (child->hasTagName ("View"))
        {
            control = ChoiceControl::forView (node, *child);
        }
        else if (auto* parameter = findParameterFor (processor, *child))
        {
            if (child->hasTagName ("Knob"))          control = std::make_unique<KnobControl> (*parameter, *child, false);
            else if (child->hasTagName ("Fader"))    control = std::make_unique<KnobControl> (*parameter, *child, true);
            else if (child->hasTagName ("Switch"))   control = std::make_unique<SwitchControl> (*parameter, *child);
            else if (child->hasTagName ("Choice"))   control = ChoiceControl::forParameter (*parameter, *child);
            else if (child->hasTagName ("Trigger"))  control = std::make_unique<TriggerControl> (*parameter, *child);
        }
        else
        {
            DBG ("Block controls: no parameter called " << child->getStringAttribute ("param"));
        }

        if (control == nullptr)
            continue;

        control->conditions = conditions;

        // A choice can bring other controls into view, so check straight away rather than on the next tick
        control->onChange = [this, isView = child->hasTagName ("View")]
        {
            updateVisibility();

            if (isView && onViewChange != nullptr)
                onViewChange();
        };
        addChildComponent (control.get());
        controls.add (control.release());
    }
}

void ControlStrip::updateVisibility()
{
    if (node == nullptr)
        return;

    bool changed = false;

    for (auto* control : controls)
    {
        const bool shown = control->isShown (*node);
        changed = changed || shown != control->isVisible();
        control->setVisible (shown);
    }

    if (changed)
        resized();
}

void ControlStrip::timerCallback()
{
    for (auto* control : controls)
        control->refresh();

    updateVisibility();
}

bool ControlStrip::handleKnob (int number, float position)
{
    bool isUsed = false;

    for (auto* control : controls)
    {
        if (control->knob == number && control->isVisible())
        {
            control->setFromKnob (position);
            isUsed = true;
        }
    }

    return isUsed;
}

bool ControlStrip::handleSwitch (int number, bool toggles, bool isOn)
{
    bool isUsed = false;

    for (auto* control : controls)
    {
        if (control->switchNumber == number && control->isVisible())
        {
            control->setFromSwitch (toggles ? ! control->isOn() : isOn);
            isUsed = true;
        }
    }

    return isUsed;
}

void ControlStrip::resized()
{
    // Left to right across the whole width, which is shared out in proportion to the room each would like
    const auto area = getLocalBounds();
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

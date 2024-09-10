#pragma once

#include <JuceHeader.h>
#include "Components.h"

class PluginComponent;

class PluginControlPanel : public Component
{
};

class PluginControl
{
public:
	PluginControl() {}
	~PluginControl() {
		if (topPanel)
			delete topPanel;

		if (bottomPanel)
			delete bottomPanel;
	}
	virtual controls::TopPanel* getTopPanel() { return topPanel;  }
	virtual controls::BottomPanel* getBottomPanel() { return bottomPanel; }

	controls::TopPanel* topPanel = nullptr;
	controls::BottomPanel* bottomPanel = nullptr;
};

class Slider {
private:
	String paramName, label;
};

class SliderPanel : public Component {
public:
	SliderPanel(
		std::map<String, AudioProcessorParameter*> parametersMap,
		std::vector<String> paramNames)
	{
		parameters = extractParams(parametersMap, paramNames);
		
		init();
	}

	SliderPanel(
		std::map<String, AudioProcessorParameter*> parametersMap,
		std::vector<String> paramNames,
		std::vector<String> paramLabels) : paramLabels(paramLabels)
	{
		if (paramNames.size() != paramLabels.size()) {
			throw std::invalid_argument("Number of parameters and labels has to match");
		}

		parameters = extractParams(parametersMap, paramNames);

		init();
	}


	~SliderPanel() {
		for (int i = 0; i < sliders.size(); i++) {
			delete sliders[i];
		}

		for (int i = 0; i < labels.size(); i++) {
			delete labels[i];
		}
	}

	void resized() override {
		auto bounds = getLocalBounds();

		auto sliderWidth = (int)(bounds.getWidth() / sliders.size());

		auto labelBounds = bounds.removeFromTop(15);

		for (int i = 0; i < sliders.size(); i++) {
			auto slider = sliders[i];
			auto label = labels[i];

			slider->setBounds(bounds.removeFromLeft(sliderWidth));
			label->setBounds(labelBounds.removeFromLeft(sliderWidth));
		}
	}

private:
	void init() {
		for (int i = 0; i < parameters.size(); i++) {
			auto parameter = parameters[i];

			if (!parameter) {
				continue;
			}

			auto slider = new juce::Slider();
			auto label = new Label();

			auto parameterName = paramLabels.size() > 0 ? paramLabels[i] : parameter->getName(20);

			label->setText(parameterName, NotificationType::dontSendNotification);
			label->setJustificationType(Justification::centred);

			slider->setName(parameter->getName(20));
			slider->setRange(Range<double>(0, 100), 1);
			slider->setValue(parameter->getValue() * slider->getRange().getEnd());
			slider->setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);

			slider->onValueChange = [this, slider, parameter]() {
				parameter->setValue(slider->getValue() / slider->getRange().getEnd());
				};


			sliders.push_back(slider);
			labels.push_back(label);

			addAndMakeVisible(slider);
			addAndMakeVisible(label);
		}
	}

	std::vector<AudioProcessorParameter*> extractParams(
		std::map<String, AudioProcessorParameter*> paramMap,
		std::vector<String> paramNames) 
	{
		std::vector<AudioProcessorParameter*> result;

		for (auto pName : paramNames) {
			auto param = paramMap[pName];

			if (!param) {
				throw std::invalid_argument("Plugin parameters do not contain a parameter " + pName.toStdString());
			}

			result.push_back(param);
		}

		return result;
	}

	std::vector<AudioProcessorParameter*> parameters;
	std::vector<String> paramLabels;
	std::vector<juce::Slider*> sliders;
	std::vector<Label*> labels;
};

class SliderPanelGroup : public Component {
public:
	SliderPanelGroup(std::vector<SliderPanel*> p) : panels(p) {
		activePanel = p[0];

		for (auto panel : panels) {
			addChildComponent(panel);
		}

		activePanel->setVisible(true);
	}
	~SliderPanelGroup() {};

	void setActivePanel(int idx) {
		for (auto panel : panels) {
			panel->setVisible(false);
		}

		activePanel = panels[idx];
		activePanel->setVisible(true);

		resized();
	}

	void resized() override {
		auto bounds = getLocalBounds();

		activePanel->setBounds(bounds);
	}

private:
	std::vector<SliderPanel*> panels;
	SliderPanel* activePanel;
};

class ButtonGroup : public Component,
	public Button::Listener

{
public:
	class Listener {
	public:
		virtual void onButtonClicked(ButtonGroup* buttonGroup, int index, bool enabled) = 0;
	};


	ButtonGroup(
		std::map<String, AudioProcessorParameter*> parametersMap,
		std::vector<String> paramNames,
		std::vector<String> buttonNames
	)
	{
		init(parametersMap, paramNames, buttonNames);
	}

	ButtonGroup(
		std::map<String, AudioProcessorParameter*> parametersMap,
		std::vector<String> paramNames,
		std::vector<String> buttonNames,
		String label
	)
	{
		groupLabel = std::make_unique<Label>();
		groupLabel->setText(label, NotificationType::dontSendNotification);
		groupLabel->setJustificationType(Justification::centred);
		addAndMakeVisible(groupLabel.get());

		init(parametersMap, paramNames, buttonNames);
	}

	~ButtonGroup() {
		for (auto btn : buttons) {
			delete btn;
		}
	}

	int getNumButtons() {
		return buttons.size();
	}

	void addListener(Listener* listener) {
		listeners.push_back(listener);
	}

	void resized() override {
		auto bounds = getLocalBounds();

		if (groupLabel) {
			groupLabel->setBounds(bounds.removeFromTop(13));
		}

		auto buttonWidth = (int)(bounds.getWidth() / buttons.size());

		for (auto btn : buttons) {
			btn->setBounds(bounds.removeFromLeft(buttonWidth));
		}
	}

	void buttonClicked(Button* clickedBtn) override {
		for (int i = 0; i < buttons.size(); i++) {
			auto btn = buttons[i];

			if (btn == clickedBtn) {
				bool enabled = !btn->getToggleState();

				if (enabled) {
					params[i]->setValue(1.0);
				}
				else {
					params[i]->setValue(0.0);
				}

				btn->setToggleState(enabled, false);
				for (auto listener : listeners) {
					listener->onButtonClicked(this, i, enabled);
				}
			}
			else if (exclusiveMode) {
				btn->setToggleState(false, false);
			}
		}
	}

private:
	void init(
		std::map<String, AudioProcessorParameter*> parametersMap,
		std::vector<String> paramNames,
		std::vector<String> buttonNames
	)
	{
		if (paramNames.size() != buttonNames.size()) {
			throw std::invalid_argument("Number of parameters and number of button names must match");
		}

		for (int i = 0; i < paramNames.size(); i++) {
			auto param = parametersMap[paramNames[i]];

			if (!param) {
				throw std::invalid_argument("param doesn't exist: " + paramNames[i].toStdString());
			}

			params.push_back(param);

			auto btn = new TextButton(buttonNames[i]);

			btn->addListener(this);
			addAndMakeVisible(btn);
			buttons.push_back(btn);

			if (param->getValue() > .5) {
				btn->setToggleState(true, false);
			}
		}
	}


	std::vector<Button*> buttons;
	std::vector<AudioProcessorParameter*> params;
	std::unique_ptr<Label> groupLabel = nullptr;
	std::vector<Listener*> listeners;
	bool exclusiveMode = false;
};

class ExclusiveButtonGroup : public Component,
	public Button::Listener

{
public:
	class Listener {
	public:
		virtual void onButtonClicked(ExclusiveButtonGroup* buttonGroup, int index) = 0;
	};

	ExclusiveButtonGroup(
		std::map<String, AudioProcessorParameter*> parametersMap,
		String paramName,
		std::vector<String> buttonNames
	) {
		param = parametersMap[paramName];

		if (!param) {
			throw std::invalid_argument("Plugin parameters do not contain a parameter " + paramName.toStdString());
		}

		init(buttonNames);
	}

	ExclusiveButtonGroup(
		std::map<String, AudioProcessorParameter*> parametersMap,
		String paramName,
		String label,
		std::vector<String> buttonNames
	)
	{
		groupLabel = std::make_unique<Label>();
		groupLabel->setText(label, NotificationType::dontSendNotification);
		groupLabel->setJustificationType(Justification::centred);
		addAndMakeVisible(groupLabel.get());

		param = parametersMap[paramName];

		if (!param) {
			throw std::invalid_argument("Plugin parameters do not contain a parameter " + paramName.toStdString());
		}


		init(buttonNames);
	}

	~ExclusiveButtonGroup() {
		for (auto btn : buttons) {
			delete btn;
		}
	}

	int getNumButtons() {
		return buttons.size();
	}

	void addListener(Listener* listener) {
		listeners.push_back(listener);
	}

	void bindSliderPanelGroup(SliderPanelGroup* sliderPanelGroup) {
		this->sliderPanelGroup = sliderPanelGroup;

		sliderPanelGroup->setActivePanel(getCurrentIndex());
	}

	void resized() override {
		auto bounds = getLocalBounds();

		if (groupLabel) {
			groupLabel->setBounds(bounds.removeFromTop(13));
		}

		auto buttonWidth = (int)(bounds.getWidth() / buttons.size());

		for (auto btn : buttons) {
			btn->setBounds(bounds.removeFromLeft(buttonWidth));
		}
	}

	void buttonClicked(Button* clickedBtn) override {
		for (int i = 0; i < buttons.size(); i++) {
			auto btn = buttons[i];

			if (btn == clickedBtn) {
				btn->setToggleState(true, false);
				for (auto listener : listeners) {
					listener->onButtonClicked(this, i);
				}

				auto value = float(i) / (buttons.size() - 1);
				param->setValue(value);


				if(sliderPanelGroup)
					sliderPanelGroup->setActivePanel(i);
			}
			else {
				btn->setToggleState(false, false);
			}
		}
	}

private:
	void init(std::vector<String> buttonNames) {
		for (auto s : buttonNames) {
			auto btn = new TextButton(s);

			btn->addListener(this);
			addAndMakeVisible(btn);
			buttons.push_back(btn);
		}

		buttons[getCurrentIndex()]->setToggleState(true, false);
	}

	int getCurrentIndex() {
		if (!param) {
			return 0;
		}

		auto value = param->getValue();
		return int(value * (buttons.size() - 1));
	}

	std::vector<Button*> buttons;
	std::unique_ptr<Label> groupLabel = nullptr;
	std::vector<Listener*> listeners;
	SliderPanelGroup* sliderPanelGroup = nullptr;
	AudioProcessorParameter* param = nullptr;
};
#include "DarkGlass.h"
#include "../PluginComponent.h"


DarkGlassTopPanel::DarkGlassTopPanel(DarkGlassControl* control) : control(control)
{
	auto paramMap = control->pluginComponent->getParameters();

	ampType = std::make_unique<ExclusiveButtonGroup>(
		paramMap,
		"Model",
		"Amp Model",
		std::vector<String>{ "B7U", "VDU"}
	);
	addAndMakeVisible(ampType.get());

	attack = std::make_unique<ExclusiveButtonGroup>(
		paramMap,
		"Attack",
		"Attack",
		std::vector<String>{ "Cut", "Boost", "Neutral"}
	);
	addAndMakeVisible(attack.get());

	grunt = std::make_unique<ExclusiveButtonGroup>(
		paramMap,
		"Grunt",
		"Grunt",
		std::vector<String>{ "Cut", "Boost", "Neutral"}
	);
	addAndMakeVisible(grunt.get());

	lowMid = std::make_unique<ExclusiveButtonGroup>(
		paramMap,
		"Low Mid Switch",
		"Low Mids",
		std::vector<String>{ "250 Hz", "1 kHz", "500 Hz"}
	);
	addAndMakeVisible(lowMid.get());

	highMid = std::make_unique<ExclusiveButtonGroup>(
		paramMap,
		"High Mid Switch",
		"High Mids",
		std::vector<String>{ "750 Hz", "3 kHz", "1.5 kHz"}
	);
	addAndMakeVisible(highMid.get());

	audioSliderPanel = std::make_unique<SliderPanel>(
		paramMap,
		std::vector<String>{ "Output Gain" },
		std::vector<String>{ "Output Gain" }
	);
	addAndMakeVisible(audioSliderPanel.get());
}

DarkGlassTopPanel::~DarkGlassTopPanel() { }

void DarkGlassTopPanel::resized()
{
	auto bound = getLocalBounds();

	const int buttonWidth = 70;

	bound.removeFromLeft(10);

	ampType->setBounds(bound.removeFromLeft(buttonWidth * ampType->getNumButtons()));

	bound.removeFromLeft(10);

	attack->setBounds(bound.removeFromLeft(buttonWidth * attack->getNumButtons()));

	bound.removeFromLeft(10);

	grunt->setBounds(bound.removeFromLeft(buttonWidth * grunt->getNumButtons()));

	bound.removeFromLeft(10);

	lowMid->setBounds(bound.removeFromLeft(buttonWidth * lowMid->getNumButtons()));

	bound.removeFromLeft(10);

	highMid->setBounds(bound.removeFromLeft(buttonWidth * highMid->getNumButtons()));

	bound.removeFromLeft(10);

	audioSliderPanel->setBounds(bound);
}

void DarkGlassTopPanel::onButtonClicked(ButtonGroup* buttonGroup, int index, bool enabled) {

}

//==============================================================================

DarkGlassBottomPanel::DarkGlassBottomPanel(DarkGlassControl* control) : control(control) {
	auto paramMap = control->pluginComponent->getParameters();

	sliderPanel = std::make_unique<SliderPanel>(
		paramMap,
		std::vector<String>{
			"Blend",
			"Level",
			"Drive",
			"Bass",
			"Treble",
			"Low Mids",
			"High Mids",
			
		}
	);
	addAndMakeVisible(sliderPanel.get());
}

DarkGlassBottomPanel::~DarkGlassBottomPanel() { }

void DarkGlassBottomPanel::resized()
{
	sliderPanel->setBounds(getLocalBounds());
}

//==============================================================================

DarkGlassControl::DarkGlassControl(PluginComponent* comp) : pluginComponent(comp) {
	/*bottomPanel = std::make_unique<DarkGlassBottomPanel>(this);
	topPanel = std::make_unique<DarkGlassTopPanel>(this);*/
}

DarkGlassControl::~DarkGlassControl() { }

controls::TopPanel* DarkGlassControl::getTopPanel() {
	return topPanel.get();
}

controls::BottomPanel* DarkGlassControl::getBottomPanel() {
	return bottomPanel.get();
}

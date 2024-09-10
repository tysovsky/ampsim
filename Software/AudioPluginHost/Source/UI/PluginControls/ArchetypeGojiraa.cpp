#include "ArchetypeGojira.h"
#include "../PluginComponent.h"

ArchetypeGojiraTopPanel::ArchetypeGojiraTopPanel(ArchetypeGojiraControl* control) : control(control) {
	auto paramMap = control->pluginComponent->getParameters();

	ampTypeGroup = std::make_unique<ExclusiveButtonGroup>(
		paramMap,
		"Amp Type",
		"Amp",
		std::vector<String>{
		"Clean",
			"Rust",
			"Hot"
	});
	ampTypeGroup->bindSliderPanelGroup(dynamic_cast<ArchetypeGojiraBottomPanel*>(control->bottomPanel.get())->amplSliderPanels.get());
	addAndMakeVisible(ampTypeGroup.get());



	preEffectsGroup = std::make_unique<ButtonGroup>(
		paramMap,
		std::vector<String>{
			"OD Active",
			"DRT Active",
			"PHSR Active",
			"CHR Active"
		},
		std::vector<String>{
			"OD",
			"DRT",
			"PHSR",
			"CHR"
		},
		"Pre Effects");
	preEffectsGroup->addListener(this);
	addAndMakeVisible(preEffectsGroup.get());

	postEffectsGroup = std::make_unique<ButtonGroup>(
		paramMap,
		std::vector<String>{
			"DLY Active",
			"REV Active"

		},
		std::vector<String>{
			"DLY",
			"REV"
		},
		"Post Effects");
	postEffectsGroup->addListener(this);
	addAndMakeVisible(postEffectsGroup.get());

	audioSliderPanel = std::make_unique<SliderPanel>(
		paramMap,
		std::vector<String>{ "Output Gain" },
		std::vector<String>{ "Output Gain" }
	);
	addAndMakeVisible(audioSliderPanel.get());
}

ArchetypeGojiraTopPanel::~ArchetypeGojiraTopPanel(){ }

void ArchetypeGojiraTopPanel::resized() {
	auto bound = getLocalBounds();

	const int ampButtonWidth = 100;

	bound.removeFromLeft(10);

	preEffectsGroup->setBounds(bound.removeFromLeft(ampButtonWidth * preEffectsGroup->getNumButtons()));
	
	bound.removeFromLeft(10);

	ampTypeGroup->setBounds(bound.removeFromLeft(ampButtonWidth * ampTypeGroup->getNumButtons()));

	bound.removeFromLeft(10);

	postEffectsGroup->setBounds(bound.removeFromLeft(ampButtonWidth * postEffectsGroup->getNumButtons()));

	bound.removeFromLeft(10);

	audioSliderPanel->setBounds(bound);
}

void ArchetypeGojiraTopPanel::onButtonClicked(ButtonGroup* buttonGroup, int index, bool enabled) {
}

//==============================================================================

ArchetypeGojiraBottomPanel::ArchetypeGojiraBottomPanel(ArchetypeGojiraControl* control) : control(control) {
	auto paramMap = control->pluginComponent->getParameters();

	cleanAmpSliderPanel = std::make_unique<SliderPanel>(
		paramMap,
		std::vector<String>{
			"CLN Amp Gain",
			"CLN Amp Bass",
			"CLN Amp Mid",
			"CLN Amp Treble"
		},
		std::vector<String>{
			"Gain",
			"Bass",
			"Mid",
			"Treble"
		}
	);

	rustAmpSliderPanel = std::make_unique<SliderPanel>(
		paramMap,
		std::vector<String>{
			"RUST Amp Gain",
			"RUST Amp Low",
			"RUST Amp Mid",
			"RUST Amp High",
			"RUST Amp Presence",
			"RUST Amp Depth"
		},
		std::vector<String>{
			"Gain",
			"Low",
			"Mid",
			"High",
			"Presence",
			"Depth"
		}
	);


	hotAmpSliderPanel = std::make_unique<SliderPanel>(
		paramMap,
		std::vector<String>{
			"HOT Amp Gain",
			"HOT Amp Gain",
			"HOT Amp Mid",
			"HOT Amp High",
			"HOT Amp Presence",
			"HOT Amp Depth"
		},
		std::vector<String>{
			"Gain",
			"Low",
			"Mid",
			"High",
			"Presence",
			"Depth"
		}
	);


	amplSliderPanels = std::make_unique<SliderPanelGroup>(std::vector<SliderPanel*> {
		cleanAmpSliderPanel.get(),
		rustAmpSliderPanel.get(),
		hotAmpSliderPanel.get()
	});

	addAndMakeVisible(amplSliderPanels.get());
}

ArchetypeGojiraBottomPanel::~ArchetypeGojiraBottomPanel() {}

void ArchetypeGojiraBottomPanel::resized() {
	auto bound = getLocalBounds();

	amplSliderPanels->setBounds(bound);
}

//==============================================================================
ArchetypeGojiraControl::ArchetypeGojiraControl(PluginComponent* comp) : pluginComponent(comp) {
	//bottomPanel = std::make_unique<ArchetypeGojiraBottomPanel>(this);
	//topPanel = std::make_unique<ArchetypeGojiraTopPanel>(this);
}

ArchetypeGojiraControl::~ArchetypeGojiraControl() {}

controls::TopPanel* ArchetypeGojiraControl::getTopPanel() {
	return topPanel.get();
};
controls::BottomPanel* ArchetypeGojiraControl::getBottomPanel() {
	return bottomPanel.get();
};

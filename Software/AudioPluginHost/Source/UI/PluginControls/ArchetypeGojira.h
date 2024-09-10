#pragma once

#include <JuceHeader.h>
#include "PluginControl.h"
#include "Components.h"

class ArchetypeGojiraControl : public PluginControl
{
public:
	ArchetypeGojiraControl(PluginComponent* comp);
	~ArchetypeGojiraControl();

	controls::TopPanel* getTopPanel() override;
	controls::BottomPanel* getBottomPanel() override;

private:
	PluginComponent* pluginComponent;
	std::unique_ptr<controls::TopPanel> topPanel;
	std::unique_ptr<controls::BottomPanel> bottomPanel;

	friend class ArchetypeGojiraTopPanel;
	friend class ArchetypeGojiraBottomPanel;
};

class ArchetypeGojiraTopPanel : public PluginControlPanel,
								public ButtonGroup::Listener
{
public:
	ArchetypeGojiraTopPanel(ArchetypeGojiraControl* control);
	~ArchetypeGojiraTopPanel();

	void resized() override;
	void onButtonClicked(ButtonGroup* buttonGroup, int index, bool enabled) override;

private:
	ArchetypeGojiraControl* control;
	std::unique_ptr<ExclusiveButtonGroup> ampTypeGroup = nullptr;;
	std::unique_ptr<ButtonGroup> preEffectsGroup = nullptr;
	std::unique_ptr<ButtonGroup> postEffectsGroup = nullptr;
	std::unique_ptr<SliderPanel> audioSliderPanel = nullptr;
};

class ArchetypeGojiraBottomPanel : public PluginControlPanel {
public:
	ArchetypeGojiraBottomPanel(ArchetypeGojiraControl* control);
	~ArchetypeGojiraBottomPanel();

	void resized() override;

	std::unique_ptr<SliderPanelGroup> amplSliderPanels = nullptr;

private:
	ArchetypeGojiraControl* control;
	std::unique_ptr<SliderPanel> cleanAmpSliderPanel = nullptr;
	std::unique_ptr<SliderPanel> rustAmpSliderPanel = nullptr;
	std::unique_ptr<SliderPanel> hotAmpSliderPanel = nullptr;
};



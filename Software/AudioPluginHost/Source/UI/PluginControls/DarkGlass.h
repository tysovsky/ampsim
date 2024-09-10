#pragma once

#include <JuceHeader.h>
#include "PluginControl.h"

class DarkGlassControl : public PluginControl
{
public:
	DarkGlassControl(PluginComponent* comp);
	~DarkGlassControl();

	controls::TopPanel* getTopPanel() override;
	controls::BottomPanel* getBottomPanel() override;

private:
	PluginComponent* pluginComponent;
	std::unique_ptr<controls::TopPanel> topPanel;
	std::unique_ptr<controls::BottomPanel> bottomPanel;

	friend class DarkGlassTopPanel;
	friend class DarkGlassBottomPanel;
};

class DarkGlassTopPanel : public PluginControlPanel,
	public ButtonGroup::Listener
{
public:
	DarkGlassTopPanel(DarkGlassControl* control);
	~DarkGlassTopPanel();

	void resized() override;
	void onButtonClicked(ButtonGroup* buttonGroup, int index, bool enabled) override;

private:
	DarkGlassControl* control;
	std::unique_ptr<ExclusiveButtonGroup> ampType = nullptr;
	std::unique_ptr<ExclusiveButtonGroup> attack = nullptr;
	std::unique_ptr<ExclusiveButtonGroup> grunt = nullptr;
	std::unique_ptr<ExclusiveButtonGroup> lowMid = nullptr;
	std::unique_ptr<ExclusiveButtonGroup> highMid = nullptr;
	std::unique_ptr<SliderPanel> audioSliderPanel = nullptr;
};

class DarkGlassBottomPanel : public PluginControlPanel {
public:
	DarkGlassBottomPanel(DarkGlassControl* control);
	~DarkGlassBottomPanel();

	void resized() override;

	std::unique_ptr<SliderPanelGroup> amplSliderPanels = nullptr;

private:
	DarkGlassControl* control;
	std::unique_ptr<SliderPanel> sliderPanel = nullptr;
};



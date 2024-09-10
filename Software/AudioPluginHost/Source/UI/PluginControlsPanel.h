#pragma once

#include <JuceHeader.h>
#include "PluginComponent.h"
#include "GraphEditorPanel.h"
#include "PluginControls/PluginControl.h"

class PluginControlsPanel : public Component,
							public GraphEditorPanel::Listener,
							public juce::MidiInputCallback
{
public:
	PluginControlsPanel(GraphEditorPanel* p, bool isTopPanel);
	~PluginControlsPanel() {};

	void resized() override;
	void paint(Graphics& g) override;

	void onActiveComponentChange(PluginComponent* activeComponent) override;
	void onNodeDeleted(PluginComponent* removedComponent) override;
	void handleIncomingMidiMessage(MidiInput* source, const MidiMessage& message) override;


private:
	void setMidiInput();

	bool isTopPanel = false;
	PluginComponent* activePlugin = nullptr;
	controls::TopPanel* topPanel = nullptr;
	controls::BottomPanel* bottomPanel = nullptr;
	juce::AudioDeviceManager deviceManager;
};
#pragma once

#include <JuceHeader.h>
#include "PluginControl.h"
#include "Components.h"
#include "../pugixml-1.14/src/pugixml.hpp"

class ControlXmlParser {
public:
	ControlXmlParser(std::map<String, AudioProcessorParameter*> paramsMap);
	~ControlXmlParser();

	PluginControl* parse(std::string filename);

	static std::unordered_map<std::string, std::string> listPluginControlsInDirectory(std::string directory);

private:
	controls::Component* parseNode(pugi::xml_node xmlNode);

	PluginControl* parsePluginControl(pugi::xml_node node);
	controls::TopPanel* parseTopPanel(pugi::xml_node node);
	controls::BottomPanel* parseBottomPanel(pugi::xml_node node);
	controls::Button* parseButton(pugi::xml_node node);
	controls::Slider* parseSlider(pugi::xml_node node);
	controls::ButtonGroup* parseButtonGroup(pugi::xml_node node);
	controls::SelectorButtons* parseSelectorButtons(pugi::xml_node node);
	controls::Panel* parsePanel(pugi::xml_node node);
	controls::PanelStack* parsePanelStack(pugi::xml_node node);
	void resolveBindings();

	std::map<String, AudioProcessorParameter*> paramsMap;
	std::vector<controls::Component*> components;
	std::map<std::string, controls::Component*> componentsMap;
};
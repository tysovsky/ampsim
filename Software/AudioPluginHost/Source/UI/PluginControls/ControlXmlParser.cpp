#include "ControlXmlParser.h"
#include <filesystem>

std::unordered_map<std::string, std::string> ControlXmlParser::listPluginControlsInDirectory(std::string directory)
{
	std::unordered_map<std::string, std::string> result;

	for (const auto& entry : std::filesystem::directory_iterator(directory)) {
		auto path = entry.path().string();

		pugi::xml_document doc;
		pugi::xml_parse_result xmlParseResult = doc.load_file(path.c_str());
		if (!xmlParseResult)
			return result;

		auto pluginControl = doc.child("PluginControl");
		if (!pluginControl)
			return result;

		auto pluginName = std::string(pluginControl.attribute("name").as_string());

		if (pluginName != "") {
			result[pluginName] = path;
		}
	}

	return result;
}

ControlXmlParser::ControlXmlParser(std::map<String, AudioProcessorParameter*> paramsMap) : paramsMap(paramsMap)
{

}

ControlXmlParser::~ControlXmlParser() 
{

}

PluginControl* ControlXmlParser::parse(std::string filename) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filename.c_str());
	if (!result)
		return nullptr;

	auto pluginControl = doc.child("PluginControl");
	if (!pluginControl)
		return nullptr;

	componentsMap.clear();
	components.clear();

	auto control =  parsePluginControl(pluginControl);

	resolveBindings();

	componentsMap.clear();
	components.clear();

	return control;
}

controls::Component* ControlXmlParser::parseNode(pugi::xml_node xmlNode) {
	auto name = std::string(xmlNode.name());

	controls::Component* component = nullptr;

	if (name == "Button") {
		component = parseButton(xmlNode);
	}
	else if (name == "Slider") {
		component = parseSlider(xmlNode);
	}
	else if (name == "ButtonGroup") {
		component = parseButtonGroup(xmlNode);
	}
	else if (name == "SelectorButtons") {
		component = parseSelectorButtons(xmlNode);
	}
	else if (name == "Panel") {
		component = parsePanel(xmlNode);
	}
	else if (name == "PanelStack") {
		component = parsePanelStack(xmlNode);
	}

	// Attributes common to all nodes
	if (component != nullptr) {
		component->setWidth(xmlNode.attribute("width").as_string());

		auto id = std::string(xmlNode.attribute("id").as_string());
		component->setId(id);

		if (id != "") {
			componentsMap[id] = component;
		}

		components.push_back(component);

		component->init();
	}

	return component;
}

PluginControl* ControlXmlParser::parsePluginControl(pugi::xml_node node) {
	auto topPanelNode = node.child("TopPanel");
	if (!topPanelNode)
		return nullptr;

	auto bottomPanelNode = node.child("BottomPanel");
	if (!bottomPanelNode)
		return nullptr;

	auto topPanel = parseTopPanel(topPanelNode);
	auto bottomPanel = parseBottomPanel(bottomPanelNode);

	auto control = new PluginControl();
	control->topPanel = topPanel;
	control->bottomPanel = bottomPanel;

	return control;
}

controls::BottomPanel* ControlXmlParser::parseBottomPanel(pugi::xml_node node) {
	auto bottomPanel = new controls::BottomPanel();

	for (auto childNode : node.children()) {
		bottomPanel->addChild(parseNode(childNode));
	}

	return bottomPanel;
}

controls::TopPanel* ControlXmlParser::parseTopPanel(pugi::xml_node node) {
	auto topPanel = new controls::TopPanel();

	for (auto childNode : node.children()) {
		auto childControl = parseNode(childNode);

		if (childControl) {
			topPanel->addChild(childControl);
		}
	}

	return topPanel;
}

controls::Button * ControlXmlParser::parseButton(pugi::xml_node node) {
	auto type = std::string(node.name());
	if (type != "Button")
		return nullptr;

	auto name = node.attribute("name").as_string();
	auto button = new controls::Button(name);

	auto paramName = std::string(node.attribute("param").as_string());
	if (paramName != "") {
		auto param = paramsMap[paramName];
		if (param) {
			button->setParam(param);
		}
	}

	auto toggle = node.attribute("toggle").as_int(-1);
	if (toggle != -1) {
		button->setToggle(toggle);
	}

	return button;
}

controls::Slider* ControlXmlParser::parseSlider(pugi::xml_node node) {
	auto type = std::string(node.name());
	if (type != "Slider")
		return nullptr;

	auto paramName = std::string(node.attribute("param").as_string());
	if (paramName == "")
		return nullptr;

	auto param = paramsMap[paramName];
	if (!param) {
		return nullptr;
	}

	auto slider = new controls::Slider(param);

	auto label = std::string(node.attribute("label").as_string());
	if (label != "")
		slider->setLabel(label);

	auto knob = node.attribute("knob").as_int(-1);
	if (knob != -1) {
		slider->setKnob(knob);
	}

	return slider;
}

controls::ButtonGroup* ControlXmlParser::parseButtonGroup(pugi::xml_node node) {
	auto type = std::string(node.name());
	if (type != "ButtonGroup")
		return nullptr;

	auto buttonGroup = new controls::ButtonGroup();

	auto label = node.attribute("label").as_string();
	buttonGroup->setLabel(label);

	for (auto buttonNode : node.children()) {
		auto btn = parseNode(buttonNode);
		if (!btn) {
			return nullptr;
		}

		buttonGroup->addChild(btn);
	}


	return buttonGroup;
}

controls::SelectorButtons* ControlXmlParser::parseSelectorButtons(pugi::xml_node node) {
	auto type = std::string(node.name());
	if (type != "SelectorButtons")
		return nullptr;

	auto paramName = std::string(node.attribute("param").as_string());
	if (paramName == "")
		return nullptr;

	auto param = paramsMap[paramName];
	if (!param) {
		return nullptr;
	}

	auto selectorButton = new controls::SelectorButtons(param);

	auto label = node.attribute("label").as_string();
	selectorButton->setLabel(label);

	for (auto buttonNode : node.children()) {
		auto btn = parseNode(buttonNode);
		if (!btn) {
			return nullptr;
		}

		selectorButton->addChild(btn);
	}

	return selectorButton;
}

controls::Panel* ControlXmlParser::parsePanel (pugi::xml_node node) {
	auto type = std::string(node.name());
	if (type != "Panel")
		return nullptr;


	auto panel = new controls::Panel();

	for (auto childNode : node.children()) {
		auto childControl = parseNode(childNode);
		if (!childControl) {
			return nullptr;
		}

		panel->addChild(childControl);
	}

	return panel;
}

controls::PanelStack* ControlXmlParser::parsePanelStack(pugi::xml_node node) {
	auto type = std::string(node.name());
	if (type != "PanelStack")
		return nullptr;


	auto panelStack = new controls::PanelStack();

	for (auto childNode : node.children()) {
		auto childControl = parseNode(childNode);
		if (!childControl) {
			return nullptr;
		}

		panelStack->addChild(childControl);
	}

	auto bindTo = node.attribute("bind_to").as_string();
	panelStack->setBindToId(bindTo);

	return panelStack;
}

void ControlXmlParser::resolveBindings() {
	for (auto c : components) {
		auto bindToId = c->getBindToId().toStdString();

		if (bindToId != "" && componentsMap[bindToId] != nullptr) {

			componentsMap[bindToId]->bind(c);
		}
	}

	for (const auto& [key, value] : componentsMap) {

	}
}
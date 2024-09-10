#pragma once

#include <JuceHeader.h>

namespace controls {
	const int LABEL_HEIGHT = 15;


	class Component : public juce::Component {
	public:
		 virtual ~Component() {
			for (auto child : children) {
				delete child;
			}
		}

		void setId(String id) {
			this->id = id;
		}

		void setWidth(String width) {
			this->width = width;
		}

		void addChild(Component* child) {
			this->children.push_back(child);
			addAndMakeVisible(child);
		}

		virtual void resized() override {
			auto localBounds = getLocalBounds();

			auto label = getLabelComponent();
			if (label) {
				label->setBounds(localBounds.removeFromTop(LABEL_HEIGHT));
			}

			int totalPadding = 0;
			for (auto c : getChildren()) {
				totalPadding += c->getPadding();
			}
			int parentWidth = localBounds.getWidth() - totalPadding;

			int totalDesiredWidth = 0;
			int numChildrenWithoutSpecifiedWidth = 0;
			for (auto child : getChildren()) {
				auto desiredWidth = child->getDesiredWidth(parentWidth);
				totalDesiredWidth += desiredWidth;

				if (desiredWidth == 0) {
					numChildrenWithoutSpecifiedWidth++;
				}
			}

			int leftOverWidh = parentWidth - totalDesiredWidth;
			int remainingSiblingWidth = 0;
			if (numChildrenWithoutSpecifiedWidth > 0) {
				remainingSiblingWidth = leftOverWidh / numChildrenWithoutSpecifiedWidth;
			}
			

			for (auto child : getChildren()) {
				localBounds.removeFromLeft(child->getPadding());
				auto desiredWidth = child->getDesiredWidth(parentWidth);
				if (desiredWidth == 0) {
					child->setBounds(localBounds.removeFromLeft(remainingSiblingWidth));
				}
				else {
					child->setBounds(localBounds.removeFromLeft(child->getDesiredWidth(parentWidth)));
				}
			}
		}

		virtual juce::Label* getLabelComponent() {
			return nullptr;
		}

		// Get desired witdth, in pixels
		int getDesiredWidth(int parentWidth) {
			// If width is not specified, compute desired width based on the childre's desired width
			if (width == "") {
				int childrenDesiredWidth = 0;

				for (auto child : getChildren()) {
					childrenDesiredWidth += child->getDesiredWidth(parentWidth);
				}

				return childrenDesiredWidth;
			}

			if (width[width.length() - 1] == '%') {
				auto percentString = width.substring(0, width.length() - 1).toStdString();
				auto percent = std::atoi(percentString.c_str());
				return parentWidth * (percent / 100.0);
			}

			if (width.substring(width.length() - 2, width.length()) == "px")
			{
				return std::atoi(width.substring(0, width.length() - 2).toRawUTF8());
			}

			return 0;
		}

		void setPadding(int padding) {
			this->padding = padding;
		}

		int getPadding() {
			return padding;
		}

		virtual void init() { }

		virtual std::vector<Component*> getChildren() {
			return children;
		}

		virtual void bind(Component* component) { }
		
		void setBindToId(String bindToId)
		{
			this->bindToId = bindToId;
		}

		String getBindToId()
		{
			return bindToId;
		}

		virtual void knobValueChanged(int knobIndex, int value) {
			for (auto c : getChildren()) {
				c->knobValueChanged(knobIndex, value);
			}
		}

		virtual void toggleValueChanged(int toggleIndex, bool enabled) {
			for (auto c : getChildren()) {
				c->toggleValueChanged(toggleIndex, enabled);
			}
		}

		virtual void setValue(int value) {}

	protected:
		String id = "";
		String width = "";
		std::vector<Component*> children;
		int padding = 10;
		String bindToId;
	};

	class IncomingMessageCallback : public juce::CallbackMessage
	{
	public:
		IncomingMessageCallback(Component* o, int value)
			: owner(o), value(value)
		{}

		void messageCallback() override
		{
			if (owner != nullptr)
				owner->setValue(value);
		}

		juce::Component::SafePointer<Component> owner;
		int value = 0;
	};

	class LabeledComponent : public Component {
	public:
		void setLabel(String label) {
			this->label = label;
		}
	protected:
		String label = "";
	};

	class Button : public Component
	{
	public:
		Button(String name) : name(name) {
			button = std::make_unique<juce::TextButton>();
			button->setButtonText(name);
			addAndMakeVisible(button.get());

			button->onClick = [this]() {
				if (disableListener)
					return;

				bool toggled = !button->getToggleState();
				setToggleState(toggled);

				if (param) {
					if (toggled) {
						param->setValue(1.0);
					}
					else
					{
						param->setValue(0.0);
					}
				}
			};
		}
		~Button() {}

		void setParam(AudioProcessorParameter* param) {
			this->param = param;

			if (param->getValue() > 0.5) {
				setToggleState(true);
			}
		}

		void resized() override {
			auto bounds = getLocalBounds();
			button->setBounds(bounds);
		}

		void setToggleState(bool toggled) {
			button->setToggleState(toggled, NotificationType::dontSendNotification);
		}

		juce::Button* getButton() 
		{
			return button.get();
		}

		void setDisableListener(bool disableListener) {
			this->disableListener = disableListener;
		}

		void setToggle(int toggle) {
			this->toggle = toggle;
		}

		void toggleValueChanged(int toggleIndex, bool enabled) override {
			if (toggleIndex == toggle) {
				(new IncomingMessageCallback(this, enabled))->post();
			}
		}

		void setValue(int value) override {
			button->setToggleState(value, juce::NotificationType::dontSendNotification);
			param->setValue(value);
		}


	private:
		String name = "";
		AudioProcessorParameter* param = nullptr;
		std::unique_ptr<juce::TextButton> button = nullptr;
		bool disableListener = false;
		int toggle = -1;
	};

	class Slider : public LabeledComponent {
	public:
		Slider(AudioProcessorParameter* param) : param(param)
		{
			slider = std::make_unique<juce::Slider>();
			slider->setName(param->getName(20));
			slider->setRange(Range<double>(0, 100), 0);
			slider->setValue(param->getValue() * slider->getRange().getEnd());
			slider->setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);

			slider->onValueChange = [this, param]() {
				param->setValue(slider->getValue() / slider->getRange().getEnd());
			};

			labelComponent = std::make_unique<juce::Label>();
			labelComponent->setText(param->getName(20), juce::NotificationType::dontSendNotification);
			labelComponent->setJustificationType(Justification::centred);


			addAndMakeVisible(slider.get());
			addAndMakeVisible(labelComponent.get());
		}

		~Slider() {}

		void resized() override {
			auto bounds = getLocalBounds();

			auto labelBounds = bounds.removeFromTop(15);

			labelComponent->setBounds(labelBounds);
			slider->setBounds(bounds);
		}

		void setKnob(int knob) {
			this->knob = knob;
		}

		void setLabel(String label) {
			this->label = label;
			labelComponent->setText(label, juce::NotificationType::dontSendNotification);
		}

		void knobValueChanged(int knobIndex, int value) override {
			if (knobIndex == knob) {
				(new IncomingMessageCallback(this, value))->post();
			}
		}

		void setValue(int value) override {
			slider->setValue(value, juce::NotificationType::sendNotificationAsync);
		}

	private:
		AudioProcessorParameter* param = nullptr;
		std::unique_ptr<juce::Slider> slider = nullptr;
		std::unique_ptr<juce::Label> labelComponent = nullptr;
		int knob = -1;
	};

	class ButtonGroup : public LabeledComponent {
	public:
		ButtonGroup() {}
		~ButtonGroup() {}

		void setLabel(String label) {
			labelComponent = std::make_unique<juce::Label>();
			labelComponent->setText(label, juce::NotificationType::dontSendNotification);
			labelComponent->setJustificationType(Justification::centred);
			addAndMakeVisible(labelComponent.get());
		}

		juce::Label* getLabelComponent() override {
			return labelComponent.get();
		}

		void init() override {
			for (auto c : children) {
				c->setPadding(0);
			}
		}
	
	private:
		std::unique_ptr<juce::Label> labelComponent = nullptr;
	};

	class Panel : public Component {
	};

	class PanelStack : public Component {
	public:
		std::vector<Component*> getChildren() override {
			if (children.size() > activePanel) {
				return std::vector<Component*>{children[activePanel]};
			}
			return children;
		}

		void setActivePanel(int activePanel) {
			removeAllChildren();
			this->activePanel = activePanel;

			if (activePanel < children.size()) {
				addAndMakeVisible(children[activePanel]);
			}

			resized();
		}
	private:
		int activePanel = 0;
	};

	class SelectorButtons : public LabeledComponent,
		public juce::Button::Listener
	{
	public:
		SelectorButtons(AudioProcessorParameter* param) : param(param) {}
		~SelectorButtons() {}

		void setLabel(String label) {
			labelComponent = std::make_unique<juce::Label>();
			labelComponent->setText(label, juce::NotificationType::dontSendNotification);
			labelComponent->setJustificationType(Justification::centred);
			addAndMakeVisible(labelComponent.get());
		}

		juce::Label* getLabelComponent() override {
			return labelComponent.get();
		}

		void init() override {
			int activeIndex = getCurrentIndex();

			auto activeButton = dynamic_cast<Button*>(children[activeIndex]);
			if (activeButton) {
				activeButton->setToggleState(true);
			}

			for (auto paneStack : boundPanelStacks) {
				paneStack->setActivePanel(activeIndex);
			}

			for (auto c : children) {
				auto btn = dynamic_cast<Button*>(c);

				btn->setPadding(0);

				btn->setDisableListener(true);
				btn->getButton()->addListener(this);
			}
		}

		void buttonClicked(juce::Button* clickedBtn) override {
			for (int i = 0; i < children.size(); i++) {
				auto btn = dynamic_cast<Button*>(children[i]);

				if (btn->getButton() == clickedBtn) {
					btn->setToggleState(true);

					auto value = float(i) / (children.size() - 1);
					param->setValue(value);

					for (auto paneStack : boundPanelStacks) {
						paneStack->setActivePanel(i);
					}
				}
				else {
					btn->setToggleState(false);
				}
			}
		}

		void bind(Component* component) override { 
			auto panelStack = dynamic_cast<PanelStack*>(component);

			if (panelStack) {
				boundPanelStacks.push_back(panelStack);
				panelStack->setActivePanel(getCurrentIndex());
			}
		}

	private:
		int getCurrentIndex() {
			if (!param) {
				return 0;
			}

			auto value = param->getValue();
			return int(value * (children.size() - 1));
		}

		AudioProcessorParameter* param = nullptr;
		std::unique_ptr<juce::Label> labelComponent = nullptr;
		std::vector<PanelStack*> boundPanelStacks;

	};

	class TopPanel : public Component {
	public: 
		TopPanel() {}
	};

	class BottomPanel : public Component {

	};
}
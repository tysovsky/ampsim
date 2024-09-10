#include <ResponsiveAnalogRead.h>
#include <ButtonDebounce.h>

#include "MIDIUSB.h"

const int NUM_KNOBS = 7;

ResponsiveAnalogRead knobOne(1, true);
ResponsiveAnalogRead knobTwo(5, true);
ResponsiveAnalogRead knobThree(6, true);
ResponsiveAnalogRead knobFour(7, true);
ResponsiveAnalogRead knobFive(8, true);
ResponsiveAnalogRead knobSix(9, true);
ResponsiveAnalogRead knobSeven(10, true);


ResponsiveAnalogRead knobs[NUM_KNOBS] = {
  knobOne,
  knobTwo,
  knobThree,
  knobFour,
  knobFive,
  knobSix,
  knobSeven,
};

int lastSwitchValue = LOW;
ButtonDebounce ampSwitch(2, 250);

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}

void noteOn(byte channel, byte note, byte value) {
  midiEventPacket_t event = {0x90, 0x90 | channel, note, value};
  MidiUSB.sendMIDI(event);
}

void noteOff(byte channel, byte note, byte value) {
  midiEventPacket_t event = {0x80, 0x80 | channel, note, value};
  MidiUSB.sendMIDI(event);
}

void midiCommand(byte cmd, byte data1, byte  data2) {
  // First parameter is the event type (top 4 bits of the command byte).
  // Second parameter is command byte combined with the channel.
  // Third parameter is the first data byte
  // Fourth parameter second data byte

  midiEventPacket_t midiMsg = {cmd >> 4, cmd, data1, data2};
  MidiUSB.sendMIDI(midiMsg);
}

void setup() {
  ampSwitch.update();
  lastSwitchValue = ampSwitch.state();
}

void loop() {
  for(int i = 0; i < NUM_KNOBS; i++) {
    knobs[i].update();
    
    if(knobs[i].hasChanged()){
      byte midiValue = 126 - map(knobs[i].getValue(), 0, 1025, 0, 127);
      controlChange(2, 10+i, midiValue);
    }
  }

  handle_switch();

  delay(10);
}

void handle_switch() {
  ampSwitch.update();
  if (ampSwitch.state() != lastSwitchValue) {
    lastSwitchValue = ampSwitch.state();
    if (ampSwitch.state() == HIGH) {
      midiCommand(0x90, 64, 127);
    } else {
      midiCommand(0x80, 64, 0);
    }
  }
}
/* !!! ATTENTION !!!
The code works with library versions:
NimBLE-Arduino 1.4.3 and ESP32-BLE-MIDI 0.3.2
  -----------------------------------------------------------------------------------------------
  HARDWARE:
  ESP32-S3
  Breath sensor MPXV7002DP
  Gyroscope MPU-6050
  Display Oled 128x32 I2c

  Author: Michal KARAS && Github Copilot
  Created: 5/12/2025
  For more information:
  https://karas-midi-woodwinds.webnode.cz
*/

#include <Arduino.h>
#include <BLEMidi.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>


// Touch pins
int threshold = 0;  // if 0 is used, benchmark value is used. Its by default 1,5% change, can be changed by touchSetDefaultThreshold(float percentage)


// --- ADDED: baroque fingering, breath trigger, range switching, aux duplicate ---
// Pins (adjust if different)
const int INSTR_UP_PIN = 42;
const int INSTR_DOWN_PIN = 41;
const int RANGE_UP_PIN = 40;
const int RANGE_DOWN_PIN = 39;
const int BREATH_PIN = 17;      // analog breath input (same as [UNI_SAX_1_2.ino](UNI_SAX_1_2.ino) uses BREATH_PIN=4)
const int AUX1_PIN    = 14;    // aux1 momentary button (same idea as [`aux1Pressed`](UNI_SAX_1_2.ino))
const uint8_t BREATH_THRESHOLD = 24; // threshold on 0..127 scale to consider "breathON"

// Breath
int offset = 2165;

// Range selector
enum RangeMode { SOPRANO_C = 0, ALTO_F = 1, TENOR_C = 2 };
RangeMode rangeMode = SOPRANO_C;

// State
bool breathON = false;
bool lastbreathON = false;
int currentNote = -1;       // currently sounding main note (for noteOff)
int currentNoteAux = -1;    // currently sounding aux note

// Fingering map entry: exact mask -> pitches for each range
struct FingerMapEntry {
  uint16_t mask;   // bitmask of touch sensors (user must encode mapping)
  int pitch[3];    // pitch[0]=Soprano_C, [1]=Alto_F, [2]=Tenor_C (MIDI note numbers)
};

// Example entries: you must replace masks and pitches with your real baroque fingering table.
// mask bits: bit0 -> T1, bit1 -> T2, bit2 -> T3, ... (choose fixed order below)
static const FingerMapEntry FINGERINGS[] = {
  // { mask, {sopranoPitch, altoPitch, tenorPitch} }
  // Fill these rows with your fingering table. Mask is an integer encoding closed(1)/open(0) buttons.

  // mask          {0-soprano-C, 1-alt-F, 2-tenor-C}
  { 0b11111111111, {60, 53, 48} }, // C4, F3, C3
  { 0b11111111110, {61, 54, 49} }, // C#4, F#3, C#3
  { 0b11111111100, {62, 55, 50} }, // D4, G3, D3
  { 0b11111111000, {63, 56, 51} }, // D#4, G#3, D#3
  { 0b11111110000, {64, 57, 52} }, // E4, A3, E3
  { 0b11111101111, {65, 58, 53} }, // F4, A#3, F3
  { 0b11111011100, {66, 59, 54} }, // F#4, B3, F#3
  { 0b11110000000, {67, 60, 55} }, // G4, B3, G3
  { 0b11110111000, {68, 61, 56} }, // G#4, C4, G#3
  { 0b11110000000, {69, 62, 57} }, // A4, C#4, A3
  { 0b11101100000, {70, 63, 58} }, // A#4, D4, A#3  
  { 0b11100000000, {71, 64, 59} }, // B4, D#4, B3
  { 0b11010000000, {72, 65, 60} }, // C5, E4, C4
  { 0b00110000000, {73, 66, 61} }, // C#5, F4, C#4
  { 0b00001000000, {74, 67, 62} }, // D5, F#4, D4
  { 0b00011111100, {75, 68, 63} }, // D#5, G4, D#4
  { 0b01111110000, {76, 69, 64} }, // E5, G#4, E4
  { 0b01111101100, {77, 70, 65} }, // F5, A4, F4
  { 0b01111010000, {78, 71, 66} }, // F#5, A#4, F#4
  { 0b01111000000, {79, 72, 67} }, // G5, B4, G4
  { 0b01110100000, {80, 73, 68} }, // G#5, C5, G#4
  { 0b01110000000, {81, 74, 69} }, // A5, C#5, A4
  { 0b01110011110, {82, 75, 70} }, // A#5, D5, A#4
  { 0b01110110000, {83, 76, 71} }, // B5, D#5, B4
  { 0b01100110000, {84, 77, 72} }, // C6, E5, C5
  { 0b01101101111, {85, 78, 73} }, // C#6, F5, C#5
  { 0b11101101111, {86, 79, 74} }, // D6, F#5, D5  0b1... INSTEAD OFF BELL CLOSED
  { 0b01011010110, {87, 80, 75} }, // D#6, G5, D#5
  { 0b01011111111, {88, 81, 76} }, // E6, G#5, E5 
};

static const size_t FINGERING_COUNT = sizeof(FINGERINGS) / sizeof(FINGERINGS[0]);

// Map touch pins -> mask bit positions (update order to match how you interpret the fingering table)
const int TOUCH_ORDER[] = { T1, T2, T4, T5, T6, T7, T3, T11, T10, T12, T13 }; // 11 sensors
const int TOUCH_COUNT = sizeof(TOUCH_ORDER) / sizeof(TOUCH_ORDER[0]);

// Helper: read current combination as mask (1 = closed/touched, 0 = open)
uint16_t readFingeringMask() {
  uint16_t mask = 0;
  for (int i = 0; i < TOUCH_COUNT; ++i) {
    bool touched = touchInterruptGetLastStatus(TOUCH_ORDER[i]); // existing API used in file
    if (touched) mask |= (1u << i);
  }
  return mask;
}

// Lookup pitch from table for current range; returns -1 if no mapping
int lookupPitchForMask(uint16_t mask, RangeMode r) {
  for (size_t i = 0; i < FINGERING_COUNT; ++i) {
    if (FINGERINGS[i].mask == mask) {
      return FINGERINGS[i].pitch[(int)r];
    }
  }
  return -1;
}

// Convert raw analog breath reading to 0..127
int breathToMidi(int raw) {
  // On ESP32 ADC range may be 0..4095; adjust if necessary
  int v = map(constrain(raw - offset, 0, 1935), 0, 1023, 0, 127);
  v = constrain(v, 0, 124);
  return v;
}

// Call this when starting a note
void noteOn(int channel, int pitch, int velocity) {
  Serial.printf("noteOn ch=%d pitch=%d vel=%d\n", channel, pitch, velocity);
  // TODO: call your MIDI send, e.g. BLEMidiServer.noteOn(channel, pitch, velocity);
  BLEMidiServer.noteOn(channel, pitch, velocity);
}

void noteOff(int channel, int pitch, int velocity) {
  Serial.printf("BLEMidiServer.noteOff ch=%d pitch=%d vel=%d\n", channel, pitch, velocity);
  // TODO: call your MIDI send, e.g. BLEMidiServer.noteOff(channel, pitch, velocity);
 BLEMidiServer.noteOff(channel, pitch, velocity);
}

static int range_index = 0;
static int instrument_index = 0;

//-----------------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(1000);  // give me time to bring up serial monitor
  Serial.println("Initializing bluetooth");
  BLEMidiServer.begin("ButtonsAI");
  Serial.println("Waiting for connections...");


  pinMode(INSTR_UP_PIN, INPUT_PULLUP);
  pinMode(INSTR_DOWN_PIN, INPUT_PULLUP);
  pinMode(RANGE_UP_PIN, INPUT_PULLUP);
  pinMode(RANGE_DOWN_PIN, INPUT_PULLUP);
  pinMode(AUX1_PIN, INPUT_PULLUP); // aux1 pressed = LOW
  pinMode(BREATH_PIN, INPUT);

  //Optional: Set the threshold to 5% of the benchmark value. Only effective if threshold = 0.
  touchSetDefaultThreshold(5);

}

//-----------------------------------------------------------------------------------

void loop() {
  // read breath and decide breathON
  int rawBreath = analogRead(BREATH_PIN);
  int breathVal = breathToMidi(rawBreath);
  breathON = (breathVal >= BREATH_THRESHOLD);

  // Aux1 state (momentary)
  bool aux1Pressed = !digitalRead(AUX1_PIN);

  // range switch via serial simple commands (optional)
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "r0") rangeMode = SOPRANO_C;
    else if (cmd == "r1") rangeMode = ALTO_F;
    else if (cmd == "r2") rangeMode = TENOR_C;
    Serial.printf("Range set to %d\n", (int)rangeMode);
  }

  // On breath start -> determine fingering & play note
  if (breathON && !lastbreathON) {
    uint16_t mask = readFingeringMask();
    int pitch = lookupPitchForMask(mask, rangeMode);
    if (pitch >= 0) {
      // main note on (channel 0)
      BLEMidiServer.noteOn(0, pitch, breathVal); // velocity = breathVal (0..127)
      currentNote = pitch;

      // aux duplicate: if aux1 held at attack, also play on channel 1
      if (aux1Pressed) {
        BLEMidiServer.noteOn(1, pitch, breathVal);
        currentNoteAux = pitch;
      } else {
        currentNoteAux = -1;
      }
    } else {
      Serial.printf("No fingering mapping for mask 0x%03X\n", mask);
      currentNote = -1;
      currentNoteAux = -1;
    }
  }

  // Sustain until breath off: on breath release -> send noteOffs
  if (!breathON && lastbreathON) {
    if (currentNote >= 0) {
      BLEMidiServer.noteOff(0, currentNote, 127);
      currentNote = -1;
    }
    if (currentNoteAux >= 0) {
      BLEMidiServer.noteOff(1, currentNoteAux, 127);
      currentNoteAux = -1;
    }
  }

  // If breathON stays true but user toggles AUX1 while holding:
  // keep auxiliary note in sync (optional: you can implement retrigger policy)
  if (breathON && currentNote >= 0) {
    if (aux1Pressed && currentNoteAux != currentNote) {
      // start aux note
      BLEMidiServer.noteOn(1, currentNote, breathVal);
      currentNoteAux = currentNote;
    } else if (!aux1Pressed && currentNoteAux >= 0) {
      // stop aux note
      BLEMidiServer.noteOff(1, currentNoteAux, 127);
      currentNoteAux = -1;
    }
  }

  lastbreathON = breathON;
  delay(20); // adjust scan rate

}

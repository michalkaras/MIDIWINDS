#include <Arduino.h>
#include <BLEMidi.h>
#include "NimBLEDevice.h"

#include <ss_oled.h>
#include "GY521.h"

// ACCELEROMETER
GY521 sensor(0x68);

//DISPLAY
#define SDA_PIN 8
#define SCL_PIN 9
#define RESET_PIN -1
#define OLED_ADDR -1  // don't rotate the display
#define FLIP180 1
#define INVERT 0      // don't invert the display
#define USE_HW_I2C 0  // Bit-Bang the I2C bus
// Change these if you're using a different OLED display
#define MY_OLED OLED_128x32
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
SSOLED ssoled;

// KEY SELECTOR            C   D   E   F  G   A   B   C#  D#  F#  G#  A#
const int notePins[12] = { 1, 42, 40,  7, 16, 18, 11, 2,  41, 15, 17, 10 };
const int SCALE[12][12] = {
  //  C   D   E   F  G   A   B   C#  D#  F#  G#  A#       KEY selection of Ionian scale
  { 48, 50, 52, 53, 55, 57, 59, 49, 51, 54, 56, 58 },
  { 48, 49, 51, 53, 54, 56, 58, 47, 50, 52, 55, 57 },
  { 49, 50, 52, 54, 55, 57, 59, 48, 51, 53, 56, 58 },
  { 48, 50, 51, 53, 55, 56, 58, 49, 52, 54, 57, 59 },
  { 49, 51, 52, 54, 56, 57, 59, 48, 50, 53, 55, 58 },
  { 48, 50, 52, 53, 55, 57, 58, 49, 51, 54, 56, 59 },
  { 49, 51, 53, 54, 56, 58, 59, 48, 50, 52, 55, 57 },
  { 48, 50, 52, 54, 55, 57, 59, 49, 51, 53, 56, 58 },
  { 48, 49, 51, 53, 55, 56, 58, 47, 50, 52, 54, 57 },
  { 49, 50, 52, 54, 56, 57, 59, 48, 51, 53, 55, 58 },
  { 48, 50, 51, 53, 55, 57, 58, 47, 49, 52, 54, 57 },
  { 49, 51, 52, 54, 56, 58, 59, 48, 50, 53, 55, 57 }
};

// PINS
const uint8_t MOUTH_PIN = 5;
const uint8_t BREATH_PIN = 4;
const uint8_t AUX1_PIN = 12;
const uint8_t AUX2_PIN = 13;
const uint8_t OCTAVE_SHIFT_PIN = 20;
const int SCALE_UP_PIN = 37;
const int SCALE_DOWN_PIN = 39;
const uint8_t PLAY_PIN = 14;
const int INSTR_UP_PIN = 35;
const int INSTR_DOWN_PIN = 36;
const int OCTAVE_UP_PIN = 47;
const int OCTAVE_DOWN_PIN = 21;

//const int MODULATION_PIN = 4;
//const int BRASS_PIN = 40;
//const int STRINGS_PIN = 39;

static int scale_index = 0;
static int instrument_index = 0;

int PIN_pitch_state[12] = {};
int lastPINstate[12] = {};
int currentMillis = millis();
int lastPINchangetime[12] = { currentMillis };
const int octave = 12;
const int octaveShift = 12;

// State variables
bool mouthTouched = false;    // Track if the mouth pin is currently touched
bool aux1Pressed = false;     // Momentary button
bool lastMouthstate = false;  // Last state of mouth pin
bool lastAux1state = false;   // Last state of AUX1 pin

int threshold = 600;  // Adjust this threshold as needed
bool touchdetected = false;
// Touch detect
void gotTouch() {
  touchdetected = false;
}

// BREATH
int offset = 2192;

// AVERAGE
const int numReadings = 0;
int readings[numReadings];  //the readings from the analog input

//----------------------------------------------------------------
void setup(void) {
  Serial.begin(115200);
  Serial.println("Initializing bluetooth");
  BLEMidiServer.begin("JOY_0_GY521");
  Serial.println("Waiting for connections...");
  
 // Initialize accelerometer (GY521)
  Wire.begin();
  delay(100);
  while (sensor.wakeup() == false) {
    Serial.print(millis());
    Serial.println("\tCould not connect to GY521: please check the GY521 address (0x68/0x69)");
    delay(1000);
  }
  sensor.setAccelSensitivity(2);  // 8g
  sensor.setGyroSensitivity(1);   // 500 degrees/s

  // Set calibration values
  sensor.axe = 0.574;
  sensor.aye = -0.002;
  sensor.aze = -1.043;
  sensor.gxe = 10.702;
  sensor.gye = -6.436;
  sensor.gze = -0.676;
   
  // Initialize touch interrupt
  touchAttachInterrupt(T5, gotTouch, threshold);

  // DISPLAY
  int rc;
  rc = oledInit(&ssoled, MY_OLED, OLED_ADDR, FLIP180, INVERT, USE_HW_I2C, SDA_PIN, SCL_PIN, RESET_PIN, 400000L); // use standard I2C bus at 400Khz
  if (rc != OLED_NOT_FOUND)

  // NOTE PINS
  for (int i = 0; i < 12; i++) {
    pinMode(notePins[i], INPUT_PULLUP);
  }

  // CC PINS
  pinMode(AUX1_PIN, INPUT_PULLUP);          // Pin 42
  pinMode(AUX2_PIN, INPUT_PULLUP);          // Pin 41
  pinMode(OCTAVE_SHIFT_PIN, INPUT_PULLUP);  // Pin 20
  pinMode(SCALE_UP_PIN, INPUT_PULLUP);      // Pin 35
  pinMode(SCALE_DOWN_PIN, INPUT_PULLUP);    // Pin 38
  pinMode(PLAY_PIN, INPUT_PULLUP);          // Pin 0
  pinMode(INSTR_UP_PIN, INPUT_PULLUP);      // Pin 36
  pinMode(INSTR_DOWN_PIN, INPUT_PULLUP);    // Pin 45
  pinMode(OCTAVE_UP_PIN, INPUT_PULLUP);     // pin 47
  pinMode(OCTAVE_DOWN_PIN, INPUT_PULLUP);   // pin 21

  for (int i = 0; i < 12; i++) {
  pinMode(notePins[i], INPUT_PULLUP);    
  }

  //AVERAGE
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
}

//SELECT SCALE
uint16_t scale_up_state = 0;
uint16_t scale_down_state = 0;
void ReadScaleUp() {
  const boolean scale_up = !digitalRead(SCALE_UP_PIN);
  if (scale_up != scale_up_state) {
    scale_index += scale_up;
    if (scale_index > 11) {
      scale_index = 0;
    }
    BLEMidiServer.controlChange(0, 26, scale_index);  // CC message to control the scale
  }
  scale_up_state = scale_up;
}
void ReadScaleDown() {
  const boolean scale_down = !digitalRead(SCALE_DOWN_PIN);
  if (scale_down != scale_down_state) {
    scale_index -= scale_down;
    if (scale_index < 0) {
      scale_index = 11;
    }
    BLEMidiServer.controlChange(0, 26, scale_index);
  }
  scale_down_state = scale_down;
}

  //Select instrument
  uint16_t instr_up_state = 0;
  uint16_t instr_down_state = 0;

void ReadInstrumentUp() {
  const boolean instr_up = !digitalRead(INSTR_UP_PIN);
  if (instr_up != instr_up_state) {
    instrument_index += instr_up;
    if (instrument_index > 19) {
      instrument_index = 0;
    }
    BLEMidiServer.controlChange(0, 32, instrument_index);  // CC message to control the instrument
    BLEMidiServer.programChange(0, instrument_index);       // PC message to change the instrument
  }
  instr_up_state = instr_up;
}

void ReadInstrumentDown() {
  const boolean instr_down = !digitalRead(INSTR_DOWN_PIN);
  if (instr_down != instr_down_state) {
    instrument_index -= instr_down;
    if (instrument_index < 0) {
      instrument_index = 19;
    }
    BLEMidiServer.controlChange(0, 32, instrument_index);  // CC message to control the instrument
    BLEMidiServer.programChange(0, instrument_index);       // PC message to change the instrument
  }
  instr_down_state = instr_down;
}
 
void ReadPlayPin() {
    static bool playPinState = false; // Track the current state (pressed/released)
    const boolean playPinPressed = !digitalRead(PLAY_PIN); // Read the state of the PLAY_PIN

    if (playPinPressed && !playPinState) { // Button pressed  
        BLEMidiServer.controlChange(0, 22, 127); // Send ON message  
        Serial.println("PLAY_PIN ON");
    } else if (!playPinPressed && playPinState) { // Button released  
        BLEMidiServer.controlChange(0, 22, 0); // Send OFF message  
        Serial.println("PLAY_PIN OFF");
    }

    playPinState = playPinPressed; // Update the last state  
}

void ReadOctaveShiftPin() {
    static bool OctaveShiftState = false; // Track the current state (pressed/released)
    const boolean OctaveShiftPinPressed = !digitalRead(OCTAVE_SHIFT_PIN); // Read the state of the PLAY_PIN

    if (OctaveShiftPinPressed && !OctaveShiftState) { // Button pressed  
        BLEMidiServer.controlChange(0, 18, 127); // Send ON message  
        Serial.println("OCTAVE_SHIFT_PIN ON");
    } else if (!OctaveShiftPinPressed && OctaveShiftState) { // Button released  
        BLEMidiServer.controlChange(0, 18, 0); // Send OFF message  
        Serial.println("OCTAVE_SHIFT_PIN OFF");
    }

    OctaveShiftState = OctaveShiftPinPressed; // Update the last state  
}

void UpdateDisplay() {
  // Define the text for scales and instruments  
  static const char* msgsscale[] = { "C   ", "Db  ", "D   ", "Eb  ", "E   ", "F   ", "F#  ", "G   ", "Ab  ", "A   ", "Bb  ", "B   " };
  static const char* msgsinstruments[] = {"Sax Alt Vib   ", "Flute Symph   ", "Oboe Symph   ", "Eng Horn   ", "Cornett    ",
                                          "Sax Tenor   ", "Flute Natur   ", "Pan Flute   ", "Clarinet   ", "Atmosphere  ", "Ooh Choir   ", "French Horn    ", "BRASS       ",
                                          "French Hrns   ", "Brass Four     ", "Strings Leg  ", "STRINGS      ", "Strings Ens   ", "Violin Slow   ", "Violin      "
                                         };

  // Keep track of the last displayed scale and instrument  
  static int old_scale = -1;
  static int old_instrument = -1;
  // Only clear the display when we first start or if we need to refresh  
  static bool firstRun = true; // Flag to check if this is the first run  
  if (firstRun) {
    oledFill(&ssoled, 0, 1); // Clear the display on the first run  
    firstRun = false; // Set the flag to false after the first run  
  }
  // Check if the instrument has changed  
  if (old_instrument != instrument_index) {
    old_instrument = instrument_index; // Update the old instrument  
    // Display the current instrument at (x=0, y=10)
    oledWriteString(&ssoled, 0, 10, 0, (char*) msgsinstruments[instrument_index], FONT_NORMAL, 0, 1);
  }
  // Check if the scale has changed  
  if (old_scale != scale_index) {
    old_scale = scale_index; // Update the old scale  
    // Display "Scale:" at (x=0, y=30)
    oledWriteString(&ssoled, 0, 10, 3, (char *)"Scale", FONT_SMALL, 0, 1);
    // Display the current scale at (x=0, y=50)
    oledWriteString(&ssoled, 0, 50, 2, (char*) msgsscale[scale_index], FONT_STRETCHED, 0, 1);
    // Redraw the instrument text to ensure it remains visible  
    oledWriteString(&ssoled, 0, 10, 0, (char*) msgsinstruments[old_instrument], FONT_NORMAL, 0, 1);
  }
}

//AVERAGE
#define RET_AVG(N_READINGS, exec) \
  { \
    const int numReadings = N_READINGS; \
    static int readings[numReadings] = {}; \
    static int readIndex = 0; \
    static int total = 0; \
\
    total -= readings[readIndex]; \
\
    readings[readIndex] = exec; \
    total += readings[readIndex]; \
\
    if (++readIndex >= numReadings) { \
      readIndex = 0; \
    } \
    return total / numReadings; \
  }

void CheckMouthTouch() {
  touchdetected = touchRead(MOUTH_PIN);
  if (touchdetected) {
    touchdetected = true;
    if (touchInterruptGetLastStatus(T5)) {
      mouthTouched = true;       
      Serial.println(" --- T5 Touched");
    } else {
      mouthTouched = false;      
      Serial.println(" --- T5 Released");
    }
  }
}

void ReadMouthPin() {
    static bool MouthPinState = false; // Track the current state (pressed/released)
    const boolean MouthPinPressed = touchInterruptGetLastStatus(T5); // Read the state of the PLAY_PIN

    if (MouthPinPressed && !MouthPinState) { // Button pressed  
        BLEMidiServer.controlChange(0, 25, 127); // Send ON message  
    } else if (!MouthPinPressed && MouthPinState) { // Button released  
        BLEMidiServer.controlChange(0, 25, 0); // Send OFF message  
    }
    MouthPinState = MouthPinPressed; // Update the last state  
}
void ReadAux1Pin() {
    static bool lastAux1state = false; // Track the current state (pressed/released)
    const boolean aux1Pressed = !digitalRead(AUX1_PIN); // Read the state of the PLAY_PIN

    if (aux1Pressed && !lastAux1state) { // Button pressed  
        BLEMidiServer.controlChange(0, 14, 127); // Send ON message  
    } else if (!aux1Pressed && lastAux1state) { // Button released  
        BLEMidiServer.controlChange(0, 14, 0); // Send OFF message  
    }
    lastAux1state = aux1Pressed; // Update the last state  
}


double averageEXP1() {
  const float sensorValue = analogRead(BREATH_PIN);
  const float outputValueEXP1 = map((sensorValue - offset) * sin(0.58), 0, 210, 0, 127);
  Serial.println(outputValueEXP1);                    
  RET_AVG(128, map(outputValueEXP1, 0, 127, 0, 127));  
}
double averageVOL1() {
  const float sensorValue = analogRead(BREATH_PIN);
  const float outputValueVOL1 =  map((sensorValue - offset) * sin(0.85), 0, 170, 0, 127);
  Serial.println(outputValueVOL1); 
  RET_AVG(128, map(outputValueVOL1, 0, 127, 0, 127));                   
}
double averageEXP2() {
  const float sensorValue = analogRead(BREATH_PIN);
  const float outputValueEXP2 = map((sensorValue - offset) * sin(0.55), 0, 200, 0, 127);
  Serial.println(outputValueEXP2);                    
  RET_AVG(128, map(outputValueEXP2, 0, 127, 0, 127));  
}
/*
double averageVOL2() {
  const float sensorValue = analogRead(BREATH_PIN);
  const float outputValueVOL2 = map((sensorValue - offset) * sin(0.90), 0, 180, 0, 127);
  Serial.println(outputValueVOL2);  
  RET_AVG(64, map(outputValueVOL2, 0, 127, 0, 127));                   
}
*/
// Function to read volume from accelerometer
float ReadVolumeFromAccelerometer() {
  sensor.read();
  float y = sensor.getAngleY();                      // Get the Y-axis angle
  float outputValueUD = map(y, -5.00, -26.00, 0, 127); // Map the angle to a MIDI volume range
  return constrain(outputValueUD, 0, 127);            // Constrain the value to valid MIDI range
}


void SendBreathInfo() {
  const float EXP1 = constrain(averageEXP1(), 0, 124);
  Serial.println(averageEXP1());
  const float VOL1 = constrain(averageVOL1(), 0, 127);
  Serial.println (averageVOL1());
  const float EXP2 = constrain(averageEXP2(), 0, 124);
  Serial.println(averageEXP2());
  // Replace VOL2 from breath sensor with accelerometer data
  const float VOL2 = ReadVolumeFromAccelerometer();
  Serial.println(ReadVolumeFromAccelerometer());
 
  if (mouthTouched) { 
    BLEMidiServer.controlChange(0, 11, EXP1); 
    BLEMidiServer.controlChange(0, 7, VOL1);     
  }
  if (mouthTouched && aux1Pressed) {
    BLEMidiServer.controlChange(1, 11, EXP2);
    BLEMidiServer.controlChange(1, 7, VOL2); 
  }
}

void SendNoteInfo() {
  unsigned long currentMillis = millis();
  const int octaveup = !digitalRead(OCTAVE_UP_PIN) * octave;
  const int octavedown = digitalRead(OCTAVE_DOWN_PIN) * octave;
  const int octaveupshift = !digitalRead(OCTAVE_SHIFT_PIN) * octaveShift;

  for (int i = 0; i < 12; i++) {
    const boolean state = !digitalRead(notePins[i]) && mouthTouched;
    const boolean has_state_changed = (state != lastPINstate[i]);
    const int pitch = SCALE[scale_index][i] + octaveup + octavedown + octaveupshift;

    if (has_state_changed) {
      lastPINstate[i] = state;
      lastPINchangetime[i] = currentMillis;
      if (state) {
        PIN_pitch_state[i] = pitch;
        BLEMidiServer.noteOn(0, pitch, 112); // Note on for channel 1                          
        if (aux1Pressed) {
          BLEMidiServer.noteOn(1, pitch, 92); // Note on for channel 2 
        }
      } else {
        BLEMidiServer.noteOff(0, PIN_pitch_state[i], 127); // Note off for channel 1  
        BLEMidiServer.noteOff(1, PIN_pitch_state[i], 127); // Note off for channel 2  
      }
    }
  }
}

//----------------------------------------------------------------
void loop() {
  CheckMouthTouch();
  aux1Pressed = !digitalRead(AUX1_PIN);
  ReadScaleUp();
  ReadScaleDown();
  ReadInstrumentUp();
  ReadInstrumentDown();
  ReadPlayPin();
  ReadOctaveShiftPin();
  UpdateDisplay();
  ReadMouthPin();
  ReadAux1Pin();
  SendBreathInfo();
  SendNoteInfo();
}

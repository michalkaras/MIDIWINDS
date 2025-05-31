/* !!! ATTENTION !!!
The code works with library versions:
NimBLE-Arduino 1.4.3 and ESP32-BLE-MIDI 0.3.2

board: ESP32-S3
breath: MPXV7002DP sensor
display: OLED I2C 128x32
accelerometer MPU6050
SWITCHES: 5-WAY DIP 10x10

CODE: The code is derived from the ESP-32-BLE-MIDI library 
example: 01-Basic-Midi-Device and 03-Receiving-Data - https://github.com/max22-

Michal Karas 
https://karas-midi-woodwinds.webnode.cz/
*/

#include <Arduino.h>
#include <BLEMidi.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>

//DISPLAY
#define SDA_PIN 8
#define SCL_PIN 9
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//ACCELEROMETER
Adafruit_MPU6050 mpu;

// KEY SELECTOR            C   D   E   F  G   A  B  C#  D#  F#  G#  A#
const int notePins[12] = { 1, 42, 40, 7, 16, 18, 11, 2, 41, 15, 17, 10 };
const int SCALE[12][12] = {
  // C   D   E   F  G   A   B   C#  D#  F#  G#  A#       KEY selection of Ionian scale
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

//const int MODULATION_PIN = 6;

static int scale_index = 0;
static int instrument_index = 0;

int PIN_pitch_state[12] = { 0 };
int lastPINstate[12] = { 0 };
int currentMillis = millis();
int lastPINchangetime[12] = { currentMillis };
const int octave = 12;
const int octaveShift = 12;

// State variables
bool mouthTouched = false;  // Track if the mouth pin is currently touched
bool aux1Pressed = false;   // Momentary button

// MOUTH
const int threshold = 52500;
// variable for storing the touch pin value
int touchDetected;

// BREATH
int offset = 2186;

// AVERAGE
const int numReadings = 0;
int readings[numReadings];  // The readings from the analog input

// Callback
void connected(){
 if (BLEMidiServer.isConnected()) {
    BLEMidiServer.controlChange(0, 26, scale_index);
    BLEMidiServer.controlChange(0, 32, instrument_index);
  }
}
void onControlChange(uint8_t channel, uint8_t controller, uint8_t value, uint16_t timestamp)
{
    Serial.printf("Received control change : channel %d, controller %d, value %d (timestamp %dms)\n", channel, controller, value, timestamp);

    // Scale change (controller 26)
    if(controller == 26 && value >= 0 && value <= 11) {
        scale_index = value;
        Serial.printf("Scale changed to %d\n", scale_index);
        UpdateDisplay();
    }
    // Instrument change (controller 32)
    else if(controller == 32 && value >= 0 && value <= 19) {
        instrument_index = value;
        Serial.printf("Instrument changed to %d\n", instrument_index);
        UpdateDisplay();
    }
}

//----------------------------------------------------------------
void setup(void) {
  Serial.begin(115200);
  Serial.println("Initializing bluetooth");
  BLEMidiServer.begin("JIM_0");
  Serial.println("Waiting for connections...");
  
  // Callback
  BLEMidiServer.setOnConnectCallback(connected);
  BLEMidiServer.setOnDisconnectCallback([](){     // To show how to make a callback with a lambda function
    Serial.println("Disconnected");
  });     
  BLEMidiServer.setControlChangeCallback(onControlChange);
 
  // DISPLAY
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println("SSD1306 allocation failed");
    for(;;);
  }
  //delay(2000); 
  display.clearDisplay();
  
  display.setRotation(2);            
  display.setTextColor(WHITE);
  display.setFont(&FreeMonoBold12pt7b);      
  display.setCursor(25, 22);             
  display.println("KARAS");
  display.display();
  delay(2000);

  //ACCELEROMETER
    if (!mpu.begin()) {
    while (1) {
      Serial.println("Failed to find MPU6050 chip");
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);


  //BREATH
  pinMode(BREATH_PIN, INPUT);               // Pin 4
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

  // NOTE PINS
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
    BLEMidiServer.programChange(0, instrument_index);      // PC message to change the instrument
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
    BLEMidiServer.programChange(0, instrument_index);      // PC message to change the instrument
  }
  instr_down_state = instr_down;
}

void ReadPlayPin() {
  static bool playPinState = false;                       // Track the current state (pressed/released)
  const boolean playPinPressed = !digitalRead(PLAY_PIN);  // Read the state of the PLAY_PIN

  if (playPinPressed && !playPinState) {      // Button pressed
    BLEMidiServer.controlChange(0, 22, 127);  // Send ON message
  } else if (!playPinPressed && playPinState) {  // Button released
    BLEMidiServer.controlChange(0, 22, 0);       // Send OFF message
  }

  playPinState = playPinPressed;  // Update the last state
}

void ReadOctaveShiftPin() {
  static bool OctaveShiftState = false;                                  // Track the current state (pressed/released)
  const boolean OctaveShiftPinPressed = !digitalRead(OCTAVE_SHIFT_PIN);  // Read the state of the OCTAVE_SHIFT_PIN

  if (OctaveShiftPinPressed && !OctaveShiftState) {  // Button pressed
    BLEMidiServer.controlChange(0, 18, 127);         // Send ON message
  } else if (!OctaveShiftPinPressed && OctaveShiftState) {  // Button released
    BLEMidiServer.controlChange(0, 18, 0);                  // Send OFF message
  }

  OctaveShiftState = OctaveShiftPinPressed;  // Update the last state
}

void ReadAUX2Pin() {
  static bool AUX2State = false;                                  // Track the current state (pressed/released)
  const boolean AUX2PinPressed = !digitalRead(AUX2_PIN);  // Read the state of the AUX2_PIN

  if (AUX2PinPressed && !AUX2State) {  // Button pressed
    BLEMidiServer.controlChange(0, 15, 127);         // Send ON message
  } else if (!AUX2PinPressed && AUX2State) {  // Button released
    BLEMidiServer.controlChange(0, 15, 0);                  // Send OFF message
  }

  AUX2State = AUX2PinPressed;  // Update the last state
}

//DISPLAY
void UpdateDisplay(){
    // Define the text for scales and instruments
  static const char *scale[] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
  static const char *instrument[] = { "SAX AV", "FLUTE", "OBOE", "E HORN", "CORNET",\
                                      "SAX T", "FLUTE N", "PAN FL", "CLARIN", "ATMOS", "F HORN","CHOIR", "BRASS",\
                                      "F HORNS", "BRASS 4", "STR LEG", "STRING", "STR ENS", "VIOLIN S", "VIOLIN" };

  // Keep track of the last displayed scale and instrument
  static int old_scale = -1;
  static int old_instrument = -1;

  // Check if the scale has changed
  if (old_scale != scale_index) {
  old_scale = scale_index;  // Update the old instrument
  
  }  
  // Check if the instrument has changed
  if (old_instrument != instrument_index) {
    old_instrument = instrument_index;  // Update the old instrument
  }

  display.clearDisplay();
  display.setRotation(2); 
  display.clearDisplay();            
  display.setTextColor(WHITE);
  display.setFont(&FreeSansBold9pt7b);      
  display.setCursor(0,25);             
  display.print(&*instrument[instrument_index]);
  display.setFont(&FreeSansBold18pt7b);
  display.setCursor(82,25); 
  display.println(&*scale[scale_index]);
  display.display();
  display.clearDisplay();
} 

//AVERAGE
#define RET_AVG(N_READINGS, exec) \
  { \
    const int numReadings = N_READINGS; \
    static int readings[numReadings] = { 0 }; \
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
  // Read the state of the TOUCH button value:
  touchDetected = touchRead(MOUTH_PIN);
  Serial.print(touchDetected);
  if(touchDetected > threshold){
    mouthTouched = true;
  }
  else{
    mouthTouched = false;
  }
}

void ReadMouthPin() {
    static bool MouthButton = false; // Track the current state (pressed/released)
    const bool MouthButtonPressed = mouthTouched; // Read the state of the MOUTH_PIN
    if (MouthButtonPressed && !MouthButton) { // Button pressed  
        BLEMidiServer.controlChange(0, 25, 127); // Send ON message  
    } else if (!MouthButtonPressed && MouthButton) { // Button released  
        BLEMidiServer.controlChange(0, 25, 0); // Send OFF message  
    }
    MouthButton = MouthButtonPressed; // Update the last state  
}

void ReadAux1Pin() {
  aux1Pressed = !digitalRead(AUX1_PIN);  // Read the state of the AUX1_PIN
  static bool lastAux1state = false;     // Track the current state (pressed/released)
  const bool aux1ON = aux1Pressed;

  if (aux1ON && !lastAux1state) {  // Button pressed
    aux1Pressed = true;
    BLEMidiServer.controlChange(0, 14, 127);  // Send ON message
  } else if (!aux1ON && lastAux1state) {      // Button released
    aux1Pressed = false;
    BLEMidiServer.controlChange(0, 14, 0);  // Send OFF message
  }
  lastAux1state = aux1Pressed;  // Update the last state
}

double averageEXP1() {
  const float sensorValueEXP1 = analogRead(BREATH_PIN);
  const float outputValueEXP1 = map((sensorValueEXP1 - offset) * sin(0.58), 0, 210, 48, 118);                  
  RET_AVG(24, constrain(outputValueEXP1, 0, 124));  
}

double averageVOL1() {
  const float sensorValueVOL1 = analogRead(BREATH_PIN);
  const float outputValueVOL1 =  map((sensorValueVOL1 - offset) * sin(0.92), 0, 170, 0, 127);
  RET_AVG(24, constrain(outputValueVOL1, 0, 124));                   
}

double averageEXP2() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  const float sensorvalueEXP2 = map((a.acceleration.x * 1000), -7200, -500, 0, 127);            
  RET_AVG(12, constrain(sensorvalueEXP2, 0, 102));  
}

double averageVOL2() {
  const float sensorValueVOL2 = analogRead(BREATH_PIN);
  const float outputValueVOL2 = map((sensorValueVOL2 - offset) * sin(0.85), 0, 180, 0, 127);  
  RET_AVG(64, constrain(outputValueVOL2, 0, 124));                   
}

double averagePB() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  const float sensorvaluePB = map((g.gyro.z * 1000), 500, 0, 8, 0);               
  RET_AVG(24, constrain(sensorvaluePB, -8, 8));                    
}

double averageMOD() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  const float sensorvalueMOD = map((g.gyro.x * 1000), 1000, 60, 127, 0);              
  RET_AVG(64, constrain(sensorvalueMOD, 0, 80));                    
}


void SendNoteInfo() {
  const float EXP1 = constrain(averageEXP1(), 24, 124);
  const float VOL1 = constrain(averageVOL1(), 48, 124);
  const float EXP2 = constrain(averageEXP2(), 0, 124);
  const float VOL2 = constrain(averageVOL2(), 24, 124);
  const float PB = constrain(averagePB(), -8, 8);
  const float MOD = constrain(averageMOD(), 0, 127);

  if (mouthTouched) {
    BLEMidiServer.controlChange(0, 11, EXP1);
    BLEMidiServer.controlChange(0, 7, VOL1);
    BLEMidiServer.pitchBend(0, PB, 127);
    BLEMidiServer.controlChange(0, 1, MOD);

  }
  if (mouthTouched && aux1Pressed) {
  BLEMidiServer.controlChange(1, 7, VOL2);
  BLEMidiServer.controlChange(1, 11, EXP2);
  BLEMidiServer.controlChange(1, 1, MOD);
  }

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
        BLEMidiServer.noteOn(0, pitch, EXP1);
        if (aux1Pressed) {
          BLEMidiServer.noteOn(1, pitch, EXP2);       
        } else {
          BLEMidiServer.noteOff(1, PIN_pitch_state[i], 127);
        }
      } else {
        BLEMidiServer.noteOff(0, PIN_pitch_state[i], 127);
        BLEMidiServer.noteOff(1, PIN_pitch_state[i], 127);
      }
    }
  }
}

//----------------------------------------------------------------
void loop() {
  CheckMouthTouch();
  ReadScaleUp();
  ReadScaleDown();
  ReadInstrumentUp();
  ReadInstrumentDown();
  ReadPlayPin();
  ReadOctaveShiftPin();
  ReadAUX2Pin();
  UpdateDisplay();
  ReadMouthPin();
  ReadAux1Pin();
  SendNoteInfo();
}

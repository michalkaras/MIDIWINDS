/*
  MPU6050 Raw

  A code for obtaining raw data from the MPU6050 module with the option to
  modify the data output format.

  Find the full MPU6050 library documentation here:
  https://github.com/ElectronicCats/mpu6050/wiki
*/
#include "I2Cdev.h"
#include "MPU6050.h"

#include <Arduino.h>
#include <BLEMidi.h>
#include "NimBLEDevice.h"

// MPU6050 default I2C address is 0x68
MPU6050 mpu;
//MPU6050 mpu(0x69);         //Use for AD0 high
//MPU6050 mpu(0x68, &Wire1); //Use for AD0 low, but 2nd Wire (TWI/I2C) object.
#define OUTPUT_READABLE_ACCELGYRO
int16_t ax, ay, az;

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing bluetooth");
  BLEMidiServer.begin("MPU6050_EXP2");
  Serial.println("Waiting for connections...");

  //--Start I2C interface--
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.begin(); 
  #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
    Fastwire::setup(400, true);
  #endif
}

void loop() {
  mpu.getAcceleration(&ax, &ay, &az);
  #ifdef OUTPUT_READABLE_ACCELGYRO
    Serial.print("ax: ");
    Serial.print(ax);
  #endif
  const float EXP2 = constrain(map((ax), 12000, 2000, 127, 0), 0, 127);
  Serial.print("  EXP2: ");
  Serial.println(EXP2);
  BLEMidiServer.controlChange(1, 11, EXP2);  
}

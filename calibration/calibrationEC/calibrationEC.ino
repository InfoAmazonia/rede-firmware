/*
The MIT License (MIT)

Copyright (c) 2015 DEV Tecnologia, Rede Infoamazônia

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "FreqCount.h"

// Offset values
uint16_t ecSmallCal = 0;
uint16_t ecMediumCal = 0;
uint16_t ecLargeCal = 0;
float ecStep = 0.0;

const byte MULT_A = 42;
const byte MULT_B = 43;
const byte MULT_C = 40;

const byte S1_EN = A5; // S1: EC sensor
const byte S2_EN = A6; // S2: ORP sensor
const byte S3_EN = A7; // S3: pH sensor

float ec = 0;

void setup() {
  Serial.begin(115200);

  // Set output pins
  pinMode(S1_EN, OUTPUT);
  digitalWrite(S1_EN, HIGH);
  pinMode(S2_EN, OUTPUT);
  digitalWrite(S2_EN, HIGH);
  pinMode(S3_EN, OUTPUT);
  digitalWrite(S3_EN, HIGH);

  pinMode(MULT_A, OUTPUT);
  digitalWrite(MULT_A, HIGH);
  pinMode(MULT_B, OUTPUT);
  digitalWrite(MULT_B, HIGH);
  pinMode(MULT_C, OUTPUT);
  digitalWrite(MULT_C, LOW);
}

void loop() {
  Serial.println("Beginning callibration...");
  Serial.println();
  
  Serial.println("Insert EC meter in the solution with 5000 uS/cm.");
  Serial.println("Press 'Send' after the probe is inserted.");
  while (Serial.available() == 0);
  Serial.readString();
  Serial.println("Measuring...");
  Serial.println();

  // Take the average of 5 measurements
  ecMediumCal = read_ec();

  Serial.println("Insert EC meter in the solution with 84 uS/cm.");
  Serial.println("Press 'Send' after the probe is inserted.");
  while (Serial.available() == 0);
  Serial.readString();
  Serial.println("Measuring...");
  Serial.println();

  ecSmallCal = read_ec();

  Serial.println("Insert EC meter in the solution with 12880 uS/cm.");
  Serial.println("Press 'Send' after the probe is inserted.");
  while (Serial.available() == 0);
  Serial.readString();
  Serial.println("Measuring...");
  Serial.println();

  ecLargeCal = read_ec();

  Serial.println("Finished making the measurements.");
  Serial.print("5000 uS/cm measurement: ");
  Serial.println(ecMediumCal);
  Serial.print("84 uS/cm measurement: ");
  Serial.println(ecSmallCal);
  Serial.print("12880 uS/cm measurement: ");
  Serial.println(ecLargeCal);

  float calc_ecSmall = calc_ecStepSmall();
  Serial.print("ec step based on 84 uS/cm measure: ");
  Serial.println(calc_ecSmall, 2);

  float calc_ecLarge = calc_ecStepLarge();
  Serial.print("ec step based on 12880 uS/cm measure: ");
  Serial.println(calc_ecLarge, 2);

  ecStep = calc_ecSmall;
  float ec = calc_ec(ecLargeCal);
  Serial.print("12880 uS/cm value when calibrated with 84 uS/cm: ");
  Serial.println(ec, 2);

  ecStep = calc_ecLarge;
  ec = calc_ec(ecSmallCal);
  Serial.print("84 uS/cm value when calibrated with 12880 uS/cm: ");
  Serial.println(ec, 2);

  Serial.println("----------------------------------");
  Serial.println("Values to calibrate the meter:");
  Serial.print("ec5000Cal: ");
  Serial.println(ecMediumCal);
  Serial.print("ecStep84 : ");
  Serial.println(calc_ecSmall, 2);
  Serial.print("ecStep12880 : ");
  Serial.println(calc_ecLarge, 2);
  Serial.println("----------------------------------");
  
  Serial.println();
  Serial.println();
  while (Serial.available() == 0);
  Serial.readString();
}

uint16_t read_ec() {
    // Calc EC
  delay(50);
  digitalWrite(S1_EN, LOW);
  FreqCount.begin(1000);
  while (!FreqCount.available());
  uint16_t val = FreqCount.read();
  FreqCount.end();
  // TODO: calibrate value
  digitalWrite(S1_EN, HIGH);

  return val;
}

float calc_ec(uint16_t val) {
  //Y = mx+b
  float tmp = (float)(ecMediumCal - val);
  return ((float)(5000 - (tmp / ecStep)));
}

float calc_ecStepSmall() {
  float ecStepeSmall = ((float)(ecMediumCal - ecSmallCal) / (5000-84));
  return abs (ecStepeSmall);
}

float calc_ecStepLarge() {
  float ecStepLarge = ((float)(ecLargeCal - ecMediumCal) / (12880-5000));
  return abs (ecStepLarge);
}

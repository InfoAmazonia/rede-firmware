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

// Offset values
uint16_t ph4Cal = 0;
uint16_t ph7Cal = 0;
uint16_t ph10Cal = 0;
float phStep = 0.0;

// Analog I/O pins
const byte PH_PIN = A12;

const byte MULT_A = 42;
const byte MULT_B = 43;
const byte MULT_C = 40;

const byte S1_EN = A5; // S1: EC sensor
const byte S2_EN = A6; // S2: ORP sensor
const byte S3_EN = A7; // S3: pH sensor

float ph = 0;

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
  
  Serial.println("Insert pH meter in the solution with pH 7.");
  Serial.println("Press 'Send' after the probe is inserted.");
  while (Serial.available() == 0);
  Serial.readString();
  Serial.println("Measuring...");
  Serial.println();

  // Take the average of 5 measurements
  ph7Cal = 0.0;
  for (int i = 0; i < 4; i++) {
    ph7Cal += read_ph();
  }
  ph7Cal >>= 2;

  Serial.println("Insert pH meter in the solution with pH 4.");
  Serial.println("Press 'Send' after the probe is inserted.");
  while (Serial.available() == 0);
  Serial.readString();
  Serial.println("Measuring...");
  Serial.println();

  ph4Cal = 0;
  for (int i = 0; i < 4; i++) {
    ph4Cal += read_ph();
  }
  ph4Cal >>= 2;

  Serial.println("Insert pH meter in the solution with pH 10.");
  Serial.println("Press 'Send' after the probe is inserted.");
  while (Serial.available() == 0);
  Serial.readString();
  Serial.println("Measuring...");
  Serial.println();

  ph10Cal = 0.0;
  for (int i = 0; i < 4; i++) {
    ph10Cal += read_ph();
  }
  ph10Cal >>= 2;

  Serial.println("Finished making the measurements.");
  Serial.print("ph7 measurement: ");
  Serial.println(ph7Cal);
  Serial.print("ph4 measurement: ");
  Serial.println(ph4Cal);
  Serial.print("ph10 measurement: ");
  Serial.println(ph10Cal);

  float calc_ph4 = calc_phStep4();
  Serial.print("ph step based on ph4: ");
  Serial.println(calc_ph4, 2);

  float calc_ph10 = calc_phStep10();
  Serial.print("ph step based on ph10: ");
  Serial.println(calc_ph10, 2);

  phStep = calc_ph4;
  float ph = calc_ph(ph10Cal);
  Serial.print("pH 10 value when calibrated with pH 4: ");
  Serial.println(ph, 2);

  phStep = calc_ph10;
  ph = calc_ph(ph4Cal);
  Serial.print("pH 4 value when calibrated with pH 10: ");
  Serial.println(ph, 2);

  Serial.println("----------------------------------");
  Serial.println("Values to calibrate the meter:");
  Serial.print("ph7Cal: ");
  Serial.println(ph7Cal);
  Serial.print("phStep4: ");
  Serial.println(calc_ph4, 2);
  Serial.print("phStep10: ");
  Serial.println(calc_ph10, 2);
  Serial.println("----------------------------------");
  
  Serial.println();
  Serial.println();
  while (Serial.available() == 0);
  Serial.readString();
}

uint16_t read_ph() {
  // Read pH
  delay(500);
  digitalWrite(S3_EN, LOW);
  delay(1000);
  uint16_t val = analogRead(PH_PIN);
  digitalWrite(S3_EN, HIGH);

  return val;
}

float calc_ph(uint16_t val) {
  float milli = (((float)val / 1024.0) * 5) * 1000.0;
  float tmp = ((((5.0 * ph7Cal) / 1024.0) * 1000.0) - milli) / 5.25;
  return 7.0 - (tmp / phStep);
}

float calc_phStep4() {
  float phStep4 = ((((5.0 * (ph7Cal - ph4Cal)) / 1024.0) * 1000.0) / 5.25) / 3.0;
  return phStep4;
}

float calc_phStep10() {
  float phStep10 = ((((5.0 * (ph10Cal - ph7Cal)) / 1024.0) * 1000.0) / 5.25) / 3.0;
  return phStep10;
}

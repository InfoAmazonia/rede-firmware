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
float orpOffset = 0.0;

// Analog I/O pins
const byte ORP_PIN = A11;

const byte MULT_A = 42;
const byte MULT_B = 43;
const byte MULT_C = 40;

const byte S1_EN = A5; // S1: EC sensor
const byte S2_EN = A6; // S2: ORP sensor
const byte S3_EN = A7; // S3: pH sensor

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

  Serial.println("Beginning callibration...");
  Serial.println("Insert a 'Short Circuit' on the ORP opening.");
  Serial.println("Press 'Send' after the 'Short' is inserted.");
  while (Serial.available() == 0);
  Serial.readString();
  Serial.println("Measuring...");
  Serial.println();

  orpOffset =  read_orp();
  
  Serial.println("----------------------------------");
  Serial.println("Values to calibrate the meter:");
  Serial.print("orpOffset: ");
  Serial.println(orpOffset);
  Serial.println("----------------------------------");
  
  Serial.println();
  Serial.println();
  while (Serial.available() == 0);
  Serial.readString();
}

void loop(){
  Serial.println(read_orp());
}

float read_orp() {
  // Read ORP
  delay(500);
  digitalWrite(S2_EN, LOW);
  delay(500);

  float val = analogRead(ORP_PIN) / 1024.0;
  val = (30.0 * 5.0 * 1000.0) - (75.0 * val * 5.0 * 1000);
  val = (val / 75.0) - orpOffset;
  
  digitalWrite(S2_EN, HIGH);
  
  return val;
}

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

#include <SoftwareSerial.h>
#include "Adafruit_FONA.h"
#include <Wire.h> //I2C needed for sensors
#include "MPL3115A2.h" //Pressure sensor
#include "HTU21D.h" //Humidity sensor
#include "OneWire.h" // Water temperature sensor

#include "FreqCount.h"

#define SEND_INTERVAL 30 // Interval between consecutive data sends (in seconds)

// Constants
// Digital I/O pins
const byte FONA_KEY = 10;
const byte FONA_RST = 12;
const byte FONA_EN_BAT = 13;

const byte MULT_A = 42;
const byte MULT_B = 43;
const byte MULT_C = 40;


// Analog I/O pins
const byte LIGHT = A13; // Not currently used
const byte REFERENCE_3V3 = A3; // Not currently used
const byte WDIR = A0; // Not currently used
const byte PH_PIN = A12;
const byte ORP_PIN = A11;
const byte WTEMP_PIN = 20;

const byte S1_EN = A5; // S1: EC sensor
const byte S2_EN = A6; // S2: ORP sensor
const byte S3_EN = A7; // S3: pH sensor

// Conductivity sensor (EC) uses pin 47 and disables pins 9, 10, 44, 45, 46.

// Offset values
const float PH_OFFSET = 0.0;
const float ORP_OFFSET = 0.0;
const float SYSTEM_VOLTAGE = 5.0;

MPL3115A2 myPressure; // Create an instance of the pressure sensor
HTU21D myHumidity; // Create an instance of the humidity sensor
OneWire waterTemperature(WTEMP_PIN);

float humidity = 0; // [%]
float temp = 0; // [temperature C]
float pressure = 0; // [pressure Pa]
float light_lvl = 455; //[analog value from 0 to 1023]
float wtemp = 0; // [water temperature C]
float ph = 0;
float orp = 0;
float ec = 0;

// Message buffer
char buffer[161];
char buffer2[201];

// Destination number of the message
char destination[] = "5511947459448";

// Destination URL of the HTTP POST
char http_post_url[] = "http://rede.infoamazonia.org/api/v1/measurements/new";

// Id of the device
char id[] = "+551199999991";

// Time elapsed since last measurement (in seconds)
int time_elapsed = 0;

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

void setup() {
  Serial.begin(115200);
  Serial.println(F("Initializing..."));
  
  pinMode(REFERENCE_3V3, INPUT);
  pinMode(LIGHT, INPUT);

  // Configure the pressure sensor
  myPressure.begin(); // Get sensor online
  myPressure.setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
  myPressure.setOversampleRate(7); // Set Oversample to the recommended 128
  myPressure.enableEventFlags(); // Enable all three pressure and temp event flags 

  // Configure the humidity sensor
  myHumidity.begin();

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
  
  // Set key pin to low
  pinMode(FONA_KEY, OUTPUT);
  digitalWrite(FONA_KEY, LOW);

  // Enables fona battery
  pinMode(FONA_EN_BAT, OUTPUT);
  digitalWrite(FONA_EN_BAT, HIGH);

  // Connection to the SIM800
  Serial2.begin(4800);

  // See if the FONA is responding
  if (!fona.begin(Serial2)) {
    Serial.println(F("FONA not found"));
    while (1);
  }
  Serial.println(F("FONA is OK"));

  // Configure a GPRS APN, username, and password.
  fona.setGPRSNetworkSettings(F("zap.vivo.com.br"), F(""), F(""));
  
  // It takes around 10 seconds for the FONA to initialize
  // We need to wait the initialization in order to enable GPRS
  // So...
  delay(10000);
  if (!fona.enableGPRS(true))
  {
    Serial.println("FONA GPRS not enabled!");
  }
  
  if (!fona.enableNetworkTimeSync(true)) {
    Serial.println(F("Failed to enable time sync"));
  }

  calc_sensors();
}

void loop() {
  if (time_elapsed >= SEND_INTERVAL) {
    time_elapsed = 0;
    // Get time.
    fona.getTime(buffer+1, 25);
    // Get readings from weather shield.
    calc_sensors();

    // Format of the buffer:
    // <YYYY-MM-DDTHH:MM:SS-03:00>;<type1>:<value1>:{unit1};...;<typeN>:<valueN>:{unitN}
    // Manually add 20 to the beginning of the buffer.
    buffer[0] = '2';
    buffer[1] = '0';
    int len = strlen(buffer);
    // Convert the date received to ISO format.
    for (int i = 0; i < len; i++) {
      if (buffer[i] == '/')
        buffer[i] = '-';
      else if (buffer[i] == ',')
        buffer[i] = 'T';
      else if (buffer[i] == '-') { // Detect -12 to convert to -03:00
        int tmp = 0;
        sscanf(&buffer[i+1], "%d", &tmp);
        sprintf(&buffer[i+1], "%02d:%02d", tmp / 4, (tmp % 4) * 15);
        break;
      }
    }

    // Use dtostrf now to print data to the buffer.

    // Add humidity
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "RH=");
    len = strlen(buffer);
    dtostrf(humidity, 1, 1, &buffer[len]);

    // Add temperature
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "Ta:");
    len = strlen(buffer);
    sprintf(&buffer[len], "C=");
    len = strlen(buffer);
    dtostrf(temp, 1, 1, &buffer[len]);

    // Add pressure 
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "AP:");
    len = strlen(buffer);
    sprintf(&buffer[len], "Pa=");
    len = strlen(buffer);
    dtostrf(pressure, 1, 0, &buffer[len]);

    // Add light level 
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "L=");
    len = strlen(buffer);
    dtostrf(light_lvl, 1, 0, &buffer[len]);

    // Add ph 
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "pH=");
    len = strlen(buffer);
    dtostrf(ph, 1, 2, &buffer[len]);

    // Add orp
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "ORP:");
    len = strlen(buffer);
    sprintf(&buffer[len], "mV=");
    len = strlen(buffer);
    dtostrf(orp, 1, 2, &buffer[len]);

    // Add ec 
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "EC:");
    len = strlen(buffer);
    sprintf(&buffer[len], "S/m=");
    len = strlen(buffer);
    dtostrf(ec, 1, 1, &buffer[len]);

    // Add water temperature
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "Tw:");
    len = strlen(buffer);
    sprintf(&buffer[len], "C=");
    len = strlen(buffer);
    dtostrf(wtemp, 1, 1, &buffer[len]);

    // Created message
    // Print message for reference
    Serial.println(buffer);
    Serial.print("Size: ");
    Serial.println(strlen(buffer));

    // Send message
    if (!fona.sendSMS(destination, buffer)) {
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("Sent!"));
    }

    if (!send_http_post(http_post_url, id, buffer)) {
      Serial.println(F("HTTP POST Failed"));
    } else {
      Serial.println(F("HTTP POST Send!"));
    }
  }
  
  // Increase counter, sleep for 1 second.
  time_elapsed++;
  delay(1000);
}

boolean send_http_post(char *url, char *id, char *data) {
  uint16_t statuscode;
  uint16_t response_length;
  boolean post_success;

  //clear serial buffer
  while (Serial.available()) 
  {
    Serial.read();
  }
  
  sprintf(buffer2, "{\"sensorIdentifier\":\"%s\",\"data\":\"%s\"}",
                    id, data);

  Serial.print("POST message: ");
  Serial.println(buffer2);
    
  fona.HTTP_POST_start(url, F("application/json"), (uint8_t *)buffer2, strlen(buffer2), &statuscode, &response_length);

  Serial.print("Status: ");
  Serial.println(statuscode);

  post_success = (statuscode == 200);
  
  while (response_length > 0 && fona.available()) {
    Serial.write(fona.read());
    response_length--;
  }

  fona.HTTP_POST_end();
  
  return post_success;
}

// Get the values for each sensor
void calc_sensors() {
  Wire.begin();
  delay(100);
  
  // Calc humidity
  humidity = myHumidity.readHumidity();
  
  // Calc temp from pressure sensor
  temp = myPressure.readTemp();

  // Calc pressure
  pressure = myPressure.readPressure();
  
  //End the I2C Hardware so the temp sensor on the same bys can work
  TWCR = 0;

  // Calc light level
  //light_lvl = get_light_level();

  // Calc EC
  delay(500);
  digitalWrite(S1_EN, LOW);
  FreqCount.begin(1000);
  while ( !FreqCount.available());
  ec = FreqCount.read();
  FreqCount.end();
  // TODO: calibrate value
  digitalWrite(S1_EN, HIGH);

  // Calc orp
  delay(500);
  digitalWrite(S2_EN, LOW);
  delay(1000);
  orp = (30.0 * SYSTEM_VOLTAGE * 1000.0) - (75.0 * analogRead(ORP_PIN) *
      SYSTEM_VOLTAGE * 1000 / 1024.0);
  orp = (orp / 75.0) - ORP_OFFSET;
  digitalWrite(S2_EN, HIGH);

  // Calc pH
  delay(500);
  digitalWrite(S3_EN, LOW);
  delay(1000);
  ph = analogRead(PH_PIN) * 5.0 / 1024;
  ph = 3.5 * ph + PH_OFFSET;
  digitalWrite(S3_EN, HIGH);

  // Calc water temperature
  wtemp = get_water_temperature();
}

// Get the water temperature for a DS18B20 sensor
float get_water_temperature() {
  static byte addr[8];
  static byte data[12];
  static int16_t raw;
  static byte i;
  static byte cfg;

  waterTemperature.reset_search();
  if (!waterTemperature.search(addr)) {
    // No sensors found.
    return 0.0;
  }

  waterTemperature.reset();
  waterTemperature.select(addr);

  // Start acquisition, with parasite power on at the end
  waterTemperature.write(0x44, 1);

  // Wait for the measurement
  delay(1000);

  waterTemperature.reset();
  waterTemperature.select(addr);

  // Read Scratchpad
  waterTemperature.write(0xBE);

  // Read 9 bytes
  for (i = 0; i < 9; i++) {
    data[i] = waterTemperature.read();
  }

  // Convert data to temperature, which is a 16bit signed integer.
  raw = (data[1] << 8) | data[0];
  cfg = (data[4] & 0x60);
  if (cfg == 0x00) raw = raw & ~7; // 9 bit resolution, 93.75ms
  else if (cfg == 0x20) raw = raw & ~3; // 10 bit resolution, 187.5ms
  else if (cfg == 0x40) raw = raw & ~1; // 11 bit resolution, 375ms
  // Default is 12 bit resolution, 750 ms acquisition time.

  // Convert to celsius
  return (float)raw / 16.0;
}

//Returns the voltage of the light sensor based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
float get_light_level() {
  float operatingVoltage = analogRead(REFERENCE_3V3);

  float lightSensor = analogRead(LIGHT);
  
  operatingVoltage = 3.3 / operatingVoltage; //The reference voltage is 3.3V
  
  lightSensor = operatingVoltage * lightSensor;
  
  return(lightSensor);
}

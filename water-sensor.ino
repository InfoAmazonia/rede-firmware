#include <SoftwareSerial.h>
#include "Adafruit_FONA.h"
#include <Wire.h> //I2C needed for sensors
#include "MPL3115A2.h" //Pressure sensor
#include "HTU21D.h" //Humidity sensor
#include "OneWire.h" // Water temperature sensor

#include "FreqCount.h"

#define SEND_INTERVAL 900 // Interval between consecutive data sends (in seconds)

// Constants
// Digital I/O pins
const byte FONA_KEY = 10;
const byte FONA_RST = 12;
const byte FONA_EN_BAT = 13;

// Analog I/O pins
const byte LIGHT = A5; // Not currently used
const byte REFERENCE_3V3 = A3; // Not currently used
const byte WDIR = A0; // Not currently used
const byte PH_PIN = A4;
const byte ORP_PIN = A3;
const byte WTEMP_PIN = A15;
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

// Destination number of the message
char destination[] = "5511947459448";

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

  // Start EC sensor
  FreqCount.begin(1000);

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
  fona.setGPRSNetworkSettings(F("zap.vivo.com.br"), F("vivo"), F("vivo"));

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
    sprintf(&buffer[len], "U:");
    len = strlen(buffer);
    dtostrf(humidity, 1, 1, &buffer[len]);
    len = strlen(buffer);
    sprintf(&buffer[len], ":RH");

    // Add temperature
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "TA:");
    len = strlen(buffer);
    dtostrf(temp, 1, 1, &buffer[len]);
    len = strlen(buffer);
    sprintf(&buffer[len], ":C");

    // Add pressure 
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "P:");
    len = strlen(buffer);
    dtostrf(pressure, 1, 0, &buffer[len]);
    len = strlen(buffer);
    sprintf(&buffer[len], ":Pa");

    // Add light level 
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "L:");
    len = strlen(buffer);
    dtostrf(light_lvl, 1, 0, &buffer[len]);

    // Add ph 
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "PH:");
    len = strlen(buffer);
    dtostrf(ph, 1, 2, &buffer[len]);

    // Add orp
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "ORP:");
    len = strlen(buffer);
    dtostrf(orp, 1, 2, &buffer[len]);
    len = strlen(buffer);
    sprintf(&buffer[len], ":mV");

    // Add ec 
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "EC:");
    len = strlen(buffer);
    dtostrf(ec, 1, 1, &buffer[len]);
    len = strlen(buffer);
    sprintf(&buffer[len], ":S/m");

    // Add water temperature
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "TH:");
    len = strlen(buffer);
    dtostrf(wtemp, 1, 1, &buffer[len]);
    len = strlen(buffer);
    sprintf(&buffer[len], ":C");

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
  }
  
  // Increase counter, sleep for 1 second.
  time_elapsed++;
  delay(1000);
}

// Get the values for each sensor
void calc_sensors() {
  // Calc humidity
  humidity = myHumidity.readHumidity();

  // Calc temp from pressure sensor
  temp = myPressure.readTemp();

  // Calc pressure
  pressure = myPressure.readPressure();

  // Calc light level
  light_lvl = get_light_level();

  // Calc pH
  ph = analogRead(PH_PIN) * 5.0 / 1024;
  ph = 3.5 * ph + PH_OFFSET;

  // Calc orp
  orp = (30.0 * SYSTEM_VOLTAGE * 1000.0) - (75.0 * analogRead(ORP_PIN) *
      SYSTEM_VOLTAGE * 1000 / 1024.0);
  orp = (orp / 75.0) - ORP_OFFSET;

  // Calc EC
  ec = FreqCount.read();
  // TODO: calibrate value

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

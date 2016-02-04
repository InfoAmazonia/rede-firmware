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
#include <avr/wdt.h> // Watchdog

#include "FreqCount.h"

// Defines to enable or disable sensors
#define HUMIDITY_SENSOR 0
#define AMBIENT_TEMPERATURE_SENSOR 1
#define PRESSURE_SENSOR 1
#define LIGHT_LEVEL_SENSOR 0
#define PH_SENSOR 1
#define ORP_SENSOR 1
#define EC_SENSOR 1
#define WATER_TEMPERATURE_SENSOR 1

// Define to enable or disable watchdog
#define WATCHDOG_ENABLE 0

// Define to enable or disable messages
#define SEND_SMS 0
#define SEND_HTTP_POST 1

/*********************************************************************************/

/******* SENSOR CONFIG *******/
// pH calibration values, need to be filled with values measured at the
// calibration program
float phStep = 65.72; // Update this value!!!
uint16_t ph7Cal = 376; // Update this value!!!

// EC calibration values, need to be filled with values measured at the
// calibration program
float ecStep = 2.23; // Update this value!!!
uint16_t ec5kCal = 11880; // Update this value!!!

// ORP calibration values, need to be filled with values measured at the
// calibration program
uint16_t orpOffset = 46.88; // Update this value!!!

/******* ID CONFIG *******/
// Id of the device
char id[] = "+5511997646041";

/******* TIMER CONFIG *******/
// Time elapsed since last measurement (in seconds)
int time_elapsed = 60;

// Interval between consecutive data sends (in seconds)
#define SEND_INTERVAL 1 //3600
#define NUM_RETRIES 5

/*********************************************************************************/

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

static byte addr[8];

// Destination number of the message
char destination[] = "5511947459448";

// Destination URL of the HTTP POST
char http_post_url[] = "http://rede.infoamazonia.org/api/v1/measurements/new";

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

#if WATCHDOG_ENABLE
uint8_t wdt_cycles;

// Watchdog timer interrupt
ISR(WDT_vect) {
  // More than 1 minute without finishing the loop, something might have happened
  if (wdt_cycles >= 8) {
    // Enable modifications to the watchdog
    WDTCSR |= (1<<WDCE) | (1<<WDE);
    // Set new watchdog timeout (8 seconds) and set watchdog to interrupt and
    // system reset mode
    WDTCSR = (1<<WDP3) | (1<<WDP0) | (1<<WDIE) | (1<<WDE);
  }
  wdt_cycles++;
}
#endif // WATCHDOG_ENABLE

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

  // Connection to the SIM800
  Serial2.begin(4800);

#if WATCHDOG_ENABLE
  cli(); // Disable interrupts

  wdt_reset();

  // Enable modifications to the watchdog
  WDTCSR |= (1<<WDCE) | (1<<WDE);

  // Set new watchdog timeout (8 seconds) and set watchdog to interrupt mode 
  WDTCSR = (1<<WDP3) | (1<<WDP0) | (1<<WDIE);

  wdt_cycles = 0;
  wdt_reset();

  sei(); // Enable interrupts
#endif // WATCHDOG_ENABLE

  calc_sensors();
}

void loop() {
  int i;
  boolean fona_enabled = false;
  if (time_elapsed >= SEND_INTERVAL) {
    time_elapsed = 0;
    // Get readings from weather shield.
    calc_sensors();

    // Enable fona
    fona_enabled = enable_fona();
    // Get time.
    fona.getTime(buffer+1, 25);

    // Format of the buffer:
    // <YYYY-MM-DDTHH:MM:SS-03:00>;<type1>:<value1>:{unit1};...;<typeN>:<valueN>:{unitN}
    // Manually add 20 to the beginning of the buffer.
    buffer[0] = '2';
    //buffer[1] = '0';
    int len = strlen(buffer);
    // Convert the date received to ISO format.
    for (i = 0; i < len; i++) {
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

#if HUMIDITY_SENSOR
    // Add humidity
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "RH=");
    len = strlen(buffer);
    dtostrf(humidity, 1, 1, &buffer[len]);
#endif // HUMIDITY_SENSOR

#if AMBIENT_TEMPERATURE_SENSOR
    // Add ambient temperature
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "Ta:");
    len = strlen(buffer);
    sprintf(&buffer[len], "C=");
    len = strlen(buffer);
    dtostrf(temp, 1, 1, &buffer[len]);
#endif // AMBIENT_TEMPERATURE_SENSOR

#if PRESSURE_SENSOR
    // Add pressure 
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "AP:");
    len = strlen(buffer);
    sprintf(&buffer[len], "Pa=");
    len = strlen(buffer);
    dtostrf(pressure, 1, 0, &buffer[len]);
#endif // PRESSURE_SENSOR

#if LIGHT_LEVEL_SENSOR
    // Add light level 
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "E=");
    len = strlen(buffer);
    dtostrf(light_lvl, 1, 0, &buffer[len]);
#endif // LIGHT_LEVEL_SENSOR

#if PH_SENSOR
    // Add ph 
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "pH=");
    len = strlen(buffer);
    dtostrf(ph, 1, 2, &buffer[len]);
#endif // PH_SENSOR

#if ORP_SENSOR
    // Add orp
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "ORP:");
    len = strlen(buffer);
    sprintf(&buffer[len], "mV=");
    len = strlen(buffer);
    dtostrf(orp, 1, 2, &buffer[len]);
#endif // ORP_SENSOR

#if EC_SENSOR
    // Add ec 
    len = strlen(buffer);
    buffer[len] = ';';
    len++;
    sprintf(&buffer[len], "EC:");
    len = strlen(buffer);
    sprintf(&buffer[len], "S/m=");
    len = strlen(buffer);
    dtostrf(ec, 1, 1, &buffer[len]);
#endif // EC_SENSOR

#if WATER_TEMPERATURE_SENSOR
    // Add water temperature
	if (wtemp != -100)
	{
		len = strlen(buffer);
		buffer[len] = ';';
		len++;
		sprintf(&buffer[len], "Tw:");
		len = strlen(buffer);
		sprintf(&buffer[len], "C=");
		len = strlen(buffer);
		dtostrf(wtemp, 1, 1, &buffer[len]);
	}
#endif // WATER_TEMPERATURE_SENSOR

    // Created message
    // Print message for reference
    Serial.println(buffer);
    Serial.print("Size: ");
    Serial.println(strlen(buffer));

    if (!fona_enabled) {
      disable_fona();
      fona_enabled = enable_fona();
    }
    
#if SEND_SMS
    // Send message
    for (i = 0; i < NUM_RETRIES; i++) {
      if (fona.sendSMS(destination, buffer)) {
        break;
      }
      delay(1000);
    }
    if (i >= NUM_RETRIES) {
      Serial.println(F("SMS failed"));
    } else {
      Serial.println(F("SMS sent!"));
    }
#endif // SEND_SMS

#if SEND_HTTP_POST
    for (i = 0; i < NUM_RETRIES; i++) {
      if (send_http_post(http_post_url, id, buffer)) {
        break;
      }
      delay(5000);
    }
    if (i >= NUM_RETRIES) {
      Serial.println(F("HTTP POST Failed"));
    }
    else {
      Serial.println(F("HTTP POST Sent!"));
    }
#endif // SEND_HTTP_POST
    disable_fona();
  }

#if WATCHDOG_ENABLE
  // Finished the loop, reset the watchdog and restart the count
  wdt_reset();
  wdt_cycles = 0;

  // Enable modifications to the watchdog
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  // Set new watchdog timeout (8 seconds) and set watchdog to interrupt mode 
  WDTCSR = (1<<WDP3) | (1<<WDP0) | (1<<WDIE);
#endif // WATCHDOG_ENABLE

  // Increase counter, sleep for 1 second.
  time_elapsed++;
  delay(1000);
}

boolean enable_fona() {
  // Set key pin to low
  int i;
  pinMode(FONA_KEY, OUTPUT);
  digitalWrite(FONA_KEY, LOW);

  // Enable fona battery
  pinMode(FONA_EN_BAT, OUTPUT);
  digitalWrite(FONA_EN_BAT, HIGH);
  
  // See if the FONA is responding
  for (i = 0; i < NUM_RETRIES; i++) {
    if (fona.begin(Serial2)) {
      break;
    }
    delay(1000);
  }
  if (i >= NUM_RETRIES) {
    Serial.println(F("FONA not found"));
    return false;
  }
  Serial.println(F("FONA is OK"));

  // Configure a GPRS APN, username, and password.
  fona.setGPRSNetworkSettings(F("zap.vivo.com.br"), F(""), F(""));
  
  // It takes around 10 seconds for the FONA to initialize
  // We need to wait the initialization in order to enable GPRS
  // So...
  delay(10000);
  for (i = 0; i < NUM_RETRIES && !fona.enableGPRS(true); i++) {
    Serial.println("FONA GPRS not enabled!");
    delay(10000);
  }
  if (i >= NUM_RETRIES) {
    Serial.println("Failed to enable GPRS!");
    return false;
  }
  
  if (!fona.enableNetworkTimeSync(true)) {
    Serial.println(F("Failed to enable time sync"));
  }

  Serial.println(F("Enabled fona"));

  return true;
}

void disable_fona() {
  // Set key pin to high
  pinMode(FONA_KEY, OUTPUT);
  digitalWrite(FONA_KEY, HIGH);

  // Disable fona battery
  pinMode(FONA_EN_BAT, OUTPUT);
  digitalWrite(FONA_EN_BAT, LOW);

  Serial.println(F("Disabled fona"));
}

boolean send_http_post(char *url, char *id, char *data) {
  uint16_t statuscode;
  uint16_t response_length;
  boolean post_success;

  //clear serial buffer
  while (Serial.available()) {
    Serial.read();
  }
  
  sprintf(buffer2, "{\"sensorIdentifier\":\"%s\",\"data\":\"%s\"}",
                    id, data);

  Serial.print("POST message: ");
  Serial.println(buffer2);
    
  fona.HTTP_POST_start(url, F("application/json"), (uint8_t *)buffer2,
                       strlen(buffer2), &statuscode, &response_length);

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
void calc_sensors()
{
  Wire.begin();
  delay(100);
  float val, minVal,maxVal;
  int i,j;
#if HUMIDITY_SENSOR
  // Calc humidity
  humidity = 0;
  for(i = 0; i < 3; i++)
  {
    humidity += myHumidity.readHumidity();
  }
  humidity = humidity/3;
#endif // HUMIDITY_SENSOR
  
#if AMBIENT_TEMPERATURE_SENSOR
  // Calc temp from pressure sensor
  temp = myPressure.readTemp();
#endif // AMBIENT_TEMPERATURE_SENSOR

#if PRESSURE_SENSOR
  // Calc pressure
  pressure = myPressure.readPressure();
#endif // PRESSURE_SENSOR
  
  //End the I2C Hardware so the temp sensor on the same bys can work
  TWCR = 0;

#if LIGHT_LEVEL_SENSOR
  // Calc light level
  light_lvl = get_light_level();
#endif // LIGHT_LEVEL_SENSOR

#if EC_SENSOR
  // Calc EC
  minVal = 100000;
  maxVal = -100000;
  val = 0;
  for(i = 0; i < 5; i++)
  {
    delay(500);
    digitalWrite(S1_EN, LOW);
    FreqCount.begin(1000);
    while (!FreqCount.available());
    ec = FreqCount.read();
    FreqCount.end();
    ec = 5000.0 + (ec - ec5kCal) / ecStep;
    digitalWrite(S1_EN, HIGH);
    val += ec;  
    if (ec < minVal)
    {
      minVal = ec;
    } 
    if (ec > maxVal)
    {
      maxVal = ec;
    }
  }
  //Remove the largest and lowest 
  val -= minVal;
  val -= maxVal;
  ec = val/3;
#endif // EC_SENSOR

#if ORP_SENSOR
  // Calc orp
  minVal = 100000;
  maxVal = -100000;
  val = 0;
  for(i = 0; i < 5; i++)
  {
    delay(500);
    digitalWrite(S2_EN, LOW);
    delay(1000);
    orp = analogRead(ORP_PIN) / 1024.0;
    orp = (30.0 * 5.0 * 1000.0) - (75.0 * orp * 5.0 * 1000);
    orp = (orp / 75.0) - orpOffset;
    digitalWrite(S2_EN, HIGH);
    val += orp;  
    if (orp < minVal)
    {
      minVal = orp;
    } 
    if (orp > maxVal)
    {
      maxVal = orp;
    }
  }
  //Remove the largest and lowest 
  val -= minVal;
  val -= maxVal;
  orp = val/3;
#endif // ORP_SENSOR

#if PH_SENSOR
  // Calc pH
  // To improve the readings we will do 5 readings,
  // remove the largest and lowest and average the
  // remaining 3.
  minVal = 100000;
  maxVal = -100000;
  val = 0;
  for(i = 0; i < 5; i++)
  {
    delay(1000);
    digitalWrite(S3_EN, LOW);
    delay(1000);
    ph = ((analogRead(PH_PIN) / 1024.0) * 5.0) * 1000.0;
    ph = ((((5.0 * ph7Cal) / 1024.0) * 1000.0) - ph) / 5.25;
    ph = 7.0 - (ph / phStep);
    digitalWrite(S3_EN, HIGH);
    val += ph;  
    if (ph < minVal)
    {
      minVal = ph;
    } 
    if (ph > maxVal)
    {
      maxVal = ph;
    }
  }
  //Remove the largest and lowest 
  val -= minVal;
  val -= maxVal;
  ph = val/3;
#endif // PH_SENSOR

#if WATER_TEMPERATURE_SENSOR
  // Calc water temperature
  val = 0;
  j = 3;
  for(i = 0; i < 3; i++)
  {
    wtemp = get_water_temperature();
    if(water_offline())
    {
      Serial.println("Error with water temperature reading, trying again");
      wtemp = get_water_temperature();        
      if (water_offline())
      {
        Serial.println("Couldn't get water temperature");        
        wtemp = 0;
        j--;
      }
    }
    val += wtemp;
  }
  
  if (j == 0)
  {
    wtemp = -100;
  }
  else
  {
    wtemp = val/j;
  }
#endif // WATER_TEMPERATURE_SENSOR
}

int water_offline()
{
	waterTemperature.reset_search();
    return (!waterTemperature.search(addr));
}

// Get the water temperature for a DS18B20 sensor
float get_water_temperature() {
  static byte data[12];
  static int16_t raw;
  static byte i;
  static byte cfg;

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

// Returns the voltage of the light sensor based on the 3.3V rail
// This allows us to ignore what VCC might be (an Arduino plugged into USB has
// VCC of 4.5 to 5.2V)
float get_light_level() {
  float operatingVoltage = analogRead(REFERENCE_3V3);

  float lightSensor = analogRead(LIGHT);
  
  operatingVoltage = 3.3 / operatingVoltage; //The reference voltage is 3.3V
  
  lightSensor = operatingVoltage * lightSensor;
  
  return(lightSensor);
}

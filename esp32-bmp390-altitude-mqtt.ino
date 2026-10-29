/*
 * ESP32 BMP390 Barometric Altitude Node (MQTT)
 * --------------------------------------------
 * Reads pressure and temperature from a Bosch BMP390 over I2C, applies
 * oversampling + an IIR low-pass filter inside the sensor, converts pressure
 * to altitude using a configurable sea-level reference, and publishes JSON
 * to an MQTT broker.
 *
 * Handy for drone, hill-trek and weather-balloon style data loggers, and for
 * thesis prototypes that need relative height changes of well under a metre.
 *
 * Required libraries (install via Library Manager):
 *   - "PubSubClient" by Nick O'Leary
 *   - "Adafruit BMP3XX Library" by Adafruit
 *   - "Adafruit Unified Sensor" by Adafruit
 *
 * Wiring (I2C):
 *   BMP390 VIN -> ESP32 3V3
 *   BMP390 GND -> ESP32 GND
 *   BMP390 SCL -> ESP32 GPIO22
 *   BMP390 SDA -> ESP32 GPIO21
 *   (I2C address 0x77 by default, 0x76 if SDO is tied to GND)
 *
 * Setup:
 *   1. Copy config.h.example to config.h
 *   2. Fill in WiFi + MQTT broker details and your local SEALEVEL_HPA.
 *   3. Flash. Open Serial Monitor at 115200.
 *
 * Sample MQTT payload:
 *   {"temp_c":28.41,"pressure_hpa":1008.62,"altitude_m":57.3,"rel_alt_m":0.0}
 *
 * (c) 2026 Fluidwire. Released under the MIT License.
 * https://github.com/fluidwire
 */

#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>
#include "config.h"

const uint8_t  BMP_ADDR            = 0x77;          // use 0x76 if SDO -> GND
const uint32_t PUBLISH_INTERVAL_MS = 5000UL;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
Adafruit_BMP3XX bmp;

bool  haveSensor = false;
bool  haveBaseline = false;
float baselineAlt = 0.0f;   // altitude at first good reading (for rel_alt_m)

void connectWiFi() {
  Serial.print(F("[wifi] connecting to "));
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.print(F("[wifi] IP: "));
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  while (!mqtt.connected()) {
    Serial.print(F("[mqtt] connecting... "));
    bool ok = strlen(MQTT_USER) > 0
      ? mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)
      : mqtt.connect(MQTT_CLIENT_ID);
    if (ok) {
      Serial.println(F("ok"));
    } else {
      Serial.print(F("failed state="));
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

// International barometric formula. pressure_hpa and sea_level_hpa in hPa.
float pressureToAltitude(float pressure_hpa, float sea_level_hpa) {
  return 44330.0f * (1.0f - powf(pressure_hpa / sea_level_hpa, 0.1903f));
}

bool initSensor() {
  if (!bmp.begin_I2C(BMP_ADDR)) return false;
  // Oversampling trades speed for lower noise; the IIR filter smooths
  // short pressure spikes (door slams, propwash) without extra code.
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(21, 22);
  haveSensor = initSensor();
  if (haveSensor) {
    Serial.println(F("[bmp390] found"));
  } else {
    Serial.println(F("[bmp390] NOT FOUND -- check wiring and address (0x76 / 0x77)"));
  }

  connectWiFi();
}

void loop() {
  static uint32_t lastPublish = 0;

  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  uint32_t now = millis();
  if (now - lastPublish < PUBLISH_INTERVAL_MS && lastPublish != 0) {
    delay(20);
    return;
  }
  lastPublish = now;

  char buf[160];
  if (!haveSensor) {
    snprintf(buf, sizeof(buf), "{\"error\":\"bmp390_not_found\",\"uptime_s\":%lu}",
             (unsigned long)(now / 1000UL));
    mqtt.publish(MQTT_TOPIC, buf);
    haveSensor = initSensor();   // retry in case it was hot-plugged
    return;
  }

  if (!bmp.performReading()) {
    Serial.println(F("[bmp390] read error"));
    return;
  }

  float tempC       = bmp.temperature;           // deg C
  float pressureHpa = bmp.pressure / 100.0f;     // Pa -> hPa
  float altitudeM   = pressureToAltitude(pressureHpa, (float)SEALEVEL_HPA);

  if (!haveBaseline) {
    baselineAlt = altitudeM;
    haveBaseline = true;
  }

  snprintf(buf, sizeof(buf),
           "{\"temp_c\":%.2f,\"pressure_hpa\":%.2f,\"altitude_m\":%.1f,\"rel_alt_m\":%.2f}",
           tempC, pressureHpa, altitudeM, altitudeM - baselineAlt);
  Serial.print(F("[mqtt] "));
  Serial.println(buf);
  mqtt.publish(MQTT_TOPIC, buf);
}

#include <bluefruit.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <Wire.h>
#include "SD.h"

#define DEBUG 1

// BLUETOOTH
BLEDfu bledfu;
BLEUart bleuart;

// ENVIRONMENT SENSOR
Adafruit_BME680 bme;

// GPS
SFE_UBLOX_GNSS g_myGNSS;
char timestamp[19];
long latitude;
long longitude;

// LOG FILE
File csvFile;
char msg[255]; // leaving some room

void setup() {
#if DEBUG
  Serial.begin(115200);
  // while (!Serial) { delay(100); }
  Serial.println("Starting Temperature/Humidity Logger...");
#endif
  led_init();
  sensor_init();
  gps_init();
  sd_init();
  ble_init();
  bme680_init();
#if DEBUG
  Serial.println("Latitude(°),Longitude(°),Temperature(°F),Humidity(%)");
#endif
}

void loop() {
  gps_get();
  bme680_get();
  log_data();
  delay(5000);
}

void led_init(void) {
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  for (uint8_t i = 0; i < 20; i++) {
    digitalToggle(LED_GREEN);
    delay(100);
    digitalToggle(LED_BLUE);
    delay(100);
  }
}

void sensor_init(void) {
  // I2C
  pinMode(WB_IO2, OUTPUT);
  digitalWrite(WB_IO2, HIGH);
  delay(1000);
  Wire.begin();
  delay(1000); // give em a sec to wake up
}

void gps_init(void) {
  if (!g_myGNSS.begin()) {
#if DEBUG
    Serial.println("ERROR: GPS not found.");
#endif
    return;
  } else {
    g_myGNSS.setI2COutput(COM_TYPE_UBX);
    g_myGNSS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT);
    // Wait on the GPS fix for accurate timestamps
#if DEBUG
    Serial.print("Searching for GPS...");
#endif
    while (g_myGNSS.getFixType() < 3) {
      digitalToggle(LED_GREEN);
      digitalToggle(LED_BLUE);
      delay(250);
#if DEBUG
      Serial.print(".");
#endif
    }
#if DEBUG
    Serial.println("GPS fix acquired.");
#endif
  }
}

void gps_get(void) {
  sprintf(timestamp,
          "%d-%02d-%02d,%02d:%02d:%02d",
          g_myGNSS.getYear(), g_myGNSS.getMonth(), g_myGNSS.getDay(),
          g_myGNSS.getHour(), g_myGNSS.getMinute(), g_myGNSS.getSecond());
  latitude = g_myGNSS.getLatitude();
  longitude = g_myGNSS.getLongitude();
}

void bme680_init(void) {
  bme.begin(0x76);
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
}

void sd_init(void) {
  if (SD.begin()) {
#if DEBUG
    Serial.println("SD Card mounted.\n");
#endif
    csvFile = SD.open("BME680.csv", FILE_WRITE);
    if (csvFile) { 
      if (csvFile.size() == 0) {
        csvFile.println("Date,Time,Latitude,Longitude,Temperature,Humidity");
        csvFile.flush();
      }
      return;
    }
#if DEBUG
    Serial.println("ERROR: Unable to create CSV file.");
#endif
  } else {
#if DEBUG
    Serial.println("ERROR: No SD Card found.\n");
#endif
  }
}

void ble_init(void) {
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.configPrphConn(92, BLE_GAP_EVENT_LENGTH_MIN, 16, 16);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("BME680 Logger");
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  bledfu.begin();
  bleuart.begin();
  startAdv();
}

void startAdv(void) {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void connect_callback(uint16_t conn_handle) {
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));
#if DEBUG
  Serial.print("Connected to ");
  Serial.println(central_name);
#endif
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void) conn_handle;
  (void) reason;
#if DEBUG
  Serial.print("Disconnected, reason = 0x");
  Serial.println(reason, HEX);
#endif
}

void bme680_get(void) {
  bme.performReading();
}

void log_data(void) {
  snprintf(msg,
           sizeof(msg),
           "%f,%f,%.1f,%.1f",
           latitude / 10000000.0,
           longitude / 10000000.0,
           bme.temperature * 1.8 + 32,
           bme.humidity);
  if (bleuart.notifyEnabled()) { bleuart.print(msg); }
  if (csvFile) {
    csvFile.print(timestamp);
    csvFile.print(",");
    csvFile.println(msg);
    csvFile.flush();
  }
#if DEBUG
  Serial.println(msg);
#endif
}

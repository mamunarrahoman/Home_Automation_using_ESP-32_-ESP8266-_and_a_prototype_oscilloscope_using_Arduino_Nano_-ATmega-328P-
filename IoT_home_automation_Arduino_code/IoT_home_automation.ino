#include "DHT.h"
#define DHTPIN 4
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#ifdef ENABLE_DEBUG
#define DEBUG_ESP_PORT Serial
#define NODEBUG_WEBSOCKETS
#define NDEBUG
#endif
#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32) || defined(ARDUINO_ARCH_RP2040)
#include <WiFi.h>
#endif
#include "SinricPro.h"
#include "SinricProSwitch.h"
#include <map>

#define WIFI_SSID "Marlboro"
#define WIFI_PASS "01753459428"
#define APP_KEY "56f20118-1684-41ae-a407-9530ec1292dc"                                          // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET "56aa1866-8453-4768-a4fa-3c89482f06ad-a7963507-eb7b-484e-835e-9d3db148d18e"  // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"

// comment the following line if you use a toggle switches instead of tactile buttons
#define TACTILE_BUTTON 1

#define BAUD_RATE 9600

#define DEBOUNCE_TIME 250

#if defined(ESP8266)
#define RELAYPIN_1 D1
#define RELAYPIN_2 D2
#define RELAYPIN_3 D3
#define RELAYPIN_4 D4
#define SWITCHPIN_1 D8
#define SWITCHPIN_2 D7
#define SWITCHPIN_3 D6
#define SWITCHPIN_4 D5
#elif defined(ESP32) || defined(ARDUINO_ARCH_RP2040)
#define LED_BUILTIN 2

#define RELAYPIN_1 12
#define RELAYPIN_2 13
#define RELAYPIN_3 18
#define RELAYPIN_4 19
#define SWITCHPIN_1 25
#define SWITCHPIN_2 26
#define SWITCHPIN_3 2
#define SWITCHPIN_4 33
#endif

typedef struct {  // struct for the std::map below
  int relayPIN;
  int flipSwitchPIN;
} deviceConfig_t;



// this is the main configuration
// please put in your deviceId, the PIN for Relay and PIN for flipSwitch
// this can be up to N devices...depending on how much pin's available on your device ;)
// right now we have 4 devicesIds going to 4 relays and 4 flip switches to switch the relay manually
std::map<String, deviceConfig_t> devices = {
  //{deviceId, {relayPIN,  flipSwitchPIN}}
  { "6512707fe53283d14e588ec3", { RELAYPIN_1, SWITCHPIN_1 } },
  { "651270d8e53283d14e588f3b", { RELAYPIN_2, SWITCHPIN_2 } },
  { "651270fa50caa73c8a7e0028", { RELAYPIN_3, SWITCHPIN_3 } },
  { "64b1c5f12ac6a1822a883389", { RELAYPIN_4, SWITCHPIN_4 } }
};

typedef struct {  // struct for the std::map below
  String deviceId;
  bool lastFlipSwitchState;
  unsigned long lastFlipSwitchChange;
} flipSwitchConfig_t;

std::map<int, flipSwitchConfig_t> flipSwitches;  // this map is used to map flipSwitch PINs to deviceId and handling debounce and last flipSwitch state checks
                                                 // it will be setup in "setupFlipSwitches" function, using informations from devices map

void setupRelays() {
  for (auto &device : devices) {            // for each device (relay, flipSwitch combination)
    int relayPIN = device.second.relayPIN;  // get the relay pin
    pinMode(relayPIN, OUTPUT);              // set relay pin to OUTPUT
  }
}

void setupFlipSwitches() {
  for (auto &device : devices) {          // for each device (relay / flipSwitch combination)
    flipSwitchConfig_t flipSwitchConfig;  // create a new flipSwitch configuration

    flipSwitchConfig.deviceId = device.first;      // set the deviceId
    flipSwitchConfig.lastFlipSwitchChange = 0;     // set debounce time
    flipSwitchConfig.lastFlipSwitchState = false;  // set lastFlipSwitchState to false (LOW)

    int flipSwitchPIN = device.second.flipSwitchPIN;  // get the flipSwitchPIN

    flipSwitches[flipSwitchPIN] = flipSwitchConfig;  // save the flipSwitch config to flipSwitches map
    pinMode(flipSwitchPIN, INPUT);                   // set the flipSwitch pin to INPUT
  }
}

bool onPowerState(String deviceId, bool &state) {
  Serial.printf("%s: %s\r\n", deviceId.c_str(), state ? "on" : "off");
  int relayPIN = devices[deviceId].relayPIN;  // get the relay pin for corresponding device
  digitalWrite(relayPIN, state);              // set the new relay state
  return true;
}

void handleFlipSwitches() {
  unsigned long actualMillis = millis();                                          // get actual millis
  for (auto &flipSwitch : flipSwitches) {                                         // for each flipSwitch in flipSwitches map
    unsigned long lastFlipSwitchChange = flipSwitch.second.lastFlipSwitchChange;  // get the timestamp when flipSwitch was pressed last time (used to debounce / limit events)

    if (actualMillis - lastFlipSwitchChange > DEBOUNCE_TIME) {  // if time is > debounce time...

      int flipSwitchPIN = flipSwitch.first;                              // get the flipSwitch pin from configuration
      bool lastFlipSwitchState = flipSwitch.second.lastFlipSwitchState;  // get the lastFlipSwitchState
      bool flipSwitchState = digitalRead(flipSwitchPIN);                 // read the current flipSwitch state
      if (flipSwitchState != lastFlipSwitchState) {                      // if the flipSwitchState has changed...
#ifdef TACTILE_BUTTON
        if (flipSwitchState) {  // if the tactile button is pressed
#endif
          flipSwitch.second.lastFlipSwitchChange = actualMillis;  // update lastFlipSwitchChange time
          String deviceId = flipSwitch.second.deviceId;           // get the deviceId from config
          int relayPIN = devices[deviceId].relayPIN;              // get the relayPIN from config
          bool newRelayState = !digitalRead(relayPIN);            // set the new relay State
          digitalWrite(relayPIN, newRelayState);                  // set the trelay to the new state

          SinricProSwitch &mySwitch = SinricPro[deviceId];  // get Switch device from SinricPro
          mySwitch.sendPowerStateEvent(newRelayState);      // send the event
#ifdef TACTILE_BUTTON
        }
#endif
        flipSwitch.second.lastFlipSwitchState = flipSwitchState;  // update lastFlipSwitchState
      }
    }
  }
}

void setupWiFi() {
  Serial.printf("\r\n[Wifi]: Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf(".");
    delay(250);
  }
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %s\r\n", WiFi.localIP().toString().c_str());
}

void setupSinricPro() {
  for (auto &device : devices) {
    const char *deviceId = device.first.c_str();
    SinricProSwitch &mySwitch = SinricPro[deviceId];
    mySwitch.onPowerState(onPowerState);
  }

  //SinricPro.restoreDeviceStates(true); // Uncomment to restore the last known state from the server.
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setup() {
  Serial.begin(BAUD_RATE);
  setupRelays();
  setupFlipSwitches();
  setupWiFi();
  setupSinricPro();
  dht.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
}

void loop() {
  SinricPro.handle();
  handleFlipSwitches();
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(25, 10);
  display.print("DHT-11 MODULE");
  display.setCursor(0, 25);
  display.print("TEMPERATURE : ");
  display.print(t);
  display.println("C");
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 40);
  display.print("HUMIDITY : ");
  display.print(h);
  display.println("%");
  display.display();
}
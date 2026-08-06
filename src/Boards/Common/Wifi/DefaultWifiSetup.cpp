#if defined(DEVICE_M5STAMPS3) || defined(DEVICE_S3DEVKIT) || defined(DEVICE_VISION_MASTER_T190) || defined(DEVICE_CUSTOM)

#include "Boards/Common/Wifi/DefaultWifiSetup.h"
#include <Preferences.h>
#include <WiFi.h>
#include <Arduino.h>

#define NVS_SSID_KEY "ssid"
#define NVS_PASS_KEY "pass"

#if LED_TYPE_RGB
#include <FastLED.h>
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];
#else
static void blink_led(int times, int delay_ms) {
    for (int i = 0; i < times; ++i) {
        digitalWrite(LED_PIN, HIGH);
        delay(delay_ms);
        digitalWrite(LED_PIN, LOW);
        delay(delay_ms);
    }
}
static void enable_led() {
    pinMode(LED_PIN, OUTPUT);
}
static void disable_led() {
    pinMode(LED_PIN, INPUT);
}
#endif

Preferences& getPreferences() {
    static Preferences preferences;
    return preferences;
}

bool loadWifiCredentials(String& ssid, String& password) {
    Preferences& preferences = getPreferences();
    preferences.begin("wifi_settings", true);  // readonly = true
    ssid = preferences.getString(NVS_SSID_KEY, "");
    password = preferences.getString(NVS_PASS_KEY, "");
    preferences.end();
    return !ssid.isEmpty() && !password.isEmpty();
}

bool setupDefaultWifi() {
    #if LED_TYPE_RGB
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    leds[0] = CRGB::White;
    FastLED.show();
    #else
    enable_led();
    blink_led(1, 500);
    delay(2000);
    #endif

    String ssid, password;
    if (!loadWifiCredentials(ssid, password)) {
        #if LED_TYPE_RGB
        leds[0] = CRGB::Blue;
        FastLED.show();
        delay(2000);
        FastLED.clear(true);
        #else
        blink_led(3, 166);
        delay(2000);
        disable_led();
        #endif
        return false;
    }

    WiFi.begin(ssid.c_str(), password.c_str());

    for (int i = 0; i < 15; ++i) {
        if (WiFi.status() == WL_CONNECTED) {
            #if LED_TYPE_RGB
            leds[0] = CRGB::Green;
            FastLED.show();
            delay(1000);
            FastLED.clear(true);
            #else
            blink_led(5, 125);
            disable_led();
            delay(2000);
            #endif
            return true;
        }
        delay(1000);
    }

    #if LED_TYPE_RGB
      leds[0] = CRGB::Red;
      FastLED.show();
      delay(1000);
      FastLED.clear(true);
    #else
      blink_led(10, 50);
      disable_led();
      delay(2000);
    #endif
    
    return false;
}

#endif

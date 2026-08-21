#include "wifi_setup.h"
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

void connectWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("[wifi_setup] connecting to ");
    Serial.print(WIFI_SSID);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("[wifi_setup] connected, IP: ");
    Serial.println(WiFi.localIP());
}

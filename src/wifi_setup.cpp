#include "wifi_setup.h"
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

namespace {
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;     // per network, before moving to the next
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;  // how often maintainWifi() tries again
constexpr size_t WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

unsigned long lastReconnectAttemptAt = 0;
size_t nextNetworkIndex = 0;
}  // namespace

bool connectWifi() {
    WiFi.mode(WIFI_STA);

    for (size_t i = 0; i < WIFI_NETWORK_COUNT; ++i) {
        const WifiCredential& net = WIFI_NETWORKS[i];
        Serial.printf("[wifi_setup] connecting to %s (%u/%u)...\n", net.ssid,
                      (unsigned)(i + 1), (unsigned)WIFI_NETWORK_COUNT);
        WiFi.begin(net.ssid, net.password);

        unsigned long attemptStart = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - attemptStart < WIFI_CONNECT_TIMEOUT_MS) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[wifi_setup] connected to %s, IP: %s\n", net.ssid,
                          WiFi.localIP().toString().c_str());
            nextNetworkIndex = (i + 1) % WIFI_NETWORK_COUNT;
            return true;
        }

        Serial.printf("[wifi_setup] %s timed out\n", net.ssid);
        WiFi.disconnect();
    }

    Serial.println("[wifi_setup] every configured network failed");
    return false;
}

void maintainWifi() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }
    unsigned long now = millis();
    if (now - lastReconnectAttemptAt < WIFI_RECONNECT_INTERVAL_MS) {
        return;
    }
    lastReconnectAttemptAt = now;

    const WifiCredential& net = WIFI_NETWORKS[nextNetworkIndex];
    Serial.printf("[wifi_setup] reconnecting: trying %s\n", net.ssid);
    WiFi.begin(net.ssid, net.password);  // async on ESP32 -- returns immediately
    nextNetworkIndex = (nextNetworkIndex + 1) % WIFI_NETWORK_COUNT;
}

#include "wifi_setup.h"
#include <Arduino.h>
#include <WiFi.h>
#include "device_config.h"

namespace {
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;     // per network, before moving to the next
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;  // how often maintainWifi() tries again
const unsigned long WIFI_POLL_MS = 100;                  // granularity of the wait (and abort check)

unsigned long lastReconnectAttemptAt = 0;
size_t nextNetworkIndex = 0;
}  // namespace

bool connectWifi(bool (*shouldAbort)()) {
    WiFi.mode(WIFI_STA);

    std::vector<WifiNetwork> networks = deviceConfig::wifiNetworks();
    for (size_t i = 0; i < networks.size(); ++i) {
        const WifiNetwork& net = networks[i];
        Serial.printf("[wifi_setup] connecting to %s (%u/%u)...\n", net.ssid.c_str(),
                      (unsigned)(i + 1), (unsigned)networks.size());
        WiFi.begin(net.ssid.c_str(), net.password.c_str());

        unsigned long attemptStart = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - attemptStart < WIFI_CONNECT_TIMEOUT_MS) {
            if (shouldAbort != nullptr && shouldAbort()) {
                Serial.println("[wifi_setup] aborted by caller");
                WiFi.disconnect();
                return false;
            }
            delay(WIFI_POLL_MS);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[wifi_setup] connected to %s, IP: %s\n", net.ssid.c_str(),
                          WiFi.localIP().toString().c_str());
            nextNetworkIndex = (i + 1) % networks.size();
            return true;
        }

        Serial.printf("[wifi_setup] %s timed out\n", net.ssid.c_str());
        WiFi.disconnect();
    }

    Serial.println("[wifi_setup] every saved network failed");
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

    std::vector<WifiNetwork> networks = deviceConfig::wifiNetworks();
    if (networks.empty()) {
        return;
    }
    const WifiNetwork& net = networks[nextNetworkIndex % networks.size()];
    Serial.printf("[wifi_setup] reconnecting: trying %s\n", net.ssid.c_str());
    WiFi.begin(net.ssid.c_str(), net.password.c_str());  // async on ESP32 -- returns immediately
    nextNetworkIndex = (nextNetworkIndex + 1) % networks.size();
}

bool joinNetwork(const String& ssid, const String& password, unsigned long timeoutMs,
                 String* failureReason) {
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long start = millis();
    wl_status_t status = WiFi.status();
    while (status != WL_CONNECTED && millis() - start < timeoutMs) {
        // These two are definitive answers from the radio; waiting out the rest of the timeout
        // would just leave someone staring at a spinner.
        if (status == WL_NO_SSID_AVAIL || status == WL_CONNECT_FAILED) {
            break;
        }
        delay(250);
        status = WiFi.status();
    }

    if (status == WL_CONNECTED) {
        return true;
    }

    if (failureReason != nullptr) {
        if (status == WL_NO_SSID_AVAIL) {
            *failureReason = "Network not found";
        } else if (status == WL_CONNECT_FAILED) {
            *failureReason = "Wrong password";
        } else {
            *failureReason = "Could not connect";
        }
    }
    WiFi.disconnect(false);  // stop the attempt, keep the hotspot up
    return false;
}

#pragma once

#include <Arduino.h>
#include <vector>

struct WifiNetwork {
    String ssid;
    String password;
};

// Everything a reader needs to know that used to be compiled into config.h, now kept in flash (NVS)
// so a manager can change it without a PC. config.h stays as the **fallback** for readers flashed
// before device pairing existed: saved values always win, and config.h values that are still the
// example placeholders count as "not configured" rather than as a network called "your-wifi-name".
namespace deviceConfig {

constexpr size_t MAX_WIFI_NETWORKS = 3;

// Networks to try, best first. Saved networks win outright: once one has been saved, the config.h
// list is ignored (so "Change WiFi" really does replace an old network rather than adding to it).
std::vector<WifiNetwork> wifiNetworks();
bool hasWifi();

// Saves a network as the preferred one: it goes first, an older entry with the same SSID is dropped,
// and only the newest MAX_WIFI_NETWORKS are kept.
void saveWifiNetwork(const String& ssid, const String& password);

String apiBaseUrl();   // saved, else API_BASE_URL from config.h
String apiKey();       // saved, else API_KEY from config.h, "" while it is still the placeholder
String companyId();    // saved, else COMPANY_ID from config.h, "" while it is still the placeholder
bool hasApiCredentials();
void saveApiCredentials(const String& baseUrl, const String& key, const String& companyId);

}  // namespace deviceConfig

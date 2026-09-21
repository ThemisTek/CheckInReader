#include "device_config.h"
#include <Preferences.h>
#include "config.h"

namespace {

const char* const NVS_NAMESPACE = "reader";
constexpr size_t LEGACY_WIFI_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

// The values config.h.example ships with. A config.h that still holds them means "nothing configured".
bool isPlaceholderSsid(const String& ssid) { return ssid.length() == 0 || ssid == "your-wifi-name"; }
bool isPlaceholderKey(const String& key) { return key.length() == 0 || key == "dsk_live_..."; }
bool isPlaceholderCompany(const String& id) {
    return id.length() == 0 || id == "00000000-0000-0000-0000-000000000000";
}

// NVS keys are limited to 15 characters: "ssid0", "pass2", "wifiCount", "companyId" all fit.
String slotKey(const char* prefix, size_t index) { return String(prefix) + String((unsigned)index); }

String readSaved(const char* key) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    String value = prefs.getString(key, "");
    prefs.end();
    return value;
}

std::vector<WifiNetwork> readSavedNetworks(Preferences& prefs) {
    std::vector<WifiNetwork> networks;
    uint8_t count = prefs.getUChar("wifiCount", 0);
    for (uint8_t i = 0; i < count && i < deviceConfig::MAX_WIFI_NETWORKS; ++i) {
        WifiNetwork net;
        net.ssid = prefs.getString(slotKey("ssid", i).c_str(), "");
        net.password = prefs.getString(slotKey("pass", i).c_str(), "");
        if (net.ssid.length() > 0) {
            networks.push_back(net);
        }
    }
    return networks;
}

}  // namespace

std::vector<WifiNetwork> deviceConfig::wifiNetworks() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    std::vector<WifiNetwork> networks = readSavedNetworks(prefs);
    prefs.end();
    if (!networks.empty()) {
        return networks;
    }

    for (size_t i = 0; i < LEGACY_WIFI_COUNT; ++i) {
        WifiNetwork net;
        net.ssid = WIFI_NETWORKS[i].ssid;
        net.password = WIFI_NETWORKS[i].password;
        if (!isPlaceholderSsid(net.ssid)) {
            networks.push_back(net);
        }
    }
    return networks;
}

bool deviceConfig::hasWifi() {
    return !wifiNetworks().empty();
}

void deviceConfig::saveWifiNetwork(const String& ssid, const String& password) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);

    std::vector<WifiNetwork> networks;
    networks.push_back({ssid, password});
    for (const WifiNetwork& existing : readSavedNetworks(prefs)) {
        if (existing.ssid != ssid && networks.size() < MAX_WIFI_NETWORKS) {
            networks.push_back(existing);
        }
    }

    prefs.putUChar("wifiCount", (uint8_t)networks.size());
    for (size_t i = 0; i < networks.size(); ++i) {
        prefs.putString(slotKey("ssid", i).c_str(), networks[i].ssid);
        prefs.putString(slotKey("pass", i).c_str(), networks[i].password);
    }
    prefs.end();
}

String deviceConfig::apiBaseUrl() {
    String saved = readSaved("apiUrl");
    return saved.length() > 0 ? saved : String(API_BASE_URL);
}

String deviceConfig::apiKey() {
    String saved = readSaved("apiKey");
    if (saved.length() > 0) {
        return saved;
    }
    String legacy = API_KEY;
    return isPlaceholderKey(legacy) ? String() : legacy;
}

String deviceConfig::companyId() {
    String saved = readSaved("companyId");
    if (saved.length() > 0) {
        return saved;
    }
    String legacy = COMPANY_ID;
    return isPlaceholderCompany(legacy) ? String() : legacy;
}

bool deviceConfig::hasApiCredentials() {
    return apiKey().length() > 0 && companyId().length() > 0;
}

void deviceConfig::saveApiCredentials(const String& baseUrl, const String& key, const String& companyId) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("apiUrl", baseUrl);
    prefs.putString("apiKey", key);
    prefs.putString("companyId", companyId);
    prefs.end();
}

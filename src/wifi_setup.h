#pragma once

#include <Arduino.h>

// Tries each saved network (device_config.h; config.h is only the fallback) in order, giving each up
// to WIFI_CONNECT_TIMEOUT_MS to connect before moving to the next. Returns true as soon as one
// succeeds, or false once every network has been tried once and none connected -- a single pass, not
// an infinite retry, so a caller that wants to keep trying (e.g. setup() looping this with a status
// message) decides that policy rather than it being hidden in here.
//
// `shouldAbort`, when given, is polled about ten times a second while waiting; returning true stops
// the pass immediately and returns false. That is how a long-press can reach the setup menu on a
// reader whose network has gone away, instead of it retrying forever with no way in.
bool connectWifi(bool (*shouldAbort)() = nullptr);

// Call every loop() tick. A no-op whenever WiFi is already connected. While disconnected, kicks
// off a reconnect to the next saved network at most once every WIFI_RECONNECT_INTERVAL_MS, cycling
// through the list the same way connectWifi() does -- so a dropped connection recovers on its own,
// including failing over to a backup network, without ever blocking the loop (WiFi.begin() on ESP32
// connects asynchronously in the background).
void maintainWifi();

// Tries ONE network, leaving the hotspot alone if it is running (WIFI_AP_STA): used by the setup
// portal to test what a person just typed. Returns true when connected. On failure fills
// `failureReason` (when given) with a sentence fit for a phone screen: "Network not found",
// "Wrong password", or a generic "Could not connect".
bool joinNetwork(const String& ssid, const String& password, unsigned long timeoutMs,
                 String* failureReason = nullptr);

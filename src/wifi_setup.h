#pragma once

// Tries each network in config.h's WIFI_NETWORKS in order, giving each up to
// WIFI_CONNECT_TIMEOUT_MS to connect before moving to the next. Returns true as soon as one
// succeeds, or false once every network in the list has been tried once and none connected --
// a single pass, not an infinite retry, so a caller that wants to keep trying (e.g. setup()
// looping this with a status message) decides that policy rather than it being hidden in here.
bool connectWifi();

// Call every loop() tick. A no-op whenever WiFi is already connected. While disconnected, kicks
// off a reconnect to the next network in WIFI_NETWORKS at most once every
// WIFI_RECONNECT_INTERVAL_MS, cycling through the list the same way connectWifi() does -- so a
// dropped connection recovers on its own, including failing over to a backup network, without
// ever blocking the loop (WiFi.begin() on ESP32 connects asynchronously in the background).
void maintainWifi();

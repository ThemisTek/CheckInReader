#include "device_pairing.h"
#include <M5Unified.h>
#include <WiFi.h>
#include "api_client.h"
#include "device_config.h"
#include "display.h"
#include "setup_screens.h"
#include "wifi_setup.h"

namespace {

const unsigned long POLL_INTERVAL_MS = 3000;
const unsigned long RETRY_DELAY_MS = 5000;
const unsigned long TOO_MANY_DELAY_MS = 30000;
const unsigned long PAIRED_DISPLAY_MS = 3000;

// The API returns six characters; a space in the middle is easier to read and to type.
String formatCode(const String& code) {
    if (code.length() == 6) {
        return code.substring(0, 3) + " " + code.substring(3);
    }
    return code;
}

// Waits, keeping WiFi alive and watching for a tap on `cancel`. True when it was tapped.
bool sleepOrCancel(unsigned long ms, const TouchRect& cancel) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        M5.update();
        maintainWifi();
        if (rectTapped(cancel)) {
            return true;
        }
        delay(20);
    }
    return false;
}

}  // namespace

PairingResult runDevicePairing(bool allowCancel) {
    waitForTouchRelease();

    while (true) {
        TouchRect cancel = showSetupNotice("Getting a pairing code...", allowCancel);
        DevicePairingStart start = startDevicePairing(WiFi.SSID());

        if (!start.ok) {
            bool tooMany = start.httpStatus == 429;
            cancel = showSetupNotice(
                tooMany ? "Too many codes are open. Waiting a few minutes..."
                        : "Can't reach the server. Retrying...",
                allowCancel);
            if (sleepOrCancel(tooMany ? TOO_MANY_DELAY_MS : RETRY_DELAY_MS, cancel)) {
                return PairingResult::Cancelled;
            }
            continue;
        }

        const unsigned long expiresAt = millis() + (unsigned long)start.expiresInSeconds * 1000UL;
        cancel = showPairingCodeScreen(formatCode(start.code), start.expiresInSeconds, allowCancel);

        unsigned long lastPollAt = 0;
        int shownSeconds = start.expiresInSeconds;
        while (millis() < expiresAt) {
            M5.update();
            maintainWifi();
            if (rectTapped(cancel)) {
                return PairingResult::Cancelled;
            }

            unsigned long now = millis();
            int secondsLeft = (int)((expiresAt - now) / 1000);
            if (secondsLeft != shownSeconds) {
                showPairingCountdown(secondsLeft);
                shownSeconds = secondsLeft;
            }

            if (now - lastPollAt >= POLL_INTERVAL_MS) {
                lastPollAt = now;
                DevicePairingPoll poll = pollDevicePairing(start.pollToken);

                if (poll.state == DevicePairingState::Claimed) {
                    deviceConfig::saveApiCredentials(deviceConfig::apiBaseUrl(), poll.apiKey, poll.companyId);
                    showMessage("Paired to " + poll.companyName + "\n" + poll.deviceName);
                    delay(PAIRED_DISPLAY_MS);
                    return PairingResult::Paired;
                }
                if (poll.state == DevicePairingState::Expired) {
                    break;  // the server forgot this code; ask for a new one
                }
            }
            delay(20);
        }
    }
}

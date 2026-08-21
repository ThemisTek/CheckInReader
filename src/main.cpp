#include <M5Unified.h>
#include "wifi_setup.h"
#include "nfc_reader.h"
#include "api_client.h"
#include "display.h"

namespace {
const unsigned long RESULT_DISPLAY_MS = 3000;
const unsigned long IDLE_REFRESH_MS = 60000;  // keeps the battery reading from going stale

String lastUid;
bool showingResult = false;
unsigned long lastScreenChangeAt = 0;
}  // namespace

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    showMessage("Connecting to WiFi...");
    connectWifi();

    if (!initNfcReader()) {
        showMessage("NFC unit not found");
        return;
    }

    showMessage("Ready");
    lastScreenChangeAt = millis();
}

void loop() {
    String uid = tryReadCard();

    if (uid.length() > 0) {
        // A card resting on/near the reader gets re-read every loop tick; only react to it once
        // per tap. Lifting the card (uid goes empty) clears this so tapping the same card again
        // later is treated as a fresh scan.
        if (uid != lastUid) {
            M5.Speaker.tone(2000,100);
            lastUid = uid;
            ScanResult result = scanCard(uid);
            String text = result.message;
            if (result.scannedCard.length() > 0) {
                text += "\nUID: " + result.scannedCard;
            }
            showMessage(text);
            showingResult = true;
            lastScreenChangeAt = millis();
        }
    } else {
        lastUid = "";
    }

    unsigned long sinceChange = millis() - lastScreenChangeAt;
    if (showingResult && sinceChange > RESULT_DISPLAY_MS) {
        showMessage("Ready");
        showingResult = false;
        lastScreenChangeAt = millis();
    } else if (!showingResult && sinceChange > IDLE_REFRESH_MS) {
        showMessage("Ready");
        lastScreenChangeAt = millis();
    }

    delay(200);
}

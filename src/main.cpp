#include <M5Unified.h>
#include "wifi_setup.h"
#include "nfc_reader.h"
#include "api_client.h"
#include "display.h"

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    // TODO: once connectWifi() is real, block here until connected.
    connectWifi();

    showMessage("Ready (stub)");
}

void loop() {
    String uid = tryReadCard();
    if (uid.length() > 0) {
        ScanResult result = scanCard(uid);
        showMessage(result.message);
    }
    delay(200);
}

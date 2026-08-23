#include <M5Unified.h>
#include "wifi_setup.h"
#include "nfc_reader.h"
#include "api_client.h"
#include "display.h"

namespace {
const unsigned long RESULT_DISPLAY_MS = 3000;       // outcomes with no Undo button
const unsigned long RESULT_WITH_UNDO_MS = 5000;      // longer so there's time to tap Undo
const unsigned long UNDONE_DISPLAY_MS = 1500;        // the brief "Undone" confirmation
const unsigned long IDLE_REFRESH_MS = 60000;         // keeps the battery reading from going stale
const unsigned long NFC_POLL_INTERVAL_MS = 150;      // I2C poll cadence for the NFC unit
const unsigned long LOOP_DELAY_MS = 10;              // just enough to yield to the watchdog/idle task

String lastUid;
bool showingResult = false;
bool showingUndone = false;
unsigned long lastScreenChangeAt = 0;
unsigned long lastNfcPollAt = 0;
unsigned long touchDownAt = 0;

ScanResult currentResult;
TouchRect undoButton;

void playOutcomeTone(ScanOutcome outcome) {
    switch (outcome) {
        case ScanOutcome::CheckedIn:
        case ScanOutcome::PairingBound:
            M5.Speaker.tone(1800, 80);
            delay(90);
            M5.Speaker.tone(2400, 100);
            break;
        case ScanOutcome::AlreadyCheckedIn:
            M5.Speaker.tone(2000, 60);
            delay(80);
            M5.Speaker.tone(2000, 60);
            break;
        case ScanOutcome::UnknownCard:
        case ScanOutcome::PairingAlreadyBound:
            M5.Speaker.tone(600, 200);
            break;
        case ScanOutcome::ChooseSession:
        case ScanOutcome::NoSession:
            M5.Speaker.tone(1200, 120);
            break;
        case ScanOutcome::Unknown:
        default:
            break;
    }
}

bool undoButtonTapped() {
    if (M5.Touch.getCount() == 0) {
        return false;
    }
    auto detail = M5.Touch.getDetail(0);
    if (detail.wasPressed()) {
        touchDownAt = millis();
    }
    if (undoButton.isEmpty() || !detail.wasClicked()) {
        return false;
    }
    // A tap whose press began before this screen was drawn belongs to whatever was on
    // screen at the time (e.g. a different scan's Undo button occupying the same spot on
    // a busy front-desk device) -- ignore it rather than attributing it to this screen.
    if (touchDownAt < lastScreenChangeAt) {
        return false;
    }
    return undoButton.contains(detail.x, detail.y);
}

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
    // Required for M5.Touch to refresh its state -- without this, getCount()/getDetail()
    // always report stale (or no) touches. This must run on (almost) every loop tick: the
    // touch chip only reports whether a finger is down *right now*, not a log of what
    // happened since the last check, so a tap that is fully pressed-and-released between two
    // calls to M5.update() is never seen at all. NFC polling is I2C round-trip time (tens of
    // ms) and used to sit directly in this loop too, which -- combined with the old flat
    // delay(200) -- made the loop slow enough for ordinary taps to fall entirely between polls.
    M5.update();

    if (showingResult && undoButtonTapped()) {
        bool undone = deleteAttendance(currentResult.attendanceId);
        showMessage(undone ? "Undone" : "Undo failed");
        showingResult = false;
        showingUndone = true;
        undoButton = TouchRect{};
        lastScreenChangeAt = millis();
        delay(LOOP_DELAY_MS);
        return;
    }

    // NFC is polled on its own timer, decoupled from the loop tick, so touch stays
    // responsive regardless of how long an I2C round-trip to the reader takes.
    unsigned long now = millis();
    if (now - lastNfcPollAt >= NFC_POLL_INTERVAL_MS) {
        lastNfcPollAt = now;
        String uid = tryReadCard();

        if (uid.length() > 0) {
            // A card resting on/near the reader gets re-read every poll; only react to it once
            // per tap. Lifting the card (uid goes empty) clears this so tapping the same card
            // again later is treated as a fresh scan.
            if (uid != lastUid) {
                lastUid = uid;
                currentResult = scanCard(uid);
                undoButton = showResult(currentResult);
                playOutcomeTone(currentResult.outcome);
                showingResult = true;
                showingUndone = false;
                lastScreenChangeAt = millis();
            }
        } else {
            lastUid = "";
        }
    }

    unsigned long sinceChange = millis() - lastScreenChangeAt;
    unsigned long resultTimeout = undoButton.isEmpty() ? RESULT_DISPLAY_MS : RESULT_WITH_UNDO_MS;

    if (showingResult && sinceChange > resultTimeout) {
        showMessage("Ready");
        showingResult = false;
        undoButton = TouchRect{};
        lastScreenChangeAt = millis();
    } else if (showingUndone && sinceChange > UNDONE_DISPLAY_MS) {
        showMessage("Ready");
        showingUndone = false;
        lastScreenChangeAt = millis();
    } else if (!showingResult && !showingUndone && sinceChange > IDLE_REFRESH_MS) {
        showMessage("Ready");
        lastScreenChangeAt = millis();
    }

    delay(LOOP_DELAY_MS);
}

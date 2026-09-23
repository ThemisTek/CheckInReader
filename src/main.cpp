#include <M5Unified.h>
#include "wifi_setup.h"
#include "nfc_reader.h"
#include "api_client.h"
#include "display.h"
#include "device_config.h"
#include "device_pairing.h"
#include "setup_menu.h"
#include "setup_portal.h"
#include "setup_screens.h"

namespace
{
    const unsigned long RESULT_DISPLAY_MS = 3000;   // outcomes with no Undo button
    const unsigned long RESULT_WITH_UNDO_MS = 5000; // longer so there's time to tap Undo
    const unsigned long UNDONE_DISPLAY_MS = 1500;   // the brief "Undone" confirmation
    const unsigned long IDLE_REFRESH_MS = 60000;    // keeps the battery reading from going stale
    const unsigned long NFC_POLL_INTERVAL_MS = 150; // I2C poll cadence for the NFC unit
    const unsigned long LOOP_DELAY_MS = 10;         // just enough to yield to the watchdog/idle task
    const int16_t TOUCH_HIT_PADDING = 20;           // forgiveness beyond the drawn UNDO button

    String lastUid;
    bool showingResult = false;
    bool showingUndone = false;
    unsigned long lastScreenChangeAt = 0;
    unsigned long lastNfcPollAt = 0;
    unsigned long touchDownAt = 0;
    bool undoButtonHeldVisually = false; // whether the button is currently drawn in its pressed style

    ScanResult currentResult;
    TouchRect undoButton;
    bool setupRequestedDuringBoot = false;

    void playOutcomeTone(ScanOutcome outcome)
    {
        M5.Speaker.setVolume(15);
        switch (outcome)
        {
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

    // Must be called every loop tick while a result with an Undo button is showing (not just on
    // release): it also drives the button's pressed/normal visual state and pauses the result
    // screen's auto-dismiss countdown for as long as a finger is down on it (see loop()).
    // Returns true exactly once, on the tick a full press-and-release on the button completes.
    bool undoButtonTapped()
    {
        if (undoButton.isEmpty())
        {
            return false;
        }
        if (M5.Touch.getCount() == 0)
        {
            if (undoButtonHeldVisually)
            {
                drawUndoButton(undoButton, false);
                undoButtonHeldVisually = false;
                lastScreenChangeAt = millis(); // give the countdown a fresh window after release
            }
            return false;
        }

        auto detail = M5.Touch.getDetail(0);
        if (detail.wasPressed())
        {
            touchDownAt = millis();
        }

        // A tap whose press began before this screen was drawn belongs to whatever was on screen
        // at the time (e.g. a different scan's Undo button occupying the same spot on a busy
        // front-desk device) -- ignore it rather than attributing it to this screen.
        bool belongsToThisScreen = touchDownAt >= lastScreenChangeAt;
        // Judge "inside the button" by where the finger FIRST touched down (base_x/base_y, latched
        // once on contact and never updated afterward) rather than the live position: M5Unified
        // reclassifies a touch as hold/flick/drag once it drifts past a threshold -- which a light
        // fingertip does routinely due to centroid noise -- and wasClicked() then never fires
        // (it's only true for the exact touch_end state). wasReleased() fires for every release
        // regardless of that reclassification, so it -- combined with the latched contact point --
        // is what makes any ordinary press-and-release register, not just a fast, dead-still one.
        bool downInsideButton = belongsToThisScreen &&
                                undoButton.containsPadded(detail.base_x, detail.base_y, TOUCH_HIT_PADDING);

        if (detail.isPressed() && downInsideButton)
        {
            if (!undoButtonHeldVisually)
            {
                drawUndoButton(undoButton, true);
                undoButtonHeldVisually = true;
            }
            return false; // still held; wait for release
        }

        if (undoButtonHeldVisually)
        {
            // Finger lifted, or dragged off the button and released elsewhere -- restore the
            // normal look and give the countdown a fresh window rather than one that may have
            // already run out while held (see the paused check in loop()).
            drawUndoButton(undoButton, false);
            undoButtonHeldVisually = false;
            lastScreenChangeAt = millis();
        }

        return detail.wasReleased() && downInsideButton;
    }

    // connectWifi() polls this while it waits, so a long-press reaches the setup menu even when the
    // saved network has gone away and the boot loop would otherwise retry forever.
    bool abortWifiForSetup()
    {
        M5.update();
        if (setupLongPressDetected())
        {
            setupRequestedDuringBoot = true;
            return true;
        }
        return false;
    }

    // The long-press menu. Returns when setup finishes or is cancelled; the caller redraws its own screen.
    void handleSetupRequest()
    {
        switch (runSetupMenu())
        {
        case SetupMenuChoice::ChangeWifi:
            runWifiSetupPortal(true);
            showMessage("Connecting to WiFi...");
            connectWifi(); // one pass; maintainWifi() keeps trying afterwards if it fails
            break;
        case SetupMenuChoice::RePair:
            runDevicePairing(true);
            break;
        case SetupMenuChoice::None:
            break;
        }
        waitForTouchRelease(); // the finger that pressed the last button must not linger into loop()
    }

    // First run and recovery. A reader with no WiFi opens its setup hotspot; one with WiFi but no
    // API key is waiting to be linked to a school and shows a pairing code. A reader flashed with a
    // complete config.h has both, so this reduces to the connect loop it always had.
    void ensureConfigured()
    {
        while (!deviceConfig::hasWifi())
        {
            runWifiSetupPortal(false);
        }

        showMessage("Connecting to WiFi...");
        // connectWifi() is a single pass over every saved network; retrying the whole pass here
        // (rather than that being hidden inside connectWifi() itself) is what turns "reached the end
        // of the list" into "start over from the top" instead of a dead end -- this device has
        // nothing useful to do without a network, so it's worth waiting for. The abort callback is
        // the way back to the setup menu when the network is not coming back.
        while (!connectWifi(abortWifiForSetup))
        {
            if (setupRequestedDuringBoot)
            {
                setupRequestedDuringBoot = false;
                handleSetupRequest();
                showMessage("Connecting to WiFi...");
            }
            else
            {
                showMessage("WiFi failed, retrying...");
            }
        }

        if (!deviceConfig::hasApiCredentials())
        {
            runDevicePairing(false);
        }
    }

} // namespace

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    initDisplayFont();

    ensureConfigured();

    if (!initNfcReader())
    {
        showMessage("NFC unit not found");
        return;
    }

    showMessage("Ready");
    lastScreenChangeAt = millis();
}

void loop()
{
    // Required for M5.Touch to refresh its state -- without this, getCount()/getDetail()
    // always report stale (or no) touches. This must run on (almost) every loop tick: the
    // touch chip only reports whether a finger is down *right now*, not a log of what
    // happened since the last check, so a tap that is fully pressed-and-released between two
    // calls to M5.update() is never seen at all. NFC polling is I2C round-trip time (tens of
    // ms) and used to sit directly in this loop too, which -- combined with the old flat
    // delay(200) -- made the loop slow enough for ordinary taps to fall entirely between polls.
    M5.update();
    // Non-blocking: a no-op whenever WiFi is up, so this doesn't affect touch/NFC responsiveness
    // during normal operation. Recovers a dropped connection (including failing over to a
    // backup network) without anyone having to power-cycle the device.
    maintainWifi();

    if (showingResult && undoButtonTapped())
    {
        // Acknowledge the tap immediately, before the blocking HTTPS round-trip: without this
        // the old result screen (button included) just sits there for the ~1s of the delete
        // request, which reads as the button not having registered at all -- prompting a
        // second, unnecessary tap.
        showMessage("Undoing...");
        bool undone = deleteAttendance(currentResult.attendanceId);
        showMessage(undone ? "Undone" : "Undo failed");
        showingResult = false;
        showingUndone = true;
        undoButton = TouchRect{};
        undoButtonHeldVisually = false;
        lastScreenChangeAt = millis();
        delay(LOOP_DELAY_MS);
        return;
    }

    // Setup is deliberate: hold the idle screen for SETUP_HOLD_MS. Only while idle, so it can never
    // collide with the Undo button or a result on screen.
    if (!showingResult && !showingUndone && setupLongPressDetected())
    {
        handleSetupRequest();
        showMessage("Ready");
        lastScreenChangeAt = millis();
        lastUid = ""; // a card resting on the reader is not a fresh tap
        return;
    }

    // NFC is polled on its own timer, decoupled from the loop tick, so touch stays
    // responsive regardless of how long an I2C round-trip to the reader takes.
    unsigned long now = millis();
    if (now - lastNfcPollAt >= NFC_POLL_INTERVAL_MS)
    {
        lastNfcPollAt = now;
        String uid = tryReadCard();

        if (uid.length() > 0)
        {
            // A card resting on/near the reader gets re-read every poll; only react to it once
            // per tap. Lifting the card (uid goes empty) clears this so tapping the same card
            // again later is treated as a fresh scan.
            if (uid != lastUid)
            {
                lastUid = uid;
                currentResult = scanCard(uid);
                undoButton = showResult(currentResult);
                undoButtonHeldVisually = false;
                playOutcomeTone(currentResult.outcome);
                showingResult = true;
                showingUndone = false;
                lastScreenChangeAt = millis();
            }
        }
        else
        {
            lastUid = "";
        }
    }

    unsigned long sinceChange = millis() - lastScreenChangeAt;
    unsigned long resultTimeout = undoButton.isEmpty() ? RESULT_DISPLAY_MS : RESULT_WITH_UNDO_MS;

    if (showingResult && undoButtonHeldVisually)
    {
        // Don't dismiss out from under a finger that's still on the button -- the countdown
        // resumes (with a fresh window) once undoButtonTapped() sees it lift, above.
    }
    else if (showingResult && sinceChange > resultTimeout)
    {
        showMessage("Ready");
        showingResult = false;
        undoButton = TouchRect{};
        undoButtonHeldVisually = false;
        lastScreenChangeAt = millis();
    }
    else if (showingUndone && sinceChange > UNDONE_DISPLAY_MS)
    {
        showMessage("Ready");
        showingUndone = false;
        lastScreenChangeAt = millis();
    }
    else if (!showingResult && !showingUndone && sinceChange > IDLE_REFRESH_MS)
    {
        showMessage("Ready");
        lastScreenChangeAt = millis();
    }

    delay(LOOP_DELAY_MS);
}

#include "setup_menu.h"
#include <M5Unified.h>
#include "setup_screens.h"
#include "wifi_setup.h"

namespace {
const unsigned long MENU_TIMEOUT_MS = 20000;
}

bool setupLongPressDetected() {
    static unsigned long pressStartedAt = 0;

    if (M5.Touch.getCount() == 0) {
        pressStartedAt = 0;
        return false;
    }
    auto detail = M5.Touch.getDetail(0);
    if (!detail.isPressed()) {
        pressStartedAt = 0;
        return false;
    }
    // A fresh press restarts the clock, so a touch that began before this function was being
    // called (the reader was showing a result) is not credited with time it never spent held here.
    if (pressStartedAt == 0 || detail.wasPressed()) {
        pressStartedAt = millis();
    }
    return millis() - pressStartedAt >= SETUP_HOLD_MS;
}

SetupMenuChoice runSetupMenu() {
    waitForTouchRelease();

    while (true) {
        SetupMenuButtons buttons = showSetupMenu();
        unsigned long shownAt = millis();
        SetupMenuChoice picked = SetupMenuChoice::None;

        while (picked == SetupMenuChoice::None) {
            M5.update();
            maintainWifi();
            if (rectTapped(buttons.changeWifi)) {
                picked = SetupMenuChoice::ChangeWifi;
            } else if (rectTapped(buttons.rePair)) {
                picked = SetupMenuChoice::RePair;
            } else if (rectTapped(buttons.cancel) || millis() - shownAt > MENU_TIMEOUT_MS) {
                return SetupMenuChoice::None;
            }
            delay(20);
        }

        const char* question = picked == SetupMenuChoice::ChangeWifi
            ? "Change this reader's WiFi? It stops checking students in until setup is finished."
            : "Link this reader to a school again? It stops checking students in until you finish pairing.";
        SetupConfirmButtons confirm = showSetupConfirm(question);
        unsigned long confirmShownAt = millis();
        waitForTouchRelease();

        while (true) {
            M5.update();
            maintainWifi();
            if (rectTapped(confirm.confirm)) {
                return picked;
            }
            if (rectTapped(confirm.back)) {
                break;  // back to the menu
            }
            if (millis() - confirmShownAt > MENU_TIMEOUT_MS) {
                return SetupMenuChoice::None;
            }
            delay(20);
        }
    }
}

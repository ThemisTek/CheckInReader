#pragma once

#include <Arduino.h>
#include "display.h"  // TouchRect

// ---- touch helpers -------------------------------------------------------------------------

// True on the one tick a finger lifts inside `rect` (padded, judged by where it first touched down --
// the same rule the Undo button uses, see main.cpp undoButtonTapped()). Call M5.update() first.
bool rectTapped(const TouchRect& rect);

// Blocks until no finger is down (capped at a few seconds). Call before an interactive loop: the
// finger that just long-pressed, or tapped the previous screen, must not count as a tap on this one.
void waitForTouchRelease();

// ---- drawing -------------------------------------------------------------------------------

// A filled rounded button with a centred label (size 2, or size 1 when the label would not fit).
TouchRect drawButton(int32_t x, int32_t y, int32_t w, int32_t h, const String& label, uint16_t fill);

// Hotspot setup: a WiFi-join QR on the left, instructions on the right, a status line below, and
// (when allowCancel) a Cancel button. Returns the Cancel button, or an empty rect.
TouchRect showWifiSetupScreen(const String& qrPayload, const String& networkName,
                              const String& status, bool allowCancel);

// A plain message, plus an optional Cancel button (returned, or an empty rect).
TouchRect showSetupNotice(const String& text, bool allowCancel);

// The pairing code, large, with the instruction to enter it in the web app. `code` is already
// formatted for reading ("K7M 4QX"). Returns the Cancel button, or an empty rect.
TouchRect showPairingCodeScreen(const String& code, int secondsLeft, bool allowCancel);
// Redraws only the "expires in" line, so the countdown does not flicker the whole screen.
void showPairingCountdown(int secondsLeft);

struct SetupMenuButtons {
    TouchRect changeWifi;
    TouchRect rePair;
    TouchRect cancel;
};
SetupMenuButtons showSetupMenu();

struct SetupConfirmButtons {
    TouchRect confirm;
    TouchRect back;
};
SetupConfirmButtons showSetupConfirm(const String& question);

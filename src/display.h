#pragma once

#include <Arduino.h>
#include "api_client.h"

// A screen-space rectangle. Used to report where showResult() drew the Undo button so
// main.cpp can hit-test touches against it without display.cpp and main.cpp sharing layout
// constants.
struct TouchRect {
    int16_t x = 0;
    int16_t y = 0;
    int16_t w = 0;
    int16_t h = 0;

    bool isEmpty() const { return w <= 0 || h <= 0; }
    bool contains(int16_t px, int16_t py) const {
        return !isEmpty() && px >= x && px < x + w && py >= y && py < y + h;
    }
    // Same test against a rect inflated by `pad` on every side. A real fingertip's reported
    // touch centroid routinely lands a few px outside the drawn control it was aimed at, so
    // hit-testing needs a bigger invisible target than the visible one (standard touch-UI
    // practice, not specific to this device).
    bool containsPadded(int16_t px, int16_t py, int16_t pad) const {
        return !isEmpty() && px >= x - pad && px < x + w + pad && py >= y - pad && py < y + h + pad;
    }
};

// Clears the screen and draws a plain message. Used for non-result states (connecting,
// ready, NFC-not-found, and the transient "Undone" confirmation).
void showMessage(const String& text);

// Clears the screen and renders a scan outcome: accent color, icon, the student's first
// name (when present, as the dominant element) and the message. Draws an UNDO button when
// result.attendanceId is non-empty and the outcome is CheckedIn or AlreadyCheckedIn, and
// returns its bounds. Returns an empty TouchRect (isEmpty() == true) when no button was drawn.
TouchRect showResult(const ScanResult& result);

// Redraws just the Undo button (as last positioned by showResult()) in its pressed or
// normal style, without touching the rest of the screen. Used to give immediate visual
// feedback for as long as a finger is down on it.
void drawUndoButton(const TouchRect& button, bool pressed);

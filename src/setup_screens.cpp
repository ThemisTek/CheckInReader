#include "setup_screens.h"
#include <M5Unified.h>

namespace {

const int32_t MARGIN = 10;
const int16_t TAP_PADDING = 20;                        // forgiveness beyond the drawn button
const unsigned long RELEASE_WAIT_CAP_MS = 3000;
const int32_t STATUS_Y = 166;                          // status line under the QR
const int32_t COUNTDOWN_Y = 174;                       // "expires in" line on the pairing screen

uint16_t greyFill() { return M5.Display.color565(60, 60, 60); }
uint16_t blueFill() { return M5.Display.color565(30, 100, 200); }
uint16_t redFill() { return M5.Display.color565(180, 40, 40); }

void beginScreen() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextDatum(top_left);
}

TouchRect drawCancelButton() {
    return drawButton(MARGIN, M5.Display.height() - 42, 110, 34, "Cancel", greyFill());
}

}  // namespace

bool rectTapped(const TouchRect& rect) {
    if (rect.isEmpty() || M5.Touch.getCount() == 0) {
        return false;
    }
    auto detail = M5.Touch.getDetail(0);
    return detail.wasReleased() && rect.containsPadded(detail.base_x, detail.base_y, TAP_PADDING);
}

void waitForTouchRelease() {
    unsigned long start = millis();
    while (millis() - start < RELEASE_WAIT_CAP_MS) {
        M5.update();
        if (M5.Touch.getCount() == 0) {
            return;
        }
        delay(10);
    }
}

TouchRect drawButton(int32_t x, int32_t y, int32_t w, int32_t h, const String& label, uint16_t fill) {
    M5.Display.fillRoundRect(x, y, w, h, 8, fill);
    M5.Display.drawRoundRect(x, y, w, h, 8, TFT_WHITE);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    if (M5.Display.textWidth(label) > w - 12) {
        M5.Display.setTextSize(1);
    }
    M5.Display.drawString(label, x + w / 2, y + h / 2);
    M5.Display.setTextDatum(top_left);

    TouchRect rect;
    rect.x = static_cast<int16_t>(x);
    rect.y = static_cast<int16_t>(y);
    rect.w = static_cast<int16_t>(w);
    rect.h = static_cast<int16_t>(h);
    return rect;
}

TouchRect showWifiSetupScreen(const String& qrPayload, const String& networkName,
                              const String& status, bool allowCancel) {
    beginScreen();

    // Version 4 (33 modules) holds the ~45-character WIFI: payload with room to spare.
    M5.Display.qrcode(qrPayload, 8, 8, 150, 4);

    M5.Display.setTextSize(1);
    const int32_t textX = 170;
    const char* lines[] = {"1. Scan this with", "   your phone camera", "2. Follow the page", "   that opens", "Network:"};
    int32_t y = 12;
    for (const char* line : lines) {
        M5.Display.drawString(line, textX, y);
        y += 20;
    }
    M5.Display.drawString(networkName, textX, y);

    M5.Display.fillRect(0, STATUS_Y, M5.Display.width(), 30, TFT_BLACK);
    M5.Display.setCursor(8, STATUS_Y);
    M5.Display.print(status);

    return allowCancel ? drawCancelButton() : TouchRect{};
}

TouchRect showSetupNotice(const String& text, bool allowCancel) {
    beginScreen();
    M5.Display.setTextSize(2);
    M5.Display.setCursor(MARGIN, MARGIN);
    M5.Display.println(text);
    return allowCancel ? drawCancelButton() : TouchRect{};
}

TouchRect showPairingCodeScreen(const String& code, int secondsLeft, bool allowCancel) {
    beginScreen();

    M5.Display.setTextSize(2);
    M5.Display.drawString("Pair this reader", MARGIN, MARGIN);

    M5.Display.setTextSize(1);
    M5.Display.drawString("In the web app: API keys > Add reader", MARGIN, 56);
    M5.Display.drawString("Then enter this code:", MARGIN, 76);

    M5.Display.setTextSize(4);
    M5.Display.setTextDatum(top_center);
    M5.Display.drawString(code, M5.Display.width() / 2, 104);
    M5.Display.setTextDatum(top_left);

    showPairingCountdown(secondsLeft);
    return allowCancel ? drawCancelButton() : TouchRect{};
}

void showPairingCountdown(int secondsLeft) {
    if (secondsLeft < 0) {
        secondsLeft = 0;
    }
    char text[24];
    snprintf(text, sizeof(text), "Expires in %d:%02d", secondsLeft / 60, secondsLeft % 60);

    M5.Display.fillRect(0, COUNTDOWN_Y, M5.Display.width(), 20, TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString(text, MARGIN, COUNTDOWN_Y);
}

SetupMenuButtons showSetupMenu() {
    beginScreen();
    M5.Display.setTextSize(2);
    M5.Display.drawString("Reader setup", MARGIN, 8);

    SetupMenuButtons buttons;
    buttons.changeWifi = drawButton(20, 52, 280, 50, "Change WiFi", blueFill());
    buttons.rePair = drawButton(20, 110, 280, 50, "Re-pair to school", blueFill());
    buttons.cancel = drawButton(20, 168, 280, 50, "Cancel", greyFill());
    return buttons;
}

SetupConfirmButtons showSetupConfirm(const String& question) {
    beginScreen();
    M5.Display.setTextSize(1);
    M5.Display.setCursor(MARGIN, 20);
    M5.Display.println(question);

    SetupConfirmButtons buttons;
    buttons.back = drawButton(10, 170, 145, 50, "Back", greyFill());
    buttons.confirm = drawButton(165, 170, 145, 50, "Continue", redFill());
    return buttons;
}

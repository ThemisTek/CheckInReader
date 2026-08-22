#include "display.h"
#include <M5Unified.h>

namespace {

struct OutcomeStyle {
    uint16_t color;
    void (*drawIcon)(int32_t x, int32_t y);  // draws into a ~40x40 box at (x, y)
};

void drawCheckmark(int32_t x, int32_t y) {
    M5.Display.drawLine(x, y + 20, x + 14, y + 34, TFT_WHITE);
    M5.Display.drawLine(x + 1, y + 20, x + 15, y + 34, TFT_WHITE);
    M5.Display.drawLine(x + 14, y + 34, x + 36, y + 6, TFT_WHITE);
    M5.Display.drawLine(x + 14, y + 35, x + 36, y + 7, TFT_WHITE);
}

void drawCross(int32_t x, int32_t y) {
    M5.Display.drawLine(x, y, x + 36, y + 36, TFT_WHITE);
    M5.Display.drawLine(x + 1, y, x + 37, y + 36, TFT_WHITE);
    M5.Display.drawLine(x, y + 36, x + 36, y, TFT_WHITE);
    M5.Display.drawLine(x + 1, y + 36, x + 37, y, TFT_WHITE);
}

void drawInfo(int32_t x, int32_t y) {
    M5.Display.drawCircle(x + 18, y + 18, 18, TFT_WHITE);
    M5.Display.fillCircle(x + 18, y + 9, 2, TFT_WHITE);
    M5.Display.fillRect(x + 16, y + 15, 4, 14, TFT_WHITE);
}

void drawQuestion(int32_t x, int32_t y) {
    M5.Display.drawCircle(x + 18, y + 18, 18, TFT_WHITE);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("?", x + 18, y + 18);
    M5.Display.setTextDatum(top_left);
}

void drawNone(int32_t, int32_t) {}

// Draws `text` centered at (cx, y) using the largest text size in [1, maxSize] that fits
// within maxWidth (measured via M5.Display.textWidth() at each candidate size). Falls back
// to size 1 if even that doesn't fit -- a best-effort minimum rather than silently clipping.
// Returns the size actually used, so the caller can space a following line correctly.
int32_t drawFittedString(const String& text, int32_t cx, int32_t y, int32_t maxWidth, int32_t maxSize) {
    int32_t size = maxSize;
    for (; size > 1; --size) {
        M5.Display.setTextSize(size);
        if (M5.Display.textWidth(text) <= maxWidth) {
            break;
        }
    }
    M5.Display.setTextSize(size);
    M5.Display.drawString(text, cx, y);
    return size;
}

OutcomeStyle styleFor(ScanOutcome outcome) {
    switch (outcome) {
        case ScanOutcome::CheckedIn:
            return OutcomeStyle{M5.Display.color565(0, 150, 70), drawCheckmark};
        case ScanOutcome::PairingBound:
            return OutcomeStyle{M5.Display.color565(0, 140, 140), drawCheckmark};
        case ScanOutcome::AlreadyCheckedIn:
            return OutcomeStyle{M5.Display.color565(30, 100, 200), drawInfo};
        case ScanOutcome::ChooseSession:
        case ScanOutcome::NoSession:
            return OutcomeStyle{M5.Display.color565(210, 140, 0), drawQuestion};
        case ScanOutcome::UnknownCard:
        case ScanOutcome::PairingAlreadyBound:
            return OutcomeStyle{TFT_RED, drawCross};
        case ScanOutcome::Unknown:
        default:
            return OutcomeStyle{M5.Display.color565(80, 80, 80), drawNone};
    }
}

const int32_t RESULT_BAND_H = 56;
const int32_t ICON_MARGIN_X = 10;
const int32_t ICON_MARGIN_Y = 8;
const int32_t TEXT_TOP_MARGIN = 16;
const int32_t TEXT_SIDE_MARGIN = 10;
const int32_t UNDO_BTN_W = 120;
const int32_t UNDO_BTN_H = 44;
const int32_t UNDO_BTN_MARGIN = 10;

}  // namespace

void showMessage(const String& text) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(top_left);
    M5.Display.setCursor(10, 10);
    M5.Display.println(text);

    // Bottom-right corner, well clear of a short 1-2 line status message up top.
    int32_t battery = M5.Power.getBatteryLevel();
    if (battery >= 0) {
        M5.Display.setTextSize(1);
        M5.Display.setCursor(M5.Display.width() - 70, M5.Display.height() - 16);
        M5.Display.printf("Batt %ld%%", (long)battery);
    }
}

TouchRect showResult(const ScanResult& result) {
    OutcomeStyle style = styleFor(result.outcome);
    int32_t screenW = M5.Display.width();
    int32_t screenH = M5.Display.height();

    M5.Display.fillScreen(TFT_BLACK);

    M5.Display.fillRect(0, 0, screenW, RESULT_BAND_H, style.color);
    style.drawIcon(ICON_MARGIN_X, ICON_MARGIN_Y);

    int32_t textY = RESULT_BAND_H + TEXT_TOP_MARGIN;
    int32_t maxTextWidth = screenW - 2 * TEXT_SIDE_MARGIN;
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextDatum(top_center);

    if (result.studentFirstName.length() > 0) {
        drawFittedString(result.studentFirstName, screenW / 2, textY, maxTextWidth, 4);
        textY += M5.Display.fontHeight() + 6;
        drawFittedString(result.message, screenW / 2, textY, maxTextWidth, 2);
    } else {
        drawFittedString(result.message, screenW / 2, textY, maxTextWidth, 2);
    }
    M5.Display.setTextDatum(top_left);

    TouchRect button;
    bool offersUndo = result.attendanceId.length() > 0 &&
                       (result.outcome == ScanOutcome::CheckedIn ||
                        result.outcome == ScanOutcome::AlreadyCheckedIn);
    if (offersUndo) {
        button.w = UNDO_BTN_W;
        button.h = UNDO_BTN_H;
        button.x = screenW - UNDO_BTN_W - UNDO_BTN_MARGIN;
        button.y = screenH - UNDO_BTN_H - UNDO_BTN_MARGIN;

        M5.Display.fillRoundRect(button.x, button.y, button.w, button.h, 8,
                                  M5.Display.color565(60, 60, 60));
        M5.Display.drawRoundRect(button.x, button.y, button.w, button.h, 8, TFT_WHITE);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setTextSize(2);
        M5.Display.drawString("UNDO", button.x + button.w / 2, button.y + button.h / 2);
        M5.Display.setTextDatum(top_left);
    }

    return button;
}

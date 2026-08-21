#include "display.h"
#include <M5Unified.h>

void showMessage(const String& text) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextSize(2);
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

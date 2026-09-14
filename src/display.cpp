#include "display.h"
#include <M5Unified.h>
#include <U8g2lib.h>
#include <vector>

namespace {

// M5GFX's default font only covers ASCII, so Greek text (student names, API messages) was
// rendering as blank glyphs. Unifont is a bitmap font bundled with the U8g2 library; this
// "_t_greek" build is a 241-glyph subset of it -- ASCII plus the full modern Greek block
// (U+0370-U+03FF, so accented vowels like a tonos are included) -- at a fixed 16px glyph
// height. It plugs into M5GFX as an ordinary IFont via the lgfx::U8g2font adapter, so the
// existing setTextSize()/textWidth()/fontHeight() calls throughout this file are unaffected.
const lgfx::U8g2font greekFont(u8g2_font_unifont_t_greek);

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
// Also returns the size actually used: it's left set on M5.Display when this returns, so a
// caller can space a following line with M5.Display.fontHeight() without a second call.
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

std::vector<String> splitWords(const String& text) {
    std::vector<String> words;
    int32_t start = 0;
    int32_t len = text.length();
    while (start < len) {
        while (start < len && text[start] == ' ') ++start;
        int32_t end = start;
        while (end < len && text[end] != ' ') ++end;
        if (end > start) {
            words.push_back(text.substring(start, end));
        }
        start = end;
    }
    return words;
}

// Splits a single `word` into pieces that each fit within maxWidth at the currently-set text
// size, breaking between characters rather than words -- the fallback for a name with no
// spaces in it at all (the common case) that's still too wide for one line. Advances by whole
// UTF-8 code points so a multi-byte Greek character is never split across two pieces.
std::vector<String> splitWordToFit(const String& word, int32_t maxWidth) {
    std::vector<String> pieces;
    String current;
    uint32_t i = 0;
    while (i < word.length()) {
        uint8_t lead = static_cast<uint8_t>(word[i]);
        uint32_t charLen = 1;
        if ((lead & 0xE0) == 0xC0) charLen = 2;
        else if ((lead & 0xF0) == 0xE0) charLen = 3;
        else if ((lead & 0xF8) == 0xF0) charLen = 4;
        String ch = word.substring(i, i + charLen);
        String candidate = current + ch;
        if (current.length() > 0 && M5.Display.textWidth(candidate) > maxWidth) {
            pieces.push_back(current);
            current = ch;
        } else {
            current = candidate;
        }
        i += charLen;
    }
    if (current.length() > 0) {
        pieces.push_back(current);
    }
    return pieces;
}

// Greedily packs `words` into lines no wider than maxWidth at the currently-set text size.
// A word that alone exceeds maxWidth (a name with no spaces, which is the usual case, at a
// size too big for it) is hard-wrapped character-by-character via splitWordToFit() rather
// than left overflowing on its own line: leaving it overflowing defeated the whole point of
// wrapping (drawWrappedString picks the largest size whose *wrap* fits maxLines, so an
// always-one-line "wrap" for a single-word name meant that check never caught an oversized
// line and never shrank the font).
std::vector<String> packLines(const std::vector<String>& words, int32_t maxWidth) {
    std::vector<String> lines;
    String current;
    for (const String& word : words) {
        if (M5.Display.textWidth(word) > maxWidth) {
            if (current.length() > 0) {
                lines.push_back(current);
                current = "";
            }
            for (const String& piece : splitWordToFit(word, maxWidth)) {
                lines.push_back(piece);
            }
            continue;
        }
        String candidate = current.length() == 0 ? word : current + " " + word;
        if (current.length() > 0 && M5.Display.textWidth(candidate) > maxWidth) {
            lines.push_back(current);
            current = word;
        } else {
            current = candidate;
        }
    }
    if (current.length() > 0) {
        lines.push_back(current);
    }
    return lines;
}

// Word-wraps `text` into at most maxLines centered lines, at the largest size in
// [1, maxSize] whose wrap fits within maxLines -- same shrink-to-fit contract as
// drawFittedString() above, but spread across lines instead of squeezed onto one, which is
// what let a long name run off the edge of the screen. Relies on packLines() actually
// reporting more lines once a line stops fitting maxWidth (including a hard, mid-word wrap --
// see packLines()) for this size-shrink loop to have anything to react to. Falls back to
// however many lines it takes at size 1 if even that doesn't fit in maxLines, same
// "best effort, never clip" reasoning as drawFittedString(). Returns the total height drawn
// so the caller can position what follows below it.
int32_t drawWrappedString(const String& text, int32_t cx, int32_t y, int32_t maxWidth,
                           int32_t maxSize, int32_t maxLines) {
    std::vector<String> words = splitWords(text);
    std::vector<String> lines;
    int32_t size = maxSize;
    for (; size > 1; --size) {
        M5.Display.setTextSize(size);
        lines = packLines(words, maxWidth);
        if (static_cast<int32_t>(lines.size()) <= maxLines) {
            break;
        }
    }
    if (size == 1) {
        M5.Display.setTextSize(1);
        lines = packLines(words, maxWidth);
    }

    const int32_t lineSpacing = 4;
    int32_t lineHeight = M5.Display.fontHeight() + lineSpacing;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        M5.Display.drawString(lines[i], cx, y + static_cast<int32_t>(i) * lineHeight);
    }
    return lines.empty() ? 0 : static_cast<int32_t>(lines.size()) * lineHeight - lineSpacing;
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

void drawUndoButton(const TouchRect& button, bool pressed) {
    // Pressed = inverted (white fill/black text) rather than a shade of the same grey, so the
    // state is unmistakable even on a small, sun-washed, or low-brightness screen.
    uint16_t fill = pressed ? TFT_WHITE : M5.Display.color565(60, 60, 60);
    uint16_t text = pressed ? TFT_BLACK : TFT_WHITE;

    M5.Display.fillRoundRect(button.x, button.y, button.w, button.h, 8, fill);
    M5.Display.drawRoundRect(button.x, button.y, button.w, button.h, 8, TFT_WHITE);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(text);
    M5.Display.drawString("UNDO", button.x + button.w / 2, button.y + button.h / 2);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextColor(TFT_WHITE);
}

void initDisplayFont() {
    M5.Display.setFont(&greekFont);
}

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
        // Wrapped rather than a single shrink-to-fit line: a long name (long first name,
        // or occasionally more than just a first name) was otherwise running off the edge
        // of the screen at the smallest allowed size instead of just taking a second line.
        textY += drawWrappedString(result.studentFirstName, screenW / 2, textY, maxTextWidth, 4, 2);
        textY += 6;
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
        drawUndoButton(button, false);
    }

    return button;
}

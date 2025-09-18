#include <Tiny4kOLED.h>
#include "zxpix_font.h"

#define FONTZXPIX (&TinyOLEDFontzxpix)
const DCfont *currentFont = FONTZXPIX;

#include <string.h>
#include <stdio.h>

// make sure you have a font pointer defined somewhere:
// extern const DCfont* currentFont;

class OLEDSerial
{
    public:

    static const uint8_t COLS = 10;            // characters per text column
    static const uint8_t SCREEN_PX_H = 32;     // display height in pixels (64x32 display -> 32)
    static const uint8_t SCREEN_PAGES = SCREEN_PX_H / 8;
    static const uint8_t MAX_ROWS = 8;         // safety upper bound

    void begin(unsigned long baud) {
        (void)baud; // unused, API compat

        // initialize OLED (your preferred init)
        oled.begin(64, 32, sizeof(tiny4koled_init_64x32r), tiny4koled_init_64x32r);
        oled.enableChargePump();
        oled.setFont(currentFont);
        oled.clear();
        oled.switchRenderFrame();
        oled.on();

        // compute font metrics
        fontHeightPx = (currentFont && currentFont->height) ? currentFont->height : 8; // pixels
        fontPages = (fontHeightPx + 7) / 8; // pages per text row (1 page == 8 px)
        rows = SCREEN_PAGES / fontPages;
        if (rows > MAX_ROWS) rows = MAX_ROWS;

        clearBuffer();
        redraw();
    }

    // basic print overloads
    void print(const char* msg) {
        putStringNoRedraw(msg);
        redraw();
    }

    void print(const String &s) {
        print(s.c_str());
    }

    void print(int num) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", num);
        print(buf);
    }

    void println(const char* msg = "") {
        putStringNoRedraw(msg);
        newline();
        redraw();
    }

    void println(const String &s) {
        println(s.c_str());
    }

    void println(int num) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", num);
        println(buf);
    }

private:
    char buffer[MAX_ROWS][COLS + 1]; // +1 for NUL
    uint8_t cursorRow = 0;
    uint8_t cursorCol = 0;
    uint8_t rows = 4;          // actual usable rows (computed in begin)
    uint8_t fontHeightPx = 8;  // pixels
    uint8_t fontPages = 1;     // pages per text row

    void clearBuffer() {
        for (uint8_t r = 0; r < MAX_ROWS; r++) {
            memset(buffer[r], ' ', COLS);
            buffer[r][COLS] = '\0';
        }
        cursorRow = 0;
        cursorCol = 0;
    }

    void putStringNoRedraw(const char* s) {
        while (*s) putChar(*s++);
    }

    void putChar(char c) {
        if (c == '\r') return;         // ignore CR
        if (c == '\n') { newline(); return; }

        // wrap if at end of column
        if (cursorCol >= COLS) {
            newline();
        }

        // if somehow outside rows (safety) — scroll
        if (cursorRow >= rows) {
            scrollUp();
            cursorRow = rows - 1;
            cursorCol = 0;
        }

        buffer[cursorRow][cursorCol++] = c;
    }

    void newline() {
        cursorRow++;
        cursorCol = 0;
        if (cursorRow >= rows) {
            scrollUp();
            cursorRow = rows - 1;
        }
    }

    void scrollUp()
    {
        // shift everything up 1 logical text row
        for (uint8_t r = 1; r < rows; r++) {
            memcpy(buffer[r - 1], buffer[r], COLS + 1);
        }
        // clear last row
        memset(buffer[rows - 1], ' ', COLS);
        buffer[rows - 1][COLS] = '\0';
    }

    void redraw() {
        oled.clear();
        // setCursor expects Y as page index (0..SCREEN_PAGES-1),
        // so multiply text-row index by fontPages.
        for (uint8_t r = 0; r < rows; r++) {
            uint8_t pageIndex = r * fontPages;
            oled.setCursor(0, pageIndex);
            oled.print(buffer[r]);
        }
        oled.switchFrame();
    }
};
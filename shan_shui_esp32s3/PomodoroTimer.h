#pragma once
#include <Arduino.h>
#include <cmath>
#include <algorithm>
#include "Canvas565.h"

// ============================================================================
// PomodoroTimer: Minimalist Desk Ambient Art Focus Timer
// - 20-minute flow session with smooth procedural sidescrolling
// - Starts paused (still landscape art piece) -> Tap to start focus flow
// - Preserves timer across art style changes (Shan-Shui <-> Mario)
// - Gentle gradual slowdown to a halt upon 20-minute completion
// - High-end artistic completion indicators:
//     * Shan-Shui Mode: Vermilion Chinese/Korean Seal [ 朱印 落款: 完/休 20m ]
//       with subtle golden vignette breathing pulse
//     * Mario Mode: 16-bit Gold Stage Clear emblem [ ★ 20 MIN CLEAR! ★ ]
// - 2px ambient hairline progress bar along the very top edge
// ============================================================================

#define POMO_WORK_DURATION_MS (20 * 60 * 1000UL) // 20 Minutes (1,200,000 ms)

enum PomodoroState {
    POMO_IDLE_READY = 0, // Still landscape piece at boot. Tap to begin 20m session
    POMO_RUNNING    = 1, // Flowing landscape. Timer actively counting 0 -> 20m
    POMO_PAUSED     = 2, // Manually paused by tap. Timer halted, tap to resume
    POMO_FINISHED   = 3  // 20m reached! Smoothly stopped with artistic completion seal
};

// Compact 5x7 ASCII bitmap font (characters 32 ' ' through 90 'Z')
static const uint8_t FONT_5X7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 ' '
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // 33 '!'
    {0x00, 0x07, 0x00, 0x07, 0x00}, // 34 '"'
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // 35 '#'
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // 36 '$'
    {0x23, 0x13, 0x08, 0x64, 0x62}, // 37 '%'
    {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 '&'
    {0x00, 0x05, 0x03, 0x00, 0x00}, // 39 '''
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // 40 '('
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // 41 ')'
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // 42 '*'
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // 43 '+'
    {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ','
    {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 '-'
    {0x00, 0x60, 0x60, 0x00, 0x00}, // 46 '.'
    {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 '/'
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 48 '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 49 '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 51 '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 52 '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 54 '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 57 '9'
    {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 ':'
    {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ';'
    {0x08, 0x14, 0x22, 0x41, 0x00}, // 60 '<'
    {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 '='
    {0x00, 0x41, 0x22, 0x14, 0x08}, // 62 '>'
    {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 '?'
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // 64 '@'
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 65 'A'
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 66 'B'
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 67 'C'
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 68 'D'
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 69 'E'
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 70 'F'
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 71 'G'
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 72 'H'
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 73 'I'
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 74 'J'
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 75 'K'
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 76 'L'
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 77 'M'
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 78 'N'
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 79 'O'
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 80 'P'
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 81 'Q'
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 82 'R'
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 83 'S'
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 84 'T'
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 85 'U'
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 86 'V'
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 87 'W'
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 'X'
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 89 'Y'
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 'Z'
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // 91 '['
    {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 '\'
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // 93 ']'
    {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 '^'
    {0x40, 0x40, 0x40, 0x40, 0x40}  // 95 '_'
};

class PomodoroTimer {
public:
    PomodoroState state = POMO_IDLE_READY; // Starts still (IDLE) at power-on
    uint32_t elapsedMs = 0;
    uint32_t lastTickMs = 0;
    float currentSpeed = 0.0f; // 0.0 when IDLE/PAUSED/FINISHED, targetSpeed when RUNNING
    float targetSpeed = 1.5f;

    // Call on power-on or manual reset
    void init() {
        state = POMO_IDLE_READY;
        elapsedMs = 0;
        currentSpeed = 0.0f;
        lastTickMs = millis();
    }

    // Touch tap handler: toggles IDLE/PAUSE/RESUME/RESTART
    void handleTap() {
        uint32_t now = millis();
        switch (state) {
            case POMO_IDLE_READY:
                // Start 20-minute focus session!
                state = POMO_RUNNING;
                elapsedMs = 0;
                lastTickMs = now;
                currentSpeed = targetSpeed;
                break;
            case POMO_RUNNING:
                // Pause session
                state = POMO_PAUSED;
                currentSpeed = 0.0f;
                break;
            case POMO_PAUSED:
                // Resume session
                state = POMO_RUNNING;
                lastTickMs = now;
                currentSpeed = targetSpeed;
                break;
            case POMO_FINISHED:
                // Reset and start next 20m focus sprint!
                state = POMO_RUNNING;
                elapsedMs = 0;
                lastTickMs = now;
                currentSpeed = targetSpeed;
                break;
        }
    }

    // Updates timer state per frame
    void update(float targetStyleSpeed) {
        targetSpeed = targetStyleSpeed;
        uint32_t now = millis();

        if (state == POMO_RUNNING) {
            uint32_t dt = now - lastTickMs;
            lastTickMs = now;
            elapsedMs += dt;
            currentSpeed = targetSpeed;

            if (elapsedMs >= POMO_WORK_DURATION_MS) {
                elapsedMs = POMO_WORK_DURATION_MS;
                state = POMO_FINISHED;
            }
        } else if (state == POMO_FINISHED) {
            // Gentle gradual glide deceleration to 0
            if (currentSpeed > 0.02f) {
                currentSpeed -= 0.04f;
                if (currentSpeed < 0.0f) currentSpeed = 0.0f;
            } else {
                currentSpeed = 0.0f;
            }
        } else {
            currentSpeed = 0.0f;
            lastTickMs = now;
        }
    }

    // ------------------------------------------------------------------------
    // Framebuffer Drawing Primitives (Zero-flicker DMA overlays)
    // ------------------------------------------------------------------------

    // Draw single character into frameBuf
    static void drawChar(uint16_t* buf, int x, int y, char c, uint16_t col, int scale = 1) {
        if (c < 32 || c > 95) {
            if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
            else c = ' ';
        }
        int idx = c - 32;
        for (int colIdx = 0; colIdx < 5; colIdx++) {
            uint8_t line = FONT_5X7[idx][colIdx];
            for (int rowIdx = 0; rowIdx < 7; rowIdx++) {
                if (line & (1 << rowIdx)) {
                    for (int sy = 0; sy < scale; sy++) {
                        int py = y + rowIdx * scale + sy;
                        if (py < 0 || py >= 240) continue;
                        for (int sx = 0; sx < scale; sx++) {
                            int px = x + colIdx * scale + sx;
                            if (px >= 0 && px < 320) {
                                buf[py * 320 + px] = col;
                            }
                        }
                    }
                }
            }
        }
    }

    // Draw string into frameBuf
    static void drawString(uint16_t* buf, int x, int y, const char* str, uint16_t col, int scale = 1) {
        int cx = x;
        while (*str) {
            drawChar(buf, cx, y, *str, col, scale);
            cx += 6 * scale;
            str++;
        }
    }

    // Semi-transparent rounded pill box helper
    static void drawPill(uint16_t* buf, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint16_t borderCol) {
        for (int cy = y; cy < y + h; cy++) {
            if (cy < 0 || cy >= 240) continue;
            uint16_t* row = &buf[cy * 320];
            for (int cx = x; cx < x + w; cx++) {
                if (cx < 0 || cx >= 320) continue;
                bool isBorder = (cy == y || cy == y + h - 1 || cx == x || cx == x + w - 1);
                if (isBorder) {
                    row[cx] = borderCol;
                } else {
                    row[cx] = Canvas565::blend565(row[cx], r, g, b, a);
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // Aesthetic Overlays for Each State
    // ------------------------------------------------------------------------

    void render(uint16_t* buf, int artStyle) {
        bool isMario = (artStyle == 1);

        // 1. Top 2-pixel ambient progress hairline (0% -> 100%)
        float progress = (float)elapsedMs / (float)POMO_WORK_DURATION_MS;
        if (progress > 1.0f) progress = 1.0f;
        int barW = (int)(progress * 320.0f);

        uint16_t barCol = isMario ? Canvas565::to565(255, 215, 0)   // Mario Star Gold
                                  : Canvas565::to565(190, 42, 28);   // Shan-Shui Vermilion Seal Red
        for (int y = 0; y < 2; y++) {
            uint16_t* row = &buf[y * 320];
            for (int x = 0; x < barW; x++) {
                row[x] = barCol;
            }
        }

        // 2. State-specific Overlays
        switch (state) {
            case POMO_IDLE_READY: {
                // Subtle bottom badge: [ 20m FOCUS | TAP TO START ]
                uint8_t pulse = 180 + (uint8_t)(55.0f * (0.5f + 0.5f * sinf(millis() * 0.004f)));
                if (!isMario) {
                    // Shan-Shui rice-paper pill
                    drawPill(buf, 75, 208, 170, 20, 245, 235, 215, pulse, Canvas565::to565(90, 50, 40));
                    drawString(buf, 86, 214, "20M FOCUS | TAP TO START", Canvas565::to565(60, 30, 20), 1);
                } else {
                    // Mario retro pill
                    drawPill(buf, 75, 176, 170, 20, 0, 0, 0, pulse, Canvas565::to565(248, 184, 0));
                    drawString(buf, 86, 182, "> 20M FOCUS | TAP TO START", Canvas565::to565(255, 255, 255), 1);
                }
                break;
            }

            case POMO_PAUSED: {
                // Subtle center-top pause pill: || PAUSED (14:20 / 20:00)
                uint32_t remSec = (POMO_WORK_DURATION_MS > elapsedMs) ? (POMO_WORK_DURATION_MS - elapsedMs) / 1000 : 0;
                char timeStr[32];
                snprintf(timeStr, sizeof(timeStr), "|| PAUSED  %02u:%02u REMAIN", remSec / 60, remSec % 60);

                if (!isMario) {
                    drawPill(buf, 65, 12, 190, 22, 245, 235, 215, 225, Canvas565::to565(120, 60, 40));
                    drawString(buf, 78, 18, timeStr, Canvas565::to565(80, 20, 10), 1);
                } else {
                    drawPill(buf, 65, 12, 190, 22, 0, 0, 0, 210, Canvas565::to565(248, 184, 0));
                    drawString(buf, 78, 18, timeStr, Canvas565::to565(248, 248, 248), 1);
                }
                break;
            }

            case POMO_FINISHED: {
                // 20 Minutes Complete! Sophisticated artistic completion!
                if (!isMario) {
                    // --- Shan-Shui: Traditional Vermilion Seal [ 落款: 完 20m ] ---
                    // Deep cinnabar red double seal stamped at top-right (x: 258, y: 12)
                    int sx = 258, sy = 12, sw = 50, sh = 42;
                    uint16_t redBorder = Canvas565::to565(184, 32, 20); // Authentic seal red
                    drawPill(buf, sx, sy, sw, sh, 170, 28, 18, 235, redBorder);
                    // Inner seal frame
                    for (int cy = sy + 2; cy < sy + sh - 2; cy++) {
                        buf[cy * 320 + (sx + 2)] = Canvas565::to565(245, 220, 200);
                        buf[cy * 320 + (sx + sw - 3)] = Canvas565::to565(245, 220, 200);
                    }
                    for (int cx = sx + 2; cx < sx + sw - 2; cx++) {
                        buf[(sy + 2) * 320 + cx] = Canvas565::to565(245, 220, 200);
                        buf[(sy + sh - 3) * 320 + cx] = Canvas565::to565(245, 220, 200);
                    }
                    // Calligraphy inscription: "20m" & "REST" in white
                    drawString(buf, sx + 14, sy + 8, "20m", Canvas565::to565(255, 248, 240), 1);
                    drawString(buf, sx + 11, sy + 22, "REST", Canvas565::to565(255, 248, 240), 1);

                    // Soft golden vignette breathing along screen borders
                    float breathe = 0.5f + 0.5f * sinf(millis() * 0.0035f);
                    uint8_t vgAlpha = (uint8_t)(35.0f * breathe);
                    for (int y = 0; y < 8; y++) {
                        uint16_t* row = &buf[y * 320];
                        for (int x = 0; x < 320; x++) row[x] = Canvas565::blend565(row[x], 255, 220, 140, vgAlpha);
                    }
                    for (int y = 232; y < 240; y++) {
                        uint16_t* row = &buf[y * 320];
                        for (int x = 0; x < 320; x++) row[x] = Canvas565::blend565(row[x], 255, 220, 140, vgAlpha);
                    }

                    // Bottom completion prompt: [ 20 MIN COMPLETE - REST & TAP ]
                    drawPill(buf, 55, 206, 210, 22, 245, 235, 215, 230, redBorder);
                    drawString(buf, 66, 212, "20 MIN COMPLETE | TAP FOR NEXT", Canvas565::to565(120, 20, 10), 1);

                } else {
                    // --- Super Mario World: 16-bit Gold Stage Clear Emblem ---
                    int bx = 50, by = 26, bw = 220, bh = 46;
                    uint16_t goldBorder = Canvas565::to565(248, 184, 0);
                    drawPill(buf, bx, by, bw, bh, 0, 0, 0, 230, goldBorder);
                    // Gold double corner rivets
                    buf[(by + 3) * 320 + (bx + 3)] = 0xFFFF;
                    buf[(by + 3) * 320 + (bx + bw - 4)] = 0xFFFF;
                    buf[(by + bh - 4) * 320 + (bx + 3)] = 0xFFFF;
                    buf[(by + bh - 4) * 320 + (bx + bw - 4)] = 0xFFFF;

                    drawString(buf, bx + 22, by + 10, "* 20 MIN COMPLETE! *", Canvas565::to565(255, 216, 0), 1);
                    drawString(buf, bx + 36, by + 26, "REST & TAP FOR NEXT", Canvas565::to565(255, 255, 255), 1);
                }
                break;
            }

            case POMO_RUNNING:
            default:
                // Distraction-free ambient art flow! Only the delicate 2px hairline at top.
                break;
        }
    }
};

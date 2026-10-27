#pragma once
#include <Arduino.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include "Canvas565.h"
#include "ShanShuiEngine.h"

// ============================================================================
// MarioWorldEngine: Super Mario World (SNES) Iconic 16-bit Landscape
// Procedurally generated endless scrolling stage:
// - Beautiful vibrant Mario Cyan Sky
// - Fluffy white smiling cartoon clouds
// - Rounded green hills with eyes (도트 눈 달린 둥근 초록 동산)
// - Classic Checkered Soil & Green Grass Ground (체크무늬 흙바닥 & 잔디)
// - Green Warp Pipes (초록색 파이프 토관)
// - Floating Question Mark [?] & Yellow/Brown Brick Blocks (물음표 블록, 벽돌)
// - Colorful Mushroom Platforms (기둥 달린 버섯 발판)
// - Bushes with eyes
// ============================================================================

class MarioWorldEngine {
public:
    ShanShuiEngine* base;
    Canvas565* canvas = nullptr;

    MarioWorldEngine(ShanShuiEngine* b) : base(b) {}

    // 16-bit SNES Super Mario World Color Palette (RGB565 converted)
    static const ColorRGBA SKY_BLUE;      // 0x5D7F: #5C94FC (맑은 마리오 하늘색)
    static const ColorRGBA GRASS_LIGHT;   // 0x4726: #40D830 (밝은 초록 잔디)
    static const ColorRGBA GRASS_DARK;    // 0x1CE0: #108800 (어두운 잔디 테두리)
    static const ColorRGBA DIRT_LIGHT;    // 0xDE68: #D89840 (주황빛 황토 흙)
    static const ColorRGBA DIRT_DARK;     // 0xA340: #A06000 (갈색 흙 경계)
    static const ColorRGBA PIPE_GREEN;    // 0x0600: #00C000 (파이프 밝은 초록)
    static const ColorRGBA PIPE_DARK;     // 0x0380: #007000 (파이프 그림자)
    static const ColorRGBA BLOCK_GOLD;    // 0xFCE0: #F8B800 (물음표 블록 노랑)
    static const ColorRGBA BLOCK_BROWN;   // 0xB2C0: #B05800 (벽돌색)
    static const ColorRGBA CLOUD_WHITE;   // 0xFFFF: #FFFFFF (순백 구름)
    static const ColorRGBA OUTLINE_BLACK; // 0x0000: #000000 (카툰 외곽선)

    // Helper: Screen-space fast filled rectangle
    void fillRect(int x, int y, int w, int h, const ColorRGBA& col) {
        if (!canvas) return;
        int x0 = std::max(0, x), x1 = std::min(canvas->width - 1, x + w - 1);
        int y0 = std::max(0, y), y1 = std::min(canvas->height - 1, y + h - 1);
        if (x0 > x1 || y0 > y1) return;
        uint8_t a = (uint8_t)(col.a * 255.0f);
        for (int cy = y0; cy <= y1; cy++) {
            canvas->blendHLine(x0, x1, cy, col.r, col.g, col.b, a, false);
        }
    }

    // Helper: Two black oval eyes with cute highlights (마리오 특유의 타원형 눈)
    void drawMarioEyes(int cx, int cy, int spacing = 12, int h = 10) {
        for (int dir : {-1, 1}) {
            int ex = cx + dir * (spacing / 2);
            // Black eye
            fillRect(ex - 2, cy - h / 2, 4, h, OUTLINE_BLACK);
            // White shine dot at top
            fillRect(ex - 1, cy - h / 2 + 1, 2, 3, CLOUD_WHITE);
        }
    }

    // 1. Classic Mario Fluffy Smiling Cloud
    void drawCloud(float wx, float wy, int bumps = 3) {
        Pt s = canvas->toScreen(Pt(wx, wy));
        int cx = (int)s.x, cy = (int)s.y;
        if (cx < -120 || cx > canvas->width + 120) return;

        int w = bumps * 24 + 16;
        int h = 26;

        // Cloud body shadow & white filling
        fillRect(cx - w / 2, cy, w, h, CLOUD_WHITE);
        for (int i = 0; i < bumps; i++) {
            int bx = cx - (bumps - 1) * 12 + i * 24;
            base->blob(wx + (i - bumps * 0.5f) * 60.0f, wy - 30.0f, 32.0f, 22.0f, 0, CLOUD_WHITE, 0.1f, false);
        }
        // Outline & Bottom border
        fillRect(cx - w / 2, cy + h - 2, w, 2, ColorRGBA(180, 210, 255));
        
        // Cute smiling eyes
        drawMarioEyes(cx, cy + 8, 14, 8);
    }

    // 2. Iconic Super Mario Rounded Hills with Eyes (동글동글한 초록 동산)
    void drawMarioHill(float wx, float wy, float wid, float hei, bool hasEyes = true) {
        int segs = 24;
        std::vector<Pt> curve;
        for (int i = 0; i <= segs; i++) {
            float p = (float)i / segs;
            float x = (p - 0.5f) * wid;
            float y = -sinf(p * (float)M_PI) * hei;
            curve.push_back(Pt(x, y));
        }
        std::vector<Pt> body = curve;
        body.push_back(Pt(wid * 0.5f, 100.0f));
        body.push_back(Pt(-wid * 0.5f, 100.0f));

        // Vibrant Green Hill Fill
        base->poly(body, wx, wy, GRASS_LIGHT, ColorRGBA(0, 0, 0, 0), 0, false);
        // Thick cartoon outline
        base->stroke(curve, wx, wy, OUTLINE_BLACK, 3.5f);

        // Spot shading stripes on hill crest
        for (int i = 4; i < segs - 4; i += 4) {
            base->blob(wx + curve[i].x, wy + curve[i].y + 14.0f, 16.0f, 8.0f, 0, GRASS_DARK, 0.2f, false);
        }

        // Distinctive Mario pair of eyes on the hill!
        if (hasEyes) {
            Pt s = canvas->toScreen(Pt(wx, wy - hei * 0.55f));
            drawMarioEyes((int)s.x, (int)s.y, 14, 11);
        }
    }

    // 3. Classic Green Warp Pipe (초록색 토관)
    void drawPipe(float wx, float wy, int heightPx = 55) {
        Pt s = canvas->toScreen(Pt(wx, wy));
        int px = (int)s.x, py = (int)s.y;
        if (px < -60 || px > canvas->width + 60) return;

        int pw = 36;
        int rimW = 44, rimH = 16;
        int topY = py - heightPx;

        // Pipe Rim (Lip)
        fillRect(px - rimW / 2, topY, rimW, rimH, PIPE_GREEN);
        fillRect(px - rimW / 2 + 3, topY + 2, 6, rimH - 4, ColorRGBA(120, 240, 100)); // highlight
        fillRect(px + rimW / 2 - 10, topY + 2, 8, rimH - 4, PIPE_DARK);              // shadow
        // Rim border
        fillRect(px - rimW / 2, topY, rimW, 2, OUTLINE_BLACK);
        fillRect(px - rimW / 2, topY + rimH - 1, rimW, 2, OUTLINE_BLACK);
        fillRect(px - rimW / 2, topY, 2, rimH, OUTLINE_BLACK);
        fillRect(px + rimW / 2 - 2, topY, 2, rimH, OUTLINE_BLACK);

        // Pipe Body
        int bodyY = topY + rimH;
        int bodyH = heightPx - rimH;
        fillRect(px - pw / 2, bodyY, pw, bodyH, PIPE_GREEN);
        fillRect(px - pw / 2 + 3, bodyY, 5, bodyH, ColorRGBA(120, 240, 100)); // highlight
        fillRect(px + pw / 2 - 9, bodyY, 7, bodyH, PIPE_DARK);               // shadow
        // Body border
        fillRect(px - pw / 2, bodyY, 2, bodyH, OUTLINE_BLACK);
        fillRect(px + pw / 2 - 2, bodyY, 2, bodyH, OUTLINE_BLACK);
    }

    // 4. Floating Question Block [?] or Brick Block
    void drawBlock(float wx, float wy, bool isQuestion = true) {
        Pt s = canvas->toScreen(Pt(wx, wy));
        int bx = (int)s.x - 10, by = (int)s.y - 10;
        int sz = 20;
        if (bx < -30 || bx > canvas->width + 30) return;

        if (isQuestion) {
            fillRect(bx, by, sz, sz, BLOCK_GOLD);
            // Black border
            fillRect(bx, by, sz, 1, OUTLINE_BLACK);
            fillRect(bx, by + sz - 1, sz, 1, OUTLINE_BLACK);
            fillRect(bx, by, 1, sz, OUTLINE_BLACK);
            fillRect(bx + sz - 1, by, 1, sz, OUTLINE_BLACK);
            // White highlight corner rivets
            fillRect(bx + 2, by + 2, 2, 2, CLOUD_WHITE);
            fillRect(bx + sz - 4, by + 2, 2, 2, DIRT_DARK);
            fillRect(bx + 2, by + sz - 4, 2, 2, DIRT_DARK);
            fillRect(bx + sz - 4, by + sz - 4, 2, 2, DIRT_DARK);
            // White Question Mark [?]
            fillRect(bx + 7, by + 4, 6, 2, CLOUD_WHITE);
            fillRect(bx + 11, by + 6, 2, 4, CLOUD_WHITE);
            fillRect(bx + 9, by + 10, 3, 2, CLOUD_WHITE);
            fillRect(bx + 9, by + 14, 2, 2, CLOUD_WHITE);
        } else {
            // Brick Block
            fillRect(bx, by, sz, sz, BLOCK_BROWN);
            fillRect(bx, by, sz, 1, OUTLINE_BLACK);
            fillRect(bx, by + sz - 1, sz, 1, OUTLINE_BLACK);
            fillRect(bx, by, 1, sz, OUTLINE_BLACK);
            fillRect(bx + sz - 1, by, 1, sz, OUTLINE_BLACK);
            // Mortar brick lines
            fillRect(bx, by + 9, sz, 2, OUTLINE_BLACK);
            fillRect(bx + 9, by, 2, 9, OUTLINE_BLACK);
            fillRect(bx + 4, by + 10, 2, 9, OUTLINE_BLACK);
            fillRect(bx + 14, by + 10, 2, 9, OUTLINE_BLACK);
        }
    }

    // 5. Mushroom Platform (버섯 기둥 발판)
    void drawMushroomPlatform(float wx, float wy, float wid = 70.0f) {
        Pt s = canvas->toScreen(Pt(wx, wy));
        int mx = (int)s.x, my = (int)s.y;
        int stemW = 12;
        int stemH = 70;
        // White stem
        fillRect(mx - stemW / 2, my, stemW, stemH, CLOUD_WHITE);
        fillRect(mx - stemW / 2, my, 2, stemH, OUTLINE_BLACK);
        fillRect(mx + stemW / 2 - 2, my, 2, stemH, OUTLINE_BLACK);

        // Orange/Red Mushroom Cap
        int capW = (int)(wid * 0.45f);
        int capH = 22;
        int cy = my - capH + 4;
        fillRect(mx - capW / 2, cy, capW, capH, ColorRGBA(248, 88, 0));
        // White spots on cap
        fillRect(mx - capW / 4, cy + 4, 8, 8, CLOUD_WHITE);
        fillRect(mx + capW / 4 - 8, cy + 4, 8, 8, CLOUD_WHITE);
        // Border
        fillRect(mx - capW / 2, cy, capW, 2, OUTLINE_BLACK);
        fillRect(mx - capW / 2, cy + capH - 1, capW, 2, OUTLINE_BLACK);
        fillRect(mx - capW / 2, cy, 2, capH, OUTLINE_BLACK);
        fillRect(mx + capW / 2 - 2, cy, 2, capH, OUTLINE_BLACK);
    }

    // 6. Mario Ground Floor: Checkered Soil & Crisp Green Grass Layer
    void drawGround(int sliceX, int sliceW) {
        if (!canvas) return;
        int groundY = canvas->height - 42; // Bottom 42 pixels

        // Checkered dirt blocks
        for (int y = groundY + 8; y < canvas->height; y += 16) {
            for (int x = sliceX; x < sliceX + sliceW; x += 16) {
                bool chk = ((x / 16) + (y / 16)) % 2 == 0;
                fillRect(x, y, 16, 16, chk ? DIRT_LIGHT : DIRT_DARK);
                // Grid border
                fillRect(x, y, 16, 1, ColorRGBA(120, 60, 0));
                fillRect(x, y, 1, 16, ColorRGBA(120, 60, 0));
            }
        }

        // Top Grass Lip (초록 잔디 테두리)
        fillRect(sliceX, groundY, sliceW, 8, GRASS_LIGHT);
        fillRect(sliceX, groundY + 7, sliceW, 2, GRASS_DARK);
        fillRect(sliceX, groundY, sliceW, 2, OUTLINE_BLACK);
    }
};

// Static color constants for Mario World
const ColorRGBA MarioWorldEngine::SKY_BLUE      = ColorRGBA(92, 148, 252); // #5C94FC
const ColorRGBA MarioWorldEngine::GRASS_LIGHT   = ColorRGBA(64, 216, 48);  // #40D830
const ColorRGBA MarioWorldEngine::GRASS_DARK    = ColorRGBA(16, 136, 0);   // #108800
const ColorRGBA MarioWorldEngine::DIRT_LIGHT    = ColorRGBA(216, 152, 64); // #D89840
const ColorRGBA MarioWorldEngine::DIRT_DARK     = ColorRGBA(160, 96, 0);   // #A06000
const ColorRGBA MarioWorldEngine::PIPE_GREEN    = ColorRGBA(0, 192, 0);    // #00C000
const ColorRGBA MarioWorldEngine::PIPE_DARK     = ColorRGBA(0, 112, 0);    // #007000
const ColorRGBA MarioWorldEngine::BLOCK_GOLD    = ColorRGBA(248, 184, 0);  // #F8B800
const ColorRGBA MarioWorldEngine::BLOCK_BROWN   = ColorRGBA(176, 88, 0);   // #B05800
const ColorRGBA MarioWorldEngine::CLOUD_WHITE   = ColorRGBA(255, 255, 255);
const ColorRGBA MarioWorldEngine::OUTLINE_BLACK = ColorRGBA(0, 0, 0);

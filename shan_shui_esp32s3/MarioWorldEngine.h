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
// - Beautiful vibrant Mario Cyan Sky (#70C0FF)
// - Fluffy white smiling cartoon clouds with crisp black outlines & cute eyes
// - Rounded green rolling hills with eyes (도트 눈 달린 둥근 초록 동산)
// - Classic Warm Checkered Soil & Green Grass Ground (체크무늬 흙바닥 & 잔디)
// - Firmly planted Green Warp Pipes (땅에 완벽히 밀착된 초록색 토관)
// - Floating Question Mark [?] & Yellow/Brown Brick Blocks (물음표 블록, 벽돌)
// - Colorful Mushroom Platforms (기둥 달린 버섯 발판)
// ============================================================================

class MarioWorldEngine {
public:
    ShanShuiEngine* base;
    Canvas565* canvas = nullptr;

    MarioWorldEngine(ShanShuiEngine* b) : base(b) {}

    // 16-bit SNES Super Mario World Color Palette
    static const ColorRGBA SKY_BLUE;      // #70C0FF (맑고 화사한 닌텐도 하늘색)
    static const ColorRGBA GRASS_LIGHT;   // #48E030 (밝은 초록 잔디)
    static const ColorRGBA GRASS_DARK;    // #109000 (어두운 잔디 테두리)
    static const ColorRGBA DIRT_LIGHT;    // #E4A048 (따뜻한 황토빛 흙)
    static const ColorRGBA DIRT_DARK;     // #B87830 (갈색 흙 경계)
    static const ColorRGBA PIPE_GREEN;    // #00C800 (파이프 밝은 초록)
    static const ColorRGBA PIPE_DARK;     // #007800 (파이프 그림자)
    static const ColorRGBA BLOCK_GOLD;    // #F8B800 (물음표 블록 노랑)
    static const ColorRGBA BLOCK_BROWN;   // #B05800 (벽돌색)
    static const ColorRGBA CLOUD_WHITE;   // #FFFFFF (순백 구름)
    static const ColorRGBA OUTLINE_BLACK; // #000000 (카툰 외곽선)

    // Helper: Screen-space fast filled rectangle
    void fillRect(int x, int y, int w, int h, const ColorRGBA& col) {
        if (!canvas || w <= 0 || h <= 0) return;
        int x0 = std::max(0, x), x1 = std::min(canvas->width - 1, x + w - 1);
        int y0 = std::max(0, y), y1 = std::min(canvas->height - 1, y + h - 1);
        if (x0 > x1 || y0 > y1) return;
        uint16_t c565 = Canvas565::to565(col.r, col.g, col.b);
        for (int cy = y0; cy <= y1; cy++) {
            uint16_t* row = &canvas->buf[cy * canvas->width];
            for (int cx = x0; cx <= x1; cx++) {
                row[cx] = c565;
            }
        }
    }

    // Helper: Screen-space fast filled circle
    void fillCircle(int cx, int cy, int r, const ColorRGBA& col) {
        if (!canvas || r <= 0) return;
        uint16_t c565 = Canvas565::to565(col.r, col.g, col.b);
        int r2 = r * r;
        for (int dy = -r; dy <= r; dy++) {
            int y = cy + dy;
            if (y < 0 || y >= canvas->height) continue;
            int dx = (int)sqrtf((float)(r2 - dy * dy));
            int x0 = std::max(0, cx - dx);
            int x1 = std::min(canvas->width - 1, cx + dx);
            uint16_t* row = &canvas->buf[y * canvas->width];
            for (int x = x0; x <= x1; x++) {
                row[x] = c565;
            }
        }
    }

    // Helper: Two black oval eyes with cute highlights (마리오 특유의 타원형 눈)
    void drawMarioEyes(int cx, int cy, int spacing = 12, int h = 9) {
        for (int dir : {-1, 1}) {
            int ex = cx + dir * (spacing / 2);
            // Black vertical pill eye
            fillRect(ex - 1, cy - h / 2, 3, h, OUTLINE_BLACK);
            // White shine dot at top
            fillRect(ex, cy - h / 2 + 1, 1, 2, CLOUD_WHITE);
        }
    }

    // 1. Classic Mario Fluffy Smiling Cloud (Crisp circular lobes with black outline)
    void drawCloud(float wx, float wy, int bumpCount = 3) {
        Pt s = canvas->toScreen(Pt(wx, wy));
        int cx = (int)s.x, cy = (int)s.y;
        if (cx < -100 || cx > canvas->width + 100) return;

        // Keep cloud floating naturally in the upper sky (y: 28..65)
        if (cy < 26) cy = 26;
        if (cy > 65) cy = 65;

        if (bumpCount <= 2) {
            // Small 2-bump cloud
            int r = 13;
            // Black border silhouette
            fillCircle(cx - 12, cy, r + 2, OUTLINE_BLACK);
            fillCircle(cx + 12, cy, r + 2, OUTLINE_BLACK);
            fillRect(cx - 12, cy - r, 24, (r + 2) * 2, OUTLINE_BLACK);

            // White body fill
            fillCircle(cx - 12, cy, r, CLOUD_WHITE);
            fillCircle(cx + 12, cy, r, CLOUD_WHITE);
            fillRect(cx - 12, cy - r + 2, 24, r * 2 - 2, CLOUD_WHITE);

            // Subtle bottom blue shade
            fillRect(cx - 10, cy + r - 2, 20, 2, ColorRGBA(180, 216, 255));

            // Cute smiling eyes
            drawMarioEyes(cx, cy, 10, 7);
        } else if (bumpCount == 3) {
            // Classic 3-bump cloud
            int rSide = 12;
            int rMid = 16;
            // Black border silhouette
            fillCircle(cx - 18, cy + 2, rSide + 2, OUTLINE_BLACK);
            fillCircle(cx,      cy - 4, rMid + 2,  OUTLINE_BLACK);
            fillCircle(cx + 18, cy + 2, rSide + 2, OUTLINE_BLACK);
            fillRect(cx - 18, cy, 36, rSide + 4, OUTLINE_BLACK);

            // White body fill
            fillCircle(cx - 18, cy + 2, rSide, CLOUD_WHITE);
            fillCircle(cx,      cy - 4, rMid,  CLOUD_WHITE);
            fillCircle(cx + 18, cy + 2, rSide, CLOUD_WHITE);
            fillRect(cx - 18, cy, 36, rSide + 2, CLOUD_WHITE);

            // Subtle bottom blue shade
            fillRect(cx - 16, cy + rSide, 32, 2, ColorRGBA(180, 216, 255));

            // Cute smiling eyes
            drawMarioEyes(cx, cy + 2, 11, 8);
        } else {
            // Large 4-bump cloud
            int rSide = 11;
            int rMid = 15;
            // Black border silhouette
            fillCircle(cx - 27, cy + 3, rSide + 2, OUTLINE_BLACK);
            fillCircle(cx - 9,  cy - 3, rMid + 2,  OUTLINE_BLACK);
            fillCircle(cx + 9,  cy - 3, rMid + 2,  OUTLINE_BLACK);
            fillCircle(cx + 27, cy + 3, rSide + 2, OUTLINE_BLACK);
            fillRect(cx - 27, cy + 1, 54, rSide + 4, OUTLINE_BLACK);

            // White body fill
            fillCircle(cx - 27, cy + 3, rSide, CLOUD_WHITE);
            fillCircle(cx - 9,  cy - 3, rMid,  CLOUD_WHITE);
            fillCircle(cx + 9,  cy - 3, rMid,  CLOUD_WHITE);
            fillCircle(cx + 27, cy + 3, rSide, CLOUD_WHITE);
            fillRect(cx - 27, cy + 1, 54, rSide + 2, CLOUD_WHITE);

            // Subtle bottom blue shade
            fillRect(cx - 24, cy + rSide, 48, 2, ColorRGBA(180, 216, 255));

            // Cute smiling eyes
            drawMarioEyes(cx, cy + 2, 12, 8);
        }
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
        body.push_back(Pt(wid * 0.5f, 150.0f));
        body.push_back(Pt(-wid * 0.5f, 150.0f));

        // Vibrant Green Hill Fill
        base->poly(body, wx, wy, GRASS_LIGHT, ColorRGBA(0, 0, 0, 0), 0, false);
        // Thick cartoon outline
        base->stroke(curve, wx, wy, OUTLINE_BLACK, 3.0f);

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

    // 3. Classic Green Warp Pipe (초록색 토관 - 땅에 완벽 밀착)
    void drawPipe(float wx, int heightPx = 48) {
        Pt s = canvas->toScreen(Pt(wx, 0));
        int px = (int)s.x;
        if (px < -60 || px > canvas->width + 60) return;

        int groundY = canvas->height - 42; // Exactly 198px (Ground surface)
        int topY = groundY - heightPx;

        int rimW = 42, rimH = 15;
        int bodyW = 34;
        int rimX = px - rimW / 2;
        int bodyX = px - bodyW / 2;

        // 1. Pipe Rim (Top collar)
        fillRect(rimX, topY, rimW, rimH, PIPE_GREEN);
        fillRect(rimX + 4, topY + 2, 5, rimH - 4, ColorRGBA(140, 248, 120)); // highlight
        fillRect(rimX + rimW - 10, topY + 2, 6, rimH - 4, PIPE_DARK);        // shadow
        // Rim black borders
        fillRect(rimX, topY, rimW, 2, OUTLINE_BLACK);
        fillRect(rimX, topY + rimH - 2, rimW, 2, OUTLINE_BLACK);
        fillRect(rimX, topY, 2, rimH, OUTLINE_BLACK);
        fillRect(rimX + rimW - 2, topY, 2, rimH, OUTLINE_BLACK);

        // 2. Pipe Body: Extends from under rim down to ground surface
        int bodyY = topY + rimH - 1;
        int bodyH = groundY - bodyY;
        if (bodyH > 0) {
            fillRect(bodyX, bodyY, bodyW, bodyH, PIPE_GREEN);
            fillRect(bodyX + 4, bodyY, 4, bodyH, ColorRGBA(140, 248, 120)); // highlight
            fillRect(bodyX + bodyW - 8, bodyY, 5, bodyH, PIPE_DARK);        // shadow
            // Vertical black borders
            fillRect(bodyX, bodyY, 2, bodyH, OUTLINE_BLACK);
            fillRect(bodyX + bodyW - 2, bodyY, 2, bodyH, OUTLINE_BLACK);
            // Bottom black anchor line
            fillRect(bodyX, groundY - 1, bodyW, 2, OUTLINE_BLACK);
        }
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
            // Corner rivets
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
        int groundY = canvas->height - 42;
        int stemW = 12;
        int stemH = groundY - my;
        if (stemH <= 0) return;

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
        int groundY = canvas->height - 42; // Bottom 42 pixels (y: 198..239)

        // Top Grass Lip (초록 잔디 테두리)
        fillRect(sliceX, groundY, sliceW, 8, GRASS_LIGHT);
        fillRect(sliceX, groundY + 7, sliceW, 2, GRASS_DARK);
        fillRect(sliceX, groundY, sliceW, 2, OUTLINE_BLACK);

        // Warm 16-bit Checkered Soil
        for (int y = groundY + 8; y < canvas->height; y += 16) {
            int bh = std::min(16, canvas->height - y);
            for (int x = sliceX; x < sliceX + sliceW; x += 16) {
                int bw = std::min(16, sliceX + sliceW - x);
                bool chk = (((x / 16) + (y / 16)) & 1) == 0;
                fillRect(x, y, bw, bh, chk ? DIRT_LIGHT : DIRT_DARK);
                // Subtle mortar lines
                fillRect(x, y, bw, 1, ColorRGBA(140, 80, 20));
                fillRect(x, y, 1, bh, ColorRGBA(140, 80, 20));
            }
        }
    }
};

// Static color constants for Mario World
const ColorRGBA MarioWorldEngine::SKY_BLUE      = ColorRGBA(112, 192, 255); // #70C0FF
const ColorRGBA MarioWorldEngine::GRASS_LIGHT   = ColorRGBA(72, 224, 48);   // #48E030
const ColorRGBA MarioWorldEngine::GRASS_DARK    = ColorRGBA(16, 144, 0);    // #109000
const ColorRGBA MarioWorldEngine::DIRT_LIGHT    = ColorRGBA(228, 160, 72);  // #E4A048
const ColorRGBA MarioWorldEngine::DIRT_DARK     = ColorRGBA(184, 120, 48);  // #B87830
const ColorRGBA MarioWorldEngine::PIPE_GREEN    = ColorRGBA(0, 200, 0);     // #00C800
const ColorRGBA MarioWorldEngine::PIPE_DARK     = ColorRGBA(0, 120, 0);     // #007800
const ColorRGBA MarioWorldEngine::BLOCK_GOLD    = ColorRGBA(248, 184, 0);   // #F8B800
const ColorRGBA MarioWorldEngine::BLOCK_BROWN   = ColorRGBA(176, 88, 0);    // #B05800
const ColorRGBA MarioWorldEngine::CLOUD_WHITE   = ColorRGBA(255, 255, 255);
const ColorRGBA MarioWorldEngine::OUTLINE_BLACK = ColorRGBA(0, 0, 0);

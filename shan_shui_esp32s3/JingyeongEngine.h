#pragma once
#include <Arduino.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>
#include "Canvas565.h"
#include "ShanShuiEngine.h"

// ============================================================================
// JingyeongEngine: Korean True-View Landscape (진경산수화, 眞景山水畫)
// Inspired by Gyeomjae Jeong Seon (겸재 정선 - 금강전도, 인왕제색도)
// Refactored: Solid, majestic granite rock monoliths with authentic wash shading,
// curved ridge contours, mist-shrouded base, and elegant pine groves.
// ============================================================================

class JingyeongEngine {
public:
    ShanShuiEngine* base;
    Canvas565* canvas = nullptr;

    JingyeongEngine(ShanShuiEngine* b) : base(b) {}

    // Subtle, elegant traditional Korean sumi-e ink tones
    static const ColorRGBA INK_STRONG; // 윤곽 및 솔잎 농묵 (rgba(30, 28, 25, 0.9))
    static const ColorRGBA INK_DARK;   // 바위 음영 중묵 (rgba(65, 60, 55, 0.7))
    static const ColorRGBA INK_LIGHT;  // 바위 밝은 면 담묵 (rgba(145, 140, 130, 0.35))
    static const ColorRGBA INK_MIST;   // 기운 안개 (rgba(195, 190, 180, 0.25))

    // 1. Elegant Korean Pine (기품 있는 조선 소나무)
    // Characters: natural S-curve trunk, sweeping pine needle clouds
    void koreanPine(float x, float y, float hei = 90.0f, float wid = 4.5f) {
        if (!canvas) return;
        int segments = 8;
        std::vector<Pt> tr1, tr2;
        float cx = 0, cy = 0;
        float bendDir = base->randChoice({-1.0f, 1.0f});

        for (int i = 0; i <= segments; i++) {
            float p = (float)i / segments;
            float w = wid * (1.0f - p * 0.6f);
            tr1.push_back(Pt(cx - w, cy));
            tr2.push_back(Pt(cx + w, cy));

            cx += bendDir * sinf(p * (float)M_PI) * 10.0f + (base->noise(i * 0.3f, x * 0.01f) - 0.5f) * 8.0f;
            cy -= (hei / segments);

            // Layered umbrella needle clusters
            if (i >= 3 && (base->rand01() < 0.6f || i == segments)) {
                float bDir = (base->rand01() > 0.5f) ? 1.0f : -1.0f;
                float bLen = base->normRand(18.0f, 38.0f);
                float bx1 = x + cx + bDir * bLen;
                float by1 = y + cy + base->normRand(-6.0f, 8.0f);

                base->stroke({ Pt(x + cx, y + cy), Pt(bx1, by1) }, 0, 0, INK_STRONG, 1.8f);

                // Cloud-like needle clusters (부채꼴 솔잎)
                base->blob(bx1, by1 - 3.0f, 20.0f, 7.0f, (float)M_PI * 0.5f, INK_STRONG, 0.3f, false);
                base->blob(bx1 - bDir * 4.0f, by1 - 5.0f, 14.0f, 5.0f, (float)M_PI * 0.5f, INK_DARK, 0.3f, false);
            }
        }

        std::vector<Pt> trunk = tr1;
        for (int i = (int)tr2.size() - 1; i >= 0; i--) trunk.push_back(tr2[i]);
        base->poly(trunk, x, y, PAPER_COLOR, INK_STRONG, 1.5f, true);
    }

    // 2. Solid Korean Granite Pillar Peak (금강산 비로봉식 장엄한 화강암 암봉)
    // Clean geometric facet planes, ink wash gradation, mist base
    void granitePeak(float xoff, float yoff, float seed, float wid = 220.0f, float hei = 280.0f) {
        if (!canvas) return;

        // Peak summit tip
        float tipX = (base->rand01() - 0.5f) * wid * 0.2f;
        float tipY = -hei;

        // Left facet ridge points
        std::vector<Pt> leftRidge = {
            Pt(tipX, tipY),
            Pt(-wid * 0.18f, -hei * 0.75f),
            Pt(-wid * 0.32f, -hei * 0.45f),
            Pt(-wid * 0.45f, -hei * 0.18f),
            Pt(-wid * 0.50f, 20.0f)
        };

        // Right facet ridge points
        std::vector<Pt> rightRidge = {
            Pt(tipX, tipY),
            Pt(wid * 0.20f, -hei * 0.70f),
            Pt(wid * 0.35f, -hei * 0.42f),
            Pt(wid * 0.46f, -hei * 0.15f),
            Pt(wid * 0.50f, 20.0f)
        };

        // Center spine (화강암 주 능선) separating shaded & illuminated faces
        std::vector<Pt> centerSpine = {
            Pt(tipX, tipY),
            Pt(tipX - 6.0f, -hei * 0.72f),
            Pt(tipX + 8.0f, -hei * 0.40f),
            Pt(tipX - 4.0f, -hei * 0.15f),
            Pt(tipX, 20.0f)
        };

        // Subtle noise on edges so it looks naturally weathered
        for (size_t i = 1; i < leftRidge.size() - 1; i++) {
            leftRidge[i].x += (base->noise(i * 0.5f, seed) - 0.5f) * 12.0f;
            leftRidge[i].y += (base->noise(i * 0.5f, seed + 1.0f) - 0.5f) * 10.0f;
        }
        for (size_t i = 1; i < rightRidge.size() - 1; i++) {
            rightRidge[i].x += (base->noise(i * 0.5f, seed + 2.0f) - 0.5f) * 12.0f;
            rightRidge[i].y += (base->noise(i * 0.5f, seed + 3.0f) - 0.5f) * 10.0f;
        }

        // --- 1. Shaded Face (음영 면 - 적묵 積墨 / 중묵) ---
        // Left side facet filled with medium-dark ink
        std::vector<Pt> shadedFacet = leftRidge;
        for (int i = (int)centerSpine.size() - 1; i >= 0; i--) shadedFacet.push_back(centerSpine[i]);
        base->poly(shadedFacet, xoff, yoff, INK_DARK, ColorRGBA(0, 0, 0, 0), 0, false);

        // --- 2. Lighted Face (양명 면 - 담묵 / 백색 화강암) ---
        // Right side facet filled with paper color & subtle wash
        std::vector<Pt> lightFacet = centerSpine;
        for (int i = (int)rightRidge.size() - 1; i >= 0; i--) lightFacet.push_back(rightRidge[i]);
        base->poly(lightFacet, xoff, yoff, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
        base->poly(lightFacet, xoff, yoff, INK_LIGHT, ColorRGBA(0, 0, 0, 0), 0, false);

        // --- 3. Bold Structural Crevice Lines (선별된 굵은 바위 주름선 3~4개) ---
        // On shaded face: a few strong diagonal clefts
        for (int c = 1; c <= 3; c++) {
            float py0 = -hei * (0.85f - c * 0.22f);
            float px0 = leftRidge[c].x * 0.8f;
            float px1 = centerSpine[c].x;
            float py1 = centerSpine[c].y - 8.0f;
            std::vector<Pt> cleft = { Pt(px0, py0), Pt((px0 + px1) * 0.5f, (py0 + py1) * 0.5f + 4.0f), Pt(px1, py1) };
            base->stroke(cleft, xoff, yoff, INK_STRONG, 2.2f);
        }

        // On light face: subtle vertical cleavage lines
        for (int c = 1; c <= 3; c++) {
            float py0 = -hei * (0.78f - c * 0.20f);
            float px0 = centerSpine[c].x + 8.0f;
            float px1 = rightRidge[c].x * 0.7f;
            float py1 = rightRidge[c].y - 6.0f;
            std::vector<Pt> cleft = { Pt(px0, py0), Pt(px1, py1) };
            base->stroke(cleft, xoff, yoff, INK_LIGHT, 1.5f);
        }

        // --- 4. Strong Contours (기운생동하는 외곽선 & 주능선) ---
        base->stroke(leftRidge, xoff, yoff, INK_STRONG, 2.8f);
        base->stroke(rightRidge, xoff, yoff, INK_STRONG, 2.8f);
        base->stroke(centerSpine, xoff, yoff, INK_STRONG, 2.4f);

        // --- 5. Waterfall cascading from rock cleft (은하수 같은 폭포) ---
        if (base->rand01() < 0.5f) {
            float wfx = xoff + centerSpine[1].x + 4.0f;
            float wfy = yoff + centerSpine[1].y;
            float wlen = hei * 0.45f;
            
            // White water plume
            std::vector<Pt> wf = {
                Pt(wfx - 2.5f, wfy), Pt(wfx + 2.5f, wfy),
                Pt(wfx + 3.0f, wfy + wlen), Pt(wfx - 3.0f, wfy + wlen)
            };
            base->poly(wf, 0, 0, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
            base->stroke({ Pt(wfx - 3.5f, wfy), Pt(wfx - 3.5f, wfy + wlen) }, 0, 0, INK_STRONG, 2.0f);
            base->stroke({ Pt(wfx + 3.5f, wfy), Pt(wfx + 3.5f, wfy + wlen) }, 0, 0, INK_STRONG, 2.0f);
        }

        // --- 6. Pine Trees rooted on crags (바위 절벽에 뿌리내린 노송 2~3그루) ---
        for (int i : {1, 3}) {
            if (base->rand01() < 0.8f) {
                koreanPine(xoff + rightRidge[i].x + 8.0f, yoff + rightRidge[i].y, base->normRand(65.0f, 95.0f), 4.0f);
            }
        }

        // --- 7. Mist band at mountain foot (산자락을 감도는 운무 雲霧) ---
        std::vector<Pt> mist = {
            Pt(-wid * 0.6f, -15.0f), Pt(wid * 0.6f, -15.0f),
            Pt(wid * 0.6f, 35.0f), Pt(-wid * 0.6f, 35.0f)
        };
        base->poly(mist, xoff, yoff, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
        base->poly(mist, xoff, yoff, INK_MIST, ColorRGBA(0, 0, 0, 0), 0, false);
    }

    // 3. Korean Traditional Mountain Pavilion (조선 정자, 亭子)
    void koreanPavilion(float xoff, float yoff) {
        if (!canvas) return;
        float wid = 42.0f, hei = 18.0f;

        // Wooden base posts & deck
        base->stroke({ Pt(-wid * 0.45f, 0), Pt(wid * 0.45f, 0) }, xoff, yoff, INK_STRONG, 2.5f);
        base->stroke({ Pt(-wid * 0.35f, 0), Pt(-wid * 0.35f, 14.0f) }, xoff, yoff, INK_STRONG, 2.0f);
        base->stroke({ Pt(wid * 0.35f, 0), Pt(wid * 0.35f, 14.0f) }, xoff, yoff, INK_STRONG, 2.0f);

        // 4 pillars
        for (float px : {-wid * 0.35f, -wid * 0.12f, wid * 0.12f, wid * 0.35f}) {
            base->stroke({ Pt(px, -hei), Pt(px, 0) }, xoff, yoff, INK_STRONG, 1.8f);
        }

        // Curved eaves Korean Giwa Roof (날렵하게 들린 팔작지붕)
        std::vector<Pt> roof = {
            Pt(-wid * 0.65f, -hei + 4.0f),
            Pt(-wid * 0.35f, -hei - 3.0f),
            Pt(0, -hei - 10.0f),
            Pt(wid * 0.35f, -hei - 3.0f),
            Pt(wid * 0.65f, -hei + 4.0f),
            Pt(wid * 0.45f, -hei),
            Pt(-wid * 0.45f, -hei)
        };
        base->poly(roof, xoff, yoff, INK_STRONG, INK_STRONG, 1.5f, false);

        // Seated scholar in white robe (도포 입은 선비)
        base->blob(xoff, yoff - 5.0f, 6.0f, 6.0f, 0, INK_STRONG, 0.2f, false);
    }

    // 4. Smooth Korean Earthen Hill (유려한 토산 土山)
    void earthenHill(float xoff, float yoff, float seed, float wid = 450.0f, float hei = 100.0f) {
        if (!canvas) return;
        int segs = 20;
        std::vector<Pt> curve;
        for (int i = 0; i <= segs; i++) {
            float p = (float)i / segs;
            float x = (p - 0.5f) * wid;
            float y = -sinf(p * (float)M_PI) * hei * (0.85f + 0.25f * base->noise(p * 2.0f, seed));
            curve.push_back(Pt(x, y));
        }

        std::vector<Pt> body = curve;
        body.push_back(Pt(wid * 0.5f, 40.0f));
        body.push_back(Pt(-wid * 0.5f, 40.0f));
        base->poly(body, xoff, yoff, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
        base->poly(body, xoff, yoff, INK_MIST, ColorRGBA(0, 0, 0, 0), 0, false);
        base->stroke(curve, xoff, yoff, INK_DARK, 2.0f);

        // Sparsely placed moss dots (간결한 태점 苔點)
        for (int i = 2; i < segs - 2; i += 3) {
            float kx = xoff + curve[i].x;
            float ky = yoff + curve[i].y;
            base->blob(kx + base->normRand(-6.0f, 6.0f), ky - 3.0f, 7.0f, 3.5f, 0, INK_STRONG, 0.3f, false);
        }
    }
};

// Static color constants
const ColorRGBA JingyeongEngine::INK_STRONG = ColorRGBA(30, 28, 25, 0.92f);  // 농묵(濃墨)
const ColorRGBA JingyeongEngine::INK_DARK   = ColorRGBA(65, 60, 55, 0.68f);  // 중묵(中墨) 음영면
const ColorRGBA JingyeongEngine::INK_LIGHT  = ColorRGBA(145, 140, 130, 0.30f); // 담묵(淡墨) 양명면
const ColorRGBA JingyeongEngine::INK_MIST   = ColorRGBA(210, 205, 195, 0.40f); // 기운 안개

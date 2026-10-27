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
// Features:
// 1. Vertical Axe-Cut / Cleaved Granite Peaks (수직준 垂直皴, 부벽준 斧劈皴)
// 2. Deep Ink-Wash Accumulated Shading (적묵법 積墨法, 짙은 흑회색 대조)
// 3. Korean Red Pines (적송/해송 - 구불구불 굵은 줄기와 부채꼴 솔잎)
// 4. Korean Mountain Pavilions & Scholar / Waterfall (사선형 지붕 정자, 계곡 폭포)
// ============================================================================

class JingyeongEngine {
public:
    ShanShuiEngine* base; // Reuse BBS PRNG and 4-octave Perlin noise
    Canvas565* canvas = nullptr;

    JingyeongEngine(ShanShuiEngine* b) : base(b) {}

    // Color definitions for deep Korean ink wash
    static const ColorRGBA INK_DEEP;   // 진한 먹색 (적묵)
    static const ColorRGBA INK_MID;    // 중간 먹색
    static const ColorRGBA INK_MIST;   // 연한 안개 먹색
    static const ColorRGBA ROCK_LIGHT; // 화강암 밝은 면
    static const ColorRGBA PINE_BARK;  // 적송 줄기

    // 1. Korean Pine Tree (조선 적송 / 해송)
    // Characters: heavily gnarled trunks, layered horizontal canopy clusters
    void koreanPine(float x, float y, float hei = 160.0f, float wid = 7.0f) {
        if (!canvas) return;
        int segments = 10;
        std::vector<Pt> tr1, tr2;
        float cx = 0, cy = 0;
        float bendDir = base->randChoice({-1.0f, 1.0f});
        
        for (int i = 0; i <= segments; i++) {
            float p = (float)i / segments;
            float w = wid * (1.0f - p * 0.65f);
            tr1.push_back(Pt(cx - w, cy));
            tr2.push_back(Pt(cx + w, cy));

            // Dramatic crooked bends characteristic of Korean mountain pines
            cx += bendDir * sinf(p * (float)M_PI * 1.5f) * 12.0f + (base->noise(i * 0.4f, x * 0.01f) - 0.5f) * 16.0f;
            cy -= (hei / segments);

            // Side branches with umbrella-like needle tufts
            if (i >= 4 && (base->rand01() < 0.55f || i == segments)) {
                float bDir = (base->rand01() > 0.5f) ? 1.0f : -1.0f;
                float bLen = base->normRand(25.0f, 60.0f);
                float bx1 = x + cx + bDir * bLen;
                float by1 = y + cy + base->normRand(-10.0f, 15.0f);
                
                // Gnarled branch
                std::vector<Pt> br = { Pt(x + cx, y + cy), Pt(x + cx + bDir * (bLen * 0.5f), y + cy - 8.0f), Pt(bx1, by1) };
                base->stroke(br, 0, 0, INK_DEEP, 2.5f);

                // Characteristic Korean pine needle clusters (부채꼴 솔잎 구름)
                for (int c = 0; c < 3; c++) {
                    float ox = (c - 1) * 8.0f;
                    base->blob(bx1 + ox, by1 - 4.0f, 22.0f, 10.0f, (float)M_PI * 0.5f, INK_DEEP, 0.4f, false);
                    base->blob(bx1 + ox, by1 - 7.0f, 16.0f, 7.0f, (float)M_PI * 0.5f, INK_MID, 0.3f, false);
                }
            }
        }

        std::vector<Pt> trunk = tr1;
        for (int i = (int)tr2.size() - 1; i >= 0; i--) trunk.push_back(tr2[i]);
        base->poly(trunk, x, y, PAPER_COLOR, INK_DEEP, 1.8f, true);

        // Rough pine bark lines (적송 껍질 결)
        for (int i = 1; i < segments; i += 2) {
            float my = cy * ((float)i / segments);
            std::vector<Pt> bl = { Pt(tr1[i].x + 2, tr1[i].y), Pt(tr2[i].x - 2, tr2[i].y) };
            base->poly(bl, x, y, ColorRGBA(0, 0, 0, 0), INK_MID, 1.2f, false);
        }
    }

    // 2. Cleaved Granite Peak (금강산 비로봉식 화강암 바위산 - 수직준 / 부벽준)
    // Characters: Sharp angular fractured cliffs, vertical strokes, stark black/white contrast
    void granitePeak(float xoff, float yoff, float seed, float wid = 300.0f, float hei = 320.0f) {
        if (!canvas) return;
        int numPillars = 5 + (int)(base->rand01() * 4.0f);
        float pWidth = wid / (numPillars - 1);

        // Overall mountain silhouette with craggy clefts
        std::vector<Pt> ridge;
        for (int i = 0; i < numPillars; i++) {
            float px = (i - numPillars * 0.5f) * pWidth;
            float norm = 1.0f - fabsf((float)i - (numPillars * 0.5f)) / (numPillars * 0.5f);
            float ph = hei * (0.6f + 0.4f * sinf(norm * (float)M_PI * 0.5f));
            ph *= (0.85f + 0.3f * base->noise(i * 0.7f, seed));
            
            // Pillar pinnacle and side jagged edges
            float cleft = (base->rand01() - 0.5f) * pWidth * 0.4f;
            ridge.push_back(Pt(px - pWidth * 0.4f, -ph * 0.92f));
            ridge.push_back(Pt(px + cleft, -ph)); // Peak tip
            ridge.push_back(Pt(px + pWidth * 0.4f, -ph * 0.88f));
        }

        // Fill mountain body with paper color (block out background scenery)
        std::vector<Pt> body = ridge;
        body.push_back(Pt(wid * 0.6f, 50.0f));
        body.push_back(Pt(-wid * 0.6f, 50.0f));
        base->poly(body, xoff, yoff, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);

        // Hard vertical cleavage lines (정선 특유의 수직준 垂直皴)
        for (int i = 0; i < numPillars; i++) {
            float px = (i - numPillars * 0.5f) * pWidth;
            float topY = -hei * (0.6f + 0.4f * base->noise(i * 0.5f, seed + 1.0f));
            
            // Draw multiple parallel vertical split lines cascading down the granite pillar
            for (int k = 0; k < 4; k++) {
                float ox = px + (k - 1.5f) * (pWidth * 0.22f);
                float splitLen = hei * base->normRand(0.4f, 0.9f);
                std::vector<Pt> splitLine;
                for (float sy = 0; sy < splitLen; sy += 18.0f) {
                    float jx = ox + (base->noise(sy * 0.1f, k) - 0.5f) * 6.0f;
                    splitLine.push_back(Pt(jx, topY + sy));
                }
                base->stroke(splitLine, xoff, yoff, INK_DEEP, base->normRand(1.8f, 3.2f), 0.2f);
            }

            // Angular axe-cut facets (부벽준 斧劈皴 - 꺾인 바위 단면)
            if (base->rand01() < 0.7f) {
                float fy = topY + hei * base->normRand(0.3f, 0.7f);
                std::vector<Pt> facet = {
                    Pt(px - 14.0f, fy),
                    Pt(px + 18.0f, fy - 8.0f),
                    Pt(px + 8.0f, fy + 22.0f),
                    Pt(px - 10.0f, fy + 16.0f)
                };
                base->poly(facet, xoff, yoff, INK_MID, INK_DEEP, 1.2f, false);
            }
        }

        // Dark ridge outline with jagged calligraphy stroke
        base->stroke(ridge, xoff, yoff, INK_DEEP, 3.2f, 0.7f);

        // Summit Korean red pines clinging to cliffs
        for (int i = 0; i < (int)ridge.size(); i += 3) {
            if (base->rand01() < 0.6f) {
                koreanPine(xoff + ridge[i].x, yoff + ridge[i].y + 4.0f, base->normRand(45.0f, 75.0f), 3.0f);
            }
        }

        // Waterfall pouring down mountain crevices (폭포, 瀑布)
        if (base->rand01() < 0.45f) {
            float wfx = xoff + base->normRand(-wid * 0.2f, wid * 0.2f);
            float wfy = yoff - hei * 0.7f;
            float wfLen = hei * 0.65f;
            
            // White water ribbon
            std::vector<Pt> wf = {
                Pt(wfx - 3.0f, wfy), Pt(wfx + 3.0f, wfy),
                Pt(wfx + 4.0f, wfy + wfLen), Pt(wfx - 4.0f, wfy + wfLen)
            };
            base->poly(wf, 0, 0, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
            
            // Dark cliffs flanking the waterfall
            base->stroke({Pt(wfx - 5.0f, wfy), Pt(wfx - 4.0f, wfy + wfLen)}, 0, 0, INK_DEEP, 2.5f);
            base->stroke({Pt(wfx + 5.0f, wfy), Pt(wfx + 4.0f, wfy + wfLen)}, 0, 0, INK_DEEP, 2.5f);
        }
    }

    // 3. Korean Traditional Mountain Pavilion (조선 정자, 亭子 - 세검정/소쇄원 스타일)
    // Features: curved tile hipped roof (팔작지붕), wooden floor and posts overlooking valley
    void koreanPavilion(float xoff, float yoff) {
        if (!canvas) return;
        float wid = 45.0f, hei = 18.0f;

        // Wooden base posts & deck
        std::vector<Pt> deck = { Pt(-wid * 0.45f, 0), Pt(wid * 0.45f, 0) };
        base->stroke(deck, xoff, yoff, INK_DEEP, 2.5f);
        base->stroke({Pt(-wid * 0.35f, 0), Pt(-wid * 0.35f, 12.0f)}, xoff, yoff, INK_DEEP, 2.0f);
        base->stroke({Pt(wid * 0.35f, 0), Pt(wid * 0.35f, 12.0f)}, xoff, yoff, INK_DEEP, 2.0f);
        base->stroke({Pt(0, 0), Pt(0, 14.0f)}, xoff, yoff, INK_DEEP, 2.0f);

        // 4 pillars
        for (float px : {-wid * 0.35f, -wid * 0.12f, wid * 0.12f, wid * 0.35f}) {
            base->stroke({Pt(px, -hei), Pt(px, 0)}, xoff, yoff, INK_DEEP, 1.8f);
        }

        // Curved eaves Korean Giwa Roof (처마선이 날렵하게 들린 팔작지붕)
        std::vector<Pt> roof = {
            Pt(-wid * 0.65f, -hei + 4.0f),
            Pt(-wid * 0.35f, -hei - 3.0f),
            Pt(0, -hei - 10.0f),
            Pt(wid * 0.35f, -hei - 3.0f),
            Pt(wid * 0.65f, -hei + 4.0f),
            Pt(wid * 0.45f, -hei),
            Pt(-wid * 0.45f, -hei)
        };
        base->poly(roof, xoff, yoff, INK_DEEP, INK_DEEP, 1.5f, false);

        // Seated scholar admiring the mountain (정자 속 도포 입은 선비)
        base->blob(xoff, yoff - 5.0f, 6.0f, 6.0f, 0, INK_DEEP, 0.2f, false);
        base->poly({Pt(xoff - 3.0f, yoff - 8.0f), Pt(xoff + 3.0f, yoff - 8.0f)}, 0, 0, INK_DEEP, INK_DEEP, 2.0f);
    }

    // 4. Korean Low Earthen Hills with Ink-Dot Foliage (토산 土山 & 미점준 米點皴)
    void earthenHill(float xoff, float yoff, float seed, float wid = 420.0f, float hei = 120.0f) {
        if (!canvas) return;
        int segs = 25;
        std::vector<Pt> curve;
        for (int i = 0; i <= segs; i++) {
            float p = (float)i / segs;
            float x = (p - 0.5f) * wid;
            float y = -sinf(p * (float)M_PI) * hei * (0.8f + 0.3f * base->noise(p * 2.0f, seed));
            curve.push_back(Pt(x, y));
        }

        std::vector<Pt> body = curve;
        body.push_back(Pt(wid * 0.5f, 40.0f));
        body.push_back(Pt(-wid * 0.5f, 40.0f));
        base->poly(body, xoff, yoff, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
        base->stroke(curve, xoff, yoff, INK_MID, 2.2f);

        // Ink dots (미점준 米點 / 태점 苔點) across ridge
        for (int i = 2; i < segs - 2; i += 2) {
            float kx = xoff + curve[i].x;
            float ky = yoff + curve[i].y;
            for (int d = 0; d < 4; d++) {
                base->blob(kx + base->normRand(-12.0f, 12.0f), ky + base->normRand(-6.0f, 6.0f),
                           8.0f, 4.0f, 0, INK_DEEP, 0.3f, false);
            }
        }
    }
};

// Static color constants
const ColorRGBA JingyeongEngine::INK_DEEP   = ColorRGBA(35, 33, 30, 0.95f);  // 흑묵(黑墨)
const ColorRGBA JingyeongEngine::INK_MID    = ColorRGBA(85, 80, 75, 0.65f);  // 중묵(中墨)
const ColorRGBA JingyeongEngine::INK_MIST   = ColorRGBA(160, 155, 145, 0.40f); // 담묵(淡墨)
const ColorRGBA JingyeongEngine::ROCK_LIGHT = ColorRGBA(225, 220, 205, 0.85f); // 화강암 백색부
const ColorRGBA JingyeongEngine::PINE_BARK  = ColorRGBA(60, 45, 38, 0.85f);  // 적송 줄기

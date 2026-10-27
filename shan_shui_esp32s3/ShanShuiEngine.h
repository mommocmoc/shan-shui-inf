#pragma once
#include <Arduino.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>
#include "Canvas565.h"

// ============================================================================
// ShanShuiEngine: C++ Port of LingDong-'s shan-shui-inf
// Infinite Procedurally Generated Chinese Landscape Painting
// ============================================================================

class ShanShuiEngine {
public:
    // --- PRNG (Blum-Blum-Shub style from shan-shui-inf) ---
    uint64_t prng_s = 1234;
    const uint64_t prng_p = 9887ULL;
    const uint64_t prng_q = 9967ULL;
    const uint64_t prng_m = 9887ULL * 9967ULL;

    void seedPRNG(uint32_t seedVal) {
        uint64_t y = (seedVal * 2654435761ULL + 12345ULL) % prng_m;
        while (y % prng_p == 0 || y % prng_q == 0 || y <= 1) {
            y = (y + 1) % prng_m;
        }
        prng_s = y;
        for (int i = 0; i < 10; i++) rand01();
        initPerlin();
    }

    inline float rand01() {
        prng_s = (prng_s * prng_s) % prng_m;
        return (float)prng_s / (float)prng_m;
    }

    // --- Perlin Noise (p5.js style from shan-shui-inf) ---
    static const int PERLIN_YWRAPB = 4;
    static const int PERLIN_YWRAP = 1 << PERLIN_YWRAPB;
    static const int PERLIN_ZWRAPB = 8;
    static const int PERLIN_ZWRAP = 1 << PERLIN_ZWRAPB;
    static const int PERLIN_SIZE = 4095;
    float perlin[PERLIN_SIZE + 1];

    void initPerlin() {
        for (int i = 0; i <= PERLIN_SIZE; i++) {
            perlin[i] = rand01();
        }
    }

    static inline float scaled_cosine(float i) {
        return 0.5f * (1.0f - cosf(i * (float)M_PI));
    }

    float noise(float x, float y = 0.0f, float z = 0.0f) const {
        if (x < 0) x = -x;
        if (y < 0) y = -y;
        if (z < 0) z = -z;
        int xi = (int)floorf(x), yi = (int)floorf(y), zi = (int)floorf(z);
        float xf = x - xi, yf = y - yi, zf = z - zi;
        float r = 0.0f, ampl = 0.5f;
        for (int o = 0; o < 4; o++) {
            int of = xi + (yi << PERLIN_YWRAPB) + (zi << PERLIN_ZWRAPB);
            float rxf = scaled_cosine(xf);
            float ryf = scaled_cosine(yf);
            float n1 = perlin[of & PERLIN_SIZE];
            n1 += rxf * (perlin[(of + 1) & PERLIN_SIZE] - n1);
            float n2 = perlin[(of + PERLIN_YWRAP) & PERLIN_SIZE];
            n2 += rxf * (perlin[(of + PERLIN_YWRAP + 1) & PERLIN_SIZE] - n2);
            n1 += ryf * (n2 - n1);
            of += PERLIN_ZWRAP;
            n2 = perlin[of & PERLIN_SIZE];
            n2 += rxf * (perlin[(of + 1) & PERLIN_SIZE] - n2);
            float n3 = perlin[(of + PERLIN_YWRAP) & PERLIN_SIZE];
            n3 += rxf * (perlin[(of + PERLIN_YWRAP + 1) & PERLIN_SIZE] - n3);
            n2 += ryf * (n3 - n2);
            n1 += scaled_cosine(zf) * (n2 - n1);
            r += n1 * ampl;
            ampl *= 0.5f;
            xi <<= 1; xf *= 2.0f;
            yi <<= 1; yf *= 2.0f;
            zi <<= 1; zf *= 2.0f;
            if (xf >= 1.0f) { xi++; xf -= 1.0f; }
            if (yf >= 1.0f) { yi++; yf -= 1.0f; }
            if (zf >= 1.0f) { zi++; zf -= 1.0f; }
        }
        return r;
    }

    // --- Math & Geometry Helpers ---
    static inline float dist(const Pt& p0, const Pt& p1) {
        float dx = p0.x - p1.x, dy = p0.y - p1.y;
        return sqrtf(dx * dx + dy * dy);
    }

    static inline Pt midPt(const Pt& p0, const Pt& p1) {
        return Pt((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
    }

    static inline Pt midPtList(const std::vector<Pt>& plist) {
        if (plist.empty()) return Pt(0, 0);
        float sx = 0, sy = 0;
        for (const auto& p : plist) { sx += p.x; sy += p.y; }
        float inv = 1.0f / (float)plist.size();
        return Pt(sx * inv, sy * inv);
    }

    static inline float mapval(float v, float istart, float istop, float ostart, float ostop) {
        if (fabsf(istop - istart) < 1e-6f) return ostart;
        return ostart + (ostop - ostart) * ((v - istart) / (istop - istart));
    }

    inline float normRand(float m, float M) {
        return mapval(rand01(), 0.0f, 1.0f, m, M);
    }

    template <typename T>
    inline T randChoice(std::initializer_list<T> list) {
        int idx = (int)floorf(rand01() * list.size());
        if (idx >= (int)list.size()) idx = (int)list.size() - 1;
        return *(list.begin() + idx);
    }

    float wtrand(std::function<float(float)> func) {
        for (int iter = 0; iter < 30; iter++) {
            float x = rand01();
            float y = rand01();
            if (y < func(x)) return x;
        }
        return rand01();
    }

    float randGaussian() {
        return wtrand([](float x) {
            float d = x - 0.5f;
            return expf(-24.0f * d * d);
        }) * 2.0f - 1.0f;
    }

    void loopNoise(std::vector<float>& nslist) {
        if (nslist.size() < 2) return;
        int n = (int)nslist.size();
        float dif = nslist[n - 1] - nslist[0];
        float bmin = 100.0f, bmax = -100.0f;
        for (int i = 0; i < n; i++) {
            nslist[i] += (dif * (n - 1 - i)) / (float)(n - 1);
            if (nslist[i] < bmin) bmin = nslist[i];
            if (nslist[i] > bmax) bmax = nslist[i];
        }
        for (int i = 0; i < n; i++) {
            nslist[i] = mapval(nslist[i], bmin, bmax, 0.0f, 1.0f);
        }
    }

    std::vector<Pt> div(const std::vector<Pt>& plist, int reso) {
        std::vector<Pt> rlist;
        if (plist.empty()) return rlist;
        if (plist.size() == 1 || reso <= 0) return plist;
        int tl = ((int)plist.size() - 1) * reso;
        rlist.reserve(tl + 1);
        for (int i = 0; i < tl; i++) {
            const Pt& lastp = plist[i / reso];
            const Pt& nextp = plist[(i / reso) + 1];
            float p = (float)(i % reso) / (float)reso;
            rlist.push_back(Pt(lastp.x * (1.0f - p) + nextp.x * p,
                               lastp.y * (1.0f - p) + nextp.y * p));
        }
        rlist.push_back(plist.back());
        return rlist;
    }

    std::vector<Pt> bezmh(std::vector<Pt> P, float w = 1.0f) {
        if (P.size() == 2) {
            P = { P[0], midPt(P[0], P[1]), P[1] };
        }
        std::vector<Pt> plist;
        if (P.size() < 3) return plist;
        for (size_t j = 0; j < P.size() - 2; j++) {
            Pt p0 = (j == 0) ? P[j] : midPt(P[j], P[j + 1]);
            Pt p1 = P[j + 1];
            Pt p2 = (j == P.size() - 3) ? P[j + 2] : midPt(P[j + 1], P[j + 2]);
            int pl = 12;
            int endI = pl + (j == P.size() - 3 ? 1 : 0);
            for (int i = 0; i < endI; i++) {
                float t = (float)i / (float)pl;
                float omt = 1.0f - t;
                float u = omt * omt + 2.0f * t * omt * w + t * t;
                plist.push_back(Pt(
                    (omt * omt * p0.x + 2.0f * t * omt * p1.x * w + t * t * p2.x) / u,
                    (omt * omt * p0.y + 2.0f * t * omt * p1.y * w + t * t * p2.y) / u
                ));
            }
        }
        return plist;
    }

    // --- Drawing Primitives onto Canvas565 ---
    Canvas565* canvas = nullptr;

    void poly(const std::vector<Pt>& plist, float xof, float yof, const ColorRGBA& fil, const ColorRGBA& str, float wid = 0.0f, bool isPaperFill = false) {
        if (!canvas || plist.empty()) return;
        if (fil.a <= 0.001f && !isPaperFill) {
            canvas->drawPolylineWorld(plist, xof, yof, str, wid, false);
        } else {
            canvas->fillPolygonWorld(plist, xof, yof, fil, str, wid, isPaperFill);
        }
    }

    void stroke(const std::vector<Pt>& ptlist, float xof, float yof, ColorRGBA col, float wid = 2.0f, float noi = 0.5f, float out = 1.0f,
                std::function<float(float)> fun = nullptr) {
        if (!canvas || ptlist.size() < 2) return;
        if (!fun) {
            fun = [](float x) { return sinf(x * (float)M_PI); };
        }
        // If stroke is very thin on screen, draw fast direct polyline
        if (wid * canvas->scale <= 0.85f && ptlist.size() > 3) {
            canvas->drawPolylineWorld(ptlist, xof, yof, col, wid, false);
            return;
        }
        std::vector<Pt> vtx0, vtx1, vtx;
        int n = (int)ptlist.size();
        vtx0.reserve(n);
        vtx1.reserve(n);
        float n0 = rand01() * 10.0f;
        for (int i = 1; i < n - 1; i++) {
            float w = wid * fun((float)i / (float)n);
            w = w * (1.0f - noi) + w * noi * noise(i * 0.5f, n0);
            float a1 = atan2f(ptlist[i].y - ptlist[i - 1].y, ptlist[i].x - ptlist[i - 1].x);
            float a2 = atan2f(ptlist[i].y - ptlist[i + 1].y, ptlist[i].x - ptlist[i + 1].x);
            float a = (a1 + a2) * 0.5f;
            if (a < a2) a += (float)M_PI;
            float ca = cosf(a), sa = sinf(a);
            vtx0.push_back(Pt(ptlist[i].x + w * ca, ptlist[i].y + w * sa));
            vtx1.push_back(Pt(ptlist[i].x - w * ca, ptlist[i].y - w * sa));
        }
        vtx.reserve(2 * n + 2);
        vtx.push_back(ptlist[0]);
        for (const auto& p : vtx0) vtx.push_back(p);
        vtx.push_back(ptlist[n - 1]);
        for (int i = (int)vtx1.size() - 1; i >= 0; i--) vtx.push_back(vtx1[i]);
        vtx.push_back(ptlist[0]);

        poly(vtx, xof, yof, col, col, out, false);
    }

    std::vector<Pt> blob(float x, float y, float len, float wid, float ang, ColorRGBA col, float noi = 0.5f, bool ret = false,
                         std::function<float(float)> fun = nullptr) {
        if (!fun) {
            fun = [](float p) {
                return (p <= 1.0f) ? powf(sinf(p * (float)M_PI), 0.5f)
                                   : -powf(sinf((p + 1.0f) * (float)M_PI), 0.5f);
            };
        }
        int reso = 14;
        std::vector<Pt> lalist(reso + 1);
        std::vector<float> nslist(reso + 1);
        float n0 = rand01() * 10.0f;
        for (int i = 0; i <= reso; i++) {
            float p = ((float)i / (float)reso) * 2.0f;
            float xo = len * 0.5f - fabsf(p - 1.0f) * len;
            float yo = (fun(p) * wid) * 0.5f;
            float a = atan2f(yo, xo);
            float l = sqrtf(xo * xo + yo * yo);
            lalist[i] = Pt(l, a);
            nslist[i] = noise(i * 0.05f, n0);
        }
        loopNoise(nslist);
        std::vector<Pt> plist(reso + 1);
        for (int i = 0; i <= reso; i++) {
            float ns = nslist[i] * noi + (1.0f - noi);
            float nx = x + cosf(lalist[i].y + ang) * lalist[i].x * ns;
            float ny = y + sinf(lalist[i].y + ang) * lalist[i].x * ns;
            plist[i] = Pt(nx, ny);
        }
        if (!ret) {
            poly(plist, 0, 0, col, col, 0, false);
        }
        return plist;
    }

    void texture(const std::vector<std::vector<Pt>>& ptlist, float xof, float yof, int tex, float wid, float len, int sha,
                 std::function<float(float)> noiFunc = nullptr,
                 std::function<ColorRGBA(float)> colFunc = nullptr,
                 std::function<float()> disFunc = nullptr) {
        if (ptlist.empty() || ptlist[0].empty()) return;
        if (!noiFunc) noiFunc = [](float x) { return 30.0f / x; };
        if (!colFunc) colFunc = [this](float x) { return ColorRGBA(100, 100, 100, rand01() * 0.3f); };
        if (!disFunc) {
            disFunc = [this]() {
                if (rand01() > 0.5f) return (1.0f / 3.0f) * rand01();
                else return (2.0f / 3.0f) + (1.0f / 3.0f) * rand01();
            };
        }
        int r0 = (int)ptlist.size();
        int r1 = (int)ptlist[0].size();
        std::vector<std::vector<Pt>> texlist;
        texlist.reserve(tex);

        for (int i = 0; i < tex; i++) {
            int mid = (int)(disFunc() * r1);
            int hlen = (int)floorf(rand01() * (r1 * len));
            int start = std::min(std::max(mid - hlen, 0), r1);
            int end   = std::min(std::max(mid + hlen, 0), r1);
            float layer = ((float)i / (float)tex) * (r0 - 1);
            int l0 = (int)floorf(layer);
            int l1 = (int)ceilf(layer);
            float p = layer - l0;

            std::vector<Pt> line;
            line.reserve(end - start);
            for (int j = start; j < end; j++) {
                float x = ptlist[l0][j].x * p + ptlist[l1][j].x * (1.0f - p);
                float y = ptlist[l0][j].y * p + ptlist[l1][j].y * (1.0f - p);
                float nval = noiFunc(layer + 1.0f);
                float nsx = nval * (noise(x, j * 0.5f) - 0.5f);
                float nsy = nval * (noise(y, j * 0.5f) - 0.5f);
                line.push_back(Pt(x + nsx, y + nsy));
            }
            texlist.push_back(std::move(line));
        }
        if (sha > 0) {
            for (size_t j = 0; j < texlist.size(); j += 2) {
                stroke(texlist[j], xof, yof, ColorRGBA(100, 100, 100, 0.1f), (float)sha);
            }
        }
        int step = 1 + sha;
        if (step < 1) step = 1;
        for (size_t j = (sha > 0 ? 1 : 0); j < texlist.size(); j += (sha > 0 ? 2 : 1)) {
            stroke(texlist[j], xof, yof, colFunc((float)j / (float)texlist.size()), wid);
        }
    }

    // --- Trees ---
    void tree01(float x, float y, float hei = 50.0f, float wid = 3.0f, ColorRGBA col = ColorRGBA(100, 100, 100, 0.5f)) {
        int reso = 10;
        std::vector<Pt> line1, line2;
        for (int i = 0; i < reso; i++) {
            float nx = x;
            float ny = y - (i * hei) / reso;
            if (i >= reso / 4) {
                int cnt = (reso - i) / 5;
                if (cnt < 1) cnt = 1;
                for (int j = 0; j < cnt; j++) {
                    ColorRGBA bcol(col.r, col.g, col.b, std::min(1.0f, rand01() * 0.2f + col.a));
                    blob(nx + (rand01() - 0.5f) * wid * 1.2f * (reso - i),
                         ny + (rand01() - 0.5f) * wid,
                         rand01() * 20.0f * (reso - i) * 0.2f + 10.0f,
                         rand01() * 6.0f + 3.0f,
                         ((rand01() - 0.5f) * (float)M_PI) / 6.0f,
                         bcol, 0.5f, false);
                }
            }
            float n0 = noise(i * 0.5f);
            float n1 = noise(i * 0.5f, 0.5f);
            line1.push_back(Pt(nx + (n0 - 0.5f) * wid - wid * 0.5f, ny));
            line2.push_back(Pt(nx + (n1 - 0.5f) * wid + wid * 0.5f, ny));
        }
        poly(line1, 0, 0, ColorRGBA(0, 0, 0, 0), col, 1.5f);
        poly(line2, 0, 0, ColorRGBA(0, 0, 0, 0), col, 1.5f);
    }

    void tree02(float x, float y, float hei = 16.0f, float wid = 8.0f, int clu = 5, ColorRGBA col = ColorRGBA(100, 100, 100, 0.5f)) {
        auto fun = [](float p) {
            return p <= 1.0f
                ? powf(sinf(p * (float)M_PI) * p, 0.5f)
                : -powf(sinf((p - 2.0f) * (float)M_PI * (p - 2.0f)), 0.5f);
        };
        for (int i = 0; i < clu; i++) {
            blob(x + randGaussian() * clu * 4.0f,
                 y + randGaussian() * clu * 4.0f,
                 rand01() * hei * 0.75f + hei * 0.5f,
                 rand01() * wid * 0.75f + wid * 0.5f,
                 (float)M_PI * 0.5f,
                 col, 0.5f, false, fun);
        }
    }

    void tree03(float x, float y, float hei = 50.0f, float wid = 5.0f, ColorRGBA col = ColorRGBA(100, 100, 100, 0.5f)) {
        int reso = 10;
        float bc = rand01() * 0.1f;
        std::vector<Pt> line1, line2;
        for (int i = 0; i < reso; i++) {
            float nx = x + (i / (float)reso) * bc * 100.0f;
            float ny = y - (i * hei) / reso;
            float n0 = noise(i * 0.5f);
            float n1 = noise(i * 0.5f, 0.5f);
            line1.push_back(Pt(nx + (((n0 - 0.5f) * wid - wid * 0.5f) * (reso - i)) / reso, ny));
            line2.push_back(Pt(nx + (((n1 - 0.5f) * wid + wid * 0.5f) * (reso - i)) / reso, ny));
        }
        std::vector<Pt> lc = line1;
        for (int i = (int)line2.size() - 1; i >= 0; i--) lc.push_back(line2[i]);
        poly(lc, 0, 0, PAPER_COLOR, col, 1.5f, true);

        for (int i = 0; i < reso; i++) {
            float nx = x + (i / (float)reso) * bc * 100.0f;
            float ny = y - (i * hei) / reso;
            if (i >= reso / 5) {
                int cnt = (reso - i);
                for (int j = 0; j < cnt; j++) {
                    float t = (float)(reso - i) / (float)reso;
                    float shape = logf(50.0f * t + 1.0f) / 3.95f;
                    float ox = rand01() * wid * 2.0f * shape;
                    ColorRGBA bcol(col.r, col.g, col.b, std::min(1.0f, rand01() * 0.2f + col.a));
                    blob(nx + ox * randChoice({-1.0f, 1.0f}),
                         ny + (rand01() - 0.5f) * wid * 2.0f,
                         ox * 2.0f,
                         rand01() * 6.0f + 3.0f,
                         ((rand01() - 0.5f) * (float)M_PI) / 6.0f,
                         bcol, 0.5f, false);
                }
            }
        }
    }

    void tree04(float x, float y, float hei = 220.0f, float wid = 6.0f) {
        // Pine / gnarly tree on flat mounts
        int steps = 12;
        std::vector<Pt> tr1, tr2;
        float cx = 0, cy = 0;
        for (int i = 0; i <= steps; i++) {
            float p = (float)i / steps;
            float w = wid * (1.0f - p * 0.7f);
            tr1.push_back(Pt(cx - w, cy));
            tr2.push_back(Pt(cx + w, cy));
            cx += (noise(i * 0.3f, x * 0.01f) - 0.5f) * 18.0f;
            cy -= hei / steps;
            if (i > 3 && (rand01() < 0.45f || i == steps)) {
                float dir = randChoice({-1.0f, 1.0f});
                std::vector<Pt> br = { Pt(x + cx, y + cy), Pt(x + cx + dir * normRand(25, 55), y + cy - normRand(5, 25)) };
                stroke(br, 0, 0, ColorRGBA(100, 100, 100, 0.55f), 2.5f);
                tree02(br[1].x, br[1].y, 14.0f, 28.0f, 4, ColorRGBA(100, 100, 100, 0.6f));
            }
        }
        std::vector<Pt> trunk = tr1;
        for (int i = (int)tr2.size() - 1; i >= 0; i--) trunk.push_back(tr2[i]);
        poly(trunk, x, y, PAPER_COLOR, ColorRGBA(100, 100, 100, 0.5f), 1.5f, true);
    }

    // --- Architecture & Boats & Figures ---
    void man(float xoff, float yoff, float sca = 0.5f, bool fli = true, bool hasStick = false, bool hat2 = false) {
        float dir = fli ? -1.0f : 1.0f;
        // Body robe polygon
        std::vector<Pt> body = {
            Pt(xoff - 4 * sca * dir, yoff),
            Pt(xoff - 5 * sca * dir, yoff - 18 * sca),
            Pt(xoff, yoff - 32 * sca),
            Pt(xoff + 5 * sca * dir, yoff - 18 * sca),
            Pt(xoff + 4 * sca * dir, yoff)
        };
        poly(body, 0, 0, PAPER_COLOR, ColorRGBA(100, 100, 100, 0.6f), 1.0f, true);
        // Head & Hat
        std::vector<Pt> hat = {
            Pt(xoff - 12 * sca, yoff - 33 * sca),
            Pt(xoff, yoff - 39 * sca),
            Pt(xoff + 12 * sca, yoff - 33 * sca)
        };
        poly(hat, 0, 0, ColorRGBA(100, 100, 100, 0.8f), ColorRGBA(100, 100, 100, 0.8f), 1.0f);
        if (hasStick) {
            std::vector<Pt> st = {
                Pt(xoff + 6 * sca * dir, yoff - 22 * sca),
                Pt(xoff + 24 * sca * dir, yoff + 5 * sca)
            };
            poly(st, 0, 0, ColorRGBA(0, 0, 0, 0), ColorRGBA(100, 100, 100, 0.5f), 1.0f);
        }
    }

    void boat01(float xoff, float yoff, float sca = 1.0f, bool fli = false) {
        float dir = fli ? -1.0f : 1.0f;
        man(xoff + 20.0f * sca * dir, yoff, 0.5f * sca, !fli, true, true);

        std::vector<Pt> plist1, plist2;
        float len = 120.0f;
        float step = 5.0f * sca;
        if (step < 2.0f) step = 2.0f;
        for (float i = 0; i < len * sca; i += step) {
            float p = i / (len * sca);
            float s = powf(sinf(p * (float)M_PI), 0.5f);
            plist1.push_back(Pt(i * dir, s * 7.0f * sca));
            plist2.push_back(Pt(i * dir, s * 10.0f * sca));
        }
        std::vector<Pt> plist = plist1;
        for (int i = (int)plist2.size() - 1; i >= 0; i--) plist.push_back(plist2[i]);
        poly(plist, xoff, yoff, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
        stroke(plist, xoff, yoff, ColorRGBA(100, 100, 100, 0.45f), 1.2f, 0.5f, 1.0f,
               [](float x) { return sinf(x * (float)M_PI * 2.0f); });
    }

    void archBox(float xoff, float yoff, float wid, float hei, float rot, float per, bool tra = false, float wei = 2.0f) {
        float mid = -wid * 0.5f + wid * rot;
        std::vector<Pt> polist = {
            Pt(-wid * 0.5f, -hei),
            Pt(wid * 0.5f, -hei),
            Pt(wid * 0.5f, 0),
            Pt(mid, per),
            Pt(-wid * 0.5f, 0)
        };
        if (!tra) {
            poly(polist, xoff, yoff, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
        }
        ColorRGBA col(100, 100, 100, 0.4f);
        stroke({Pt(-wid * 0.5f, -hei), Pt(-wid * 0.5f, 0)}, xoff, yoff, col, wei);
        stroke({Pt(wid * 0.5f, -hei), Pt(wid * 0.5f, 0)}, xoff, yoff, col, wei);
        stroke({Pt(mid, -hei), Pt(mid, per)}, xoff, yoff, col, wei);
        stroke({Pt(-wid * 0.5f, 0), Pt(mid, per)}, xoff, yoff, col, wei);
        stroke({Pt(wid * 0.5f, 0), Pt(mid, per)}, xoff, yoff, col, wei);
    }

    void pagroof(float xoff, float yoff, float wid, float hei, float per, float cor = 10.0f, float wei = 2.5f) {
        int sid = 4;
        std::vector<Pt> polist = { Pt(0, -hei) };
        std::vector<std::vector<Pt>> lines;
        for (int i = 0; i < sid; i++) {
            float fx = wid * ((float)i / (sid - 1) - 0.5f);
            float fy = per * (1.0f - fabsf((float)i / (sid - 1) - 0.5f) * 2.0f);
            float fxx = (wid + cor) * ((float)i / (sid - 1) - 0.5f);
            if (i > 0) {
                lines.push_back({ polist.back(), Pt(fxx, fy) });
            }
            lines.push_back({ Pt(0, -hei), Pt(fx * 0.5f, (-hei + fy) * 0.5f), Pt(fxx, fy) });
            polist.push_back(Pt(fxx, fy));
        }
        poly(polist, xoff, yoff, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
        for (const auto& l : lines) {
            stroke(div(l, 4), xoff, yoff, ColorRGBA(100, 100, 100, 0.4f), wei);
        }
    }

    void arch02(float xoff, float yoff, float wid = 50.0f, float hei = 10.0f, int sto = 2, float rot = 0.3f, float per = 5.0f) {
        float hoff = 0;
        for (int i = 0; i < sto; i++) {
            float cw = wid * powf(0.85f, i);
            archBox(xoff, yoff - hoff, cw, hei, rot, per, false, 1.5f);
            pagroof(xoff, yoff - hoff - hei, wid * powf(0.9f, i), hei, per, 6.0f, 1.5f);
            hoff += hei * 1.5f;
        }
    }

    void arch03(float xoff, float yoff, float wid = 50.0f, int sto = 5) {
        // Pagoda on mountain peak
        float hei = 10.0f, per = 5.0f, hoff = 0;
        for (int i = 0; i < sto; i++) {
            float cw = wid * powf(0.85f, i);
            archBox(xoff, yoff - hoff, cw, hei, 0.7f, per * 0.5f, false, 1.5f);
            pagroof(xoff, yoff - hoff - hei, wid * powf(0.9f, i), hei * 1.5f, per, 8.0f, 1.5f);
            hoff += hei * 1.5f;
        }
    }

    // --- Mountains, Rocks, Water ---
    void rock(float xoff, float yoff, float seed, float hei = 80.0f, float wid = 100.0f, int tex = 30, int sha = 5) {
        int r0 = 7, r1 = 30;
        std::vector<std::vector<Pt>> ptlist(r0);
        for (int i = 0; i < r0; i++) {
            std::vector<float> nslist(r1);
            for (int j = 0; j < r1; j++) nslist[j] = noise(i, j * 0.2f, seed);
            loopNoise(nslist);
            ptlist[i].reserve(r1);
            for (int j = 0; j < r1; j++) {
                float a = ((float)j / r1) * (float)M_PI * 2.0f - (float)M_PI * 0.5f;
                float ca = cosf(a), sa = sinf(a);
                float l = (wid * hei) / sqrtf( (hei * ca) * (hei * ca) + (wid * sa) * (wid * sa) );
                l *= 0.7f + 0.3f * nslist[j];
                float p = 1.0f - (float)i / r0;
                float nx = ca * l * p;
                float ny = -sa * l * p;
                if (a > (float)M_PI || a < 0.0f) ny *= 0.2f;
                ny += hei * ((float)i / r0) * 0.2f;
                ptlist[i].push_back(Pt(nx, ny));
            }
        }
        std::vector<Pt> bg = ptlist[0];
        bg.push_back(Pt(0, 0));
        poly(bg, xoff, yoff, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
        stroke(ptlist[0], xoff, yoff, ColorRGBA(100, 100, 100, 0.3f), 3.0f, 1.0f);
        texture(ptlist, xoff, yoff, tex, 2.5f, 0.2f, sha,
                nullptr,
                [this](float x) { return ColorRGBA(180, 180, 180, 0.3f + rand01() * 0.3f); },
                [this]() { return (rand01() > 0.5f) ? 0.15f + 0.15f * rand01() : 0.85f - 0.15f * rand01(); });
    }

    void foot(const std::vector<std::vector<Pt>>& ptlist, float xof, float yof) {
        std::vector<std::vector<Pt>> ftlist;
        int span = 8;
        int ni = 0;
        int n = (int)ptlist.size();
        for (int i = 0; i < n - 2; i++) {
            if (i == ni) {
                ni = std::min(ni + randChoice({1, 2}), n - 1);
                std::vector<Pt> f1, f2;
                int mlen = std::min((int)ptlist[i].size() / 8, 8);
                int rlen = (int)ptlist[i].size();
                for (int j = 0; j < mlen; j++) {
                    f1.push_back(Pt(ptlist[i][j].x + noise(j * 0.1f, i) * 10.0f, ptlist[i][j].y));
                    f2.push_back(Pt(ptlist[i][rlen - 1 - j].x - noise(j * 0.1f, i) * 10.0f, ptlist[i][rlen - 1 - j].y));
                }
                std::reverse(f1.begin(), f1.end());
                std::reverse(f2.begin(), f2.end());
                for (int j = 0; j < span; j++) {
                    float p = (float)j / span;
                    float x1 = ptlist[i][0].x * (1.0f - p) + ptlist[ni][0].x * p;
                    float y1 = ptlist[i][0].y * (1.0f - p) + ptlist[ni][0].y * p;
                    float x2 = ptlist[i][rlen - 1].x * (1.0f - p) + ptlist[ni][rlen - 1].x * p;
                    float y2 = ptlist[i][rlen - 1].y * (1.0f - p) + ptlist[ni][rlen - 1].y * p;
                    float vib = -1.7f * (p - 1.0f) * powf(p, 0.2f);
                    y1 += vib * 5.0f + noise(xof * 0.05f, i) * 5.0f;
                    y2 += vib * 5.0f + noise(xof * 0.05f, i) * 5.0f;
                    f1.push_back(Pt(x1, y1));
                    f2.push_back(Pt(x2, y2));
                }
                ftlist.push_back(std::move(f1));
                ftlist.push_back(std::move(f2));
            }
        }
        for (const auto& f : ftlist) {
            poly(f, xof, yof, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
        }
        for (const auto& f : ftlist) {
            stroke(f, xof, yof, ColorRGBA(100, 100, 100, 0.1f + rand01() * 0.1f), 1.0f);
        }
    }

    void mountain(float xoff, float yoff, float seed) {
        float hei = 100.0f + rand01() * 380.0f;
        float wid = 380.0f + rand01() * 180.0f;
        int tex = 80;

        int r0 = 8, r1 = 30;
        std::vector<std::vector<Pt>> ptlist(r0);
        float hoff = 0;
        for (int j = 0; j < r0; j++) {
            hoff += (rand01() * yoff) / 100.0f;
            ptlist[j].reserve(r1);
            for (int i = 0; i < r1; i++) {
                float x = ((float)i / r1 - 0.5f) * (float)M_PI;
                float y = cosf(x) * noise(x + 10.0f, j * 0.15f, seed);
                float p = 1.0f - (float)j / r0;
                ptlist[j].push_back(Pt((x / (float)M_PI) * wid * p, -y * hei * p + hoff));
            }
        }

        // RIM vegetation
        for (int j = 0; j < r1; j++) {
            float ns = noise(j * 0.1f, seed);
            if (ns * ns * ns < 0.1f && fabsf(ptlist[0][j].y) / hei > 0.2f) {
                float x = ptlist[0][j].x, y = ptlist[0][j].y;
                float a = noise(0.01f * x, 0.01f * y) * 0.15f + 0.5f;
                tree02(x + xoff, y + yoff - 5.0f, 16.0f, 8.0f, 2, ColorRGBA(100, 100, 100, a));
            }
        }

        // WHITE (PAPER) BG
        std::vector<Pt> bg = ptlist[0];
        bg.push_back(Pt(0, r0 * 4.0f));
        poly(bg, xoff, yoff, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);

        // OUTLINE
        stroke(ptlist[0], xoff, yoff, ColorRGBA(100, 100, 100, 0.3f), 3.0f, 1.0f);

        // FOOT & TEXTURE
        foot(ptlist, xoff, yoff);
        texture(ptlist, xoff, yoff, tex, 1.5f, 0.2f, randChoice({0, 0, 0, 0, 5}));

        // TOP vegetation
        for (int i = 0; i < r0; i++) {
            for (int j = 0; j < r1; j++) {
                float ns = noise(i * 0.1f, j * 0.1f, seed + 2.0f);
                if (ns * ns * ns < 0.1f && fabsf(ptlist[i][j].y) / hei > 0.5f) {
                    float x = ptlist[i][j].x, y = ptlist[i][j].y;
                    float a = noise(0.01f * x, 0.01f * y) * 0.15f + 0.5f;
                    tree02(x + xoff, y + yoff, 16.0f, 8.0f, 4, ColorRGBA(100, 100, 100, a));
                }
            }
        }

        // MIDDLE vegetation (clustered tree01)
        std::vector<Pt> midVeg;
        for (int i = 0; i < r0; i++) {
            for (int j = 0; j < r1; j++) {
                float ns = noise(i * 0.2f, j * 0.05f, seed);
                if ((j % 2) && (ns * ns * ns * ns < 0.012f) && (fabsf(ptlist[i][j].y) / hei < 0.3f)) {
                    midVeg.push_back(ptlist[i][j]);
                }
            }
        }
        for (size_t i = 0; i < midVeg.size(); i++) {
            int counter = 0;
            for (size_t j = 0; j < midVeg.size(); j++) {
                if (i != j) {
                    float dx = midVeg[i].x - midVeg[j].x, dy = midVeg[i].y - midVeg[j].y;
                    if (dx * dx + dy * dy < 900.0f) counter++;
                }
            }
            if (counter > 2) {
                float x = midVeg[i].x, y = midVeg[i].y;
                float ht = ((hei + y) / hei) * 70.0f;
                ht = ht * 0.3f + rand01() * ht * 0.7f;
                float a = noise(0.01f * x, 0.01f * y) * 0.15f + 0.3f;
                tree01(x + xoff, y + yoff, ht, rand01() * 3.0f + 1.0f, ColorRGBA(100, 100, 100, a));
            }
        }

        // BOTTOM vegetation (tree03)
        for (int i = 0; i < r0; i++) {
            for (int j : {0, r1 - 1}) {
                float ns = noise(i * 0.2f, j * 0.05f, seed);
                if (ns * ns * ns * ns < 0.012f) {
                    float x = ptlist[i][j].x, y = ptlist[i][j].y;
                    float ht = ((hei + y) / hei) * 120.0f;
                    ht = ht * 0.5f + rand01() * ht * 0.5f;
                    float a = noise(0.01f * x, 0.01f * y) * 0.15f + 0.3f;
                    tree03(x + xoff, y + yoff, ht, 5.0f, ColorRGBA(100, 100, 100, a));
                }
            }
        }

        // BOTTOM ARCH
        for (int i = 1; i < r0; i++) {
            for (int j : {1, r1 - 2}) {
                float ns = noise(i * 0.2f, j * 0.05f, seed + 10.0f);
                if (ns * ns * ns * ns < 0.008f) {
                    int tt = randChoice({0, 0, 1, 1, 1, 2});
                    if (tt == 1) {
                        arch02(ptlist[i][j].x + xoff, ptlist[i][j].y + yoff, normRand(40, 70), 10.0f, randChoice({1, 2, 2, 3}), rand01());
                    } else if (tt == 2) {
                        arch02(ptlist[i][j].x + xoff, ptlist[i][j].y + yoff, 30.0f, 15.0f, randChoice({1, 1, 2}));
                    }
                }
            }
        }

        // TOP ARCH (Pagoda)
        int midJ = r1 / 2;
        if (rand01() < 0.04f) {
            arch03(ptlist[1][midJ].x + xoff, ptlist[1][midJ].y + yoff, 40.0f + rand01() * 20.0f, randChoice({5, 7}));
        }

        // BOTTOM ROCK
        for (int i = 0; i < r0; i++) {
            for (int j : {0, r1 - 1}) {
                if (rand01() < 0.1f) {
                    rock(ptlist[i][j].x + xoff, ptlist[i][j].y + yoff, seed, 20.0f + rand01() * 20.0f, 20.0f + rand01() * 20.0f, 25, 2);
                }
            }
        }
    }

    void flatMount(float xoff, float yoff, float seed, float wid, float hei, float cho) {
        int r0 = 5, r1 = 40;
        std::vector<std::vector<Pt>> ptlist(r0);
        std::vector<std::vector<Pt>> flat(r0);
        float hoff = 0;
        for (int j = 0; j < r0; j++) {
            hoff += (rand01() * yoff) / 100.0f;
            for (int i = 0; i < r1; i++) {
                float x = ((float)i / r1 - 0.5f) * (float)M_PI;
                float y = (cosf(x * 2.0f) + 1.0f) * noise(x + 10.0f, j * 0.1f, seed);
                float p = 1.0f - ((float)j / r0) * 0.6f;
                float nx = (x / (float)M_PI) * wid * p;
                float ny = -y * hei * p + hoff;
                float h = 100.0f;
                if (ny < -h * cho + hoff) {
                    ny = -h * cho + hoff;
                    if (flat[j].size() % 2 == 0) flat[j].push_back(Pt(nx, ny));
                } else {
                    if (flat[j].size() % 2 == 1 && !ptlist[j].empty()) {
                        flat[j].push_back(ptlist[j].back());
                    }
                }
                ptlist[j].push_back(Pt(nx, ny));
            }
        }

        std::vector<Pt> bg = ptlist[0];
        bg.push_back(Pt(0, r0 * 4.0f));
        poly(bg, xoff, yoff, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
        stroke(ptlist[0], xoff, yoff, ColorRGBA(100, 100, 100, 0.3f), 3.0f, 1.0f);
        texture(ptlist, xoff, yoff, 60, 2.0f, 0.2f, 0, nullptr, nullptr, [this]() {
            return (rand01() > 0.5f) ? 0.1f + 0.4f * rand01() : 0.9f - 0.4f * rand01();
        });

        std::vector<Pt> grlist1, grlist2;
        for (int i = 0; i < r0; i += 2) {
            if (flat[i].size() >= 2) {
                grlist1.push_back(flat[i].front());
                grlist2.push_back(flat[i].back());
            }
        }
        if (grlist1.empty()) return;

        float wb0 = grlist1[0].x, wb1 = grlist2[0].x;
        for (int i = 0; i < 3; i++) {
            float p = 0.8f - i * 0.2f;
            grlist1.insert(grlist1.begin(), Pt(wb0 * p, grlist1[0].y - 5.0f));
            grlist2.insert(grlist2.begin(), Pt(wb1 * p, grlist2[0].y - 5.0f));
        }
        wb0 = grlist1.back().x; wb1 = grlist2.back().x;
        for (int i = 0; i < 3; i++) {
            float p = 0.6f - i * i * 0.1f;
            grlist1.push_back(Pt(wb0 * p, grlist1.back().y + 1.0f));
            grlist2.push_back(Pt(wb1 * p, grlist2.back().y + 1.0f));
        }

        int d = 5;
        grlist1 = div(grlist1, d);
        grlist2 = div(grlist2, d);
        std::reverse(grlist1.begin(), grlist1.end());
        std::vector<Pt> grlist = grlist1;
        for (const auto& p : grlist2) grlist.push_back(p);
        grlist.push_back(grlist1[0]);

        float xmin = 1e9f, xmax = -1e9f, ymin = 1e9f, ymax = -1e9f;
        for (size_t i = 0; i < grlist.size(); i++) {
            float v = (1.0f - fabsf((float)((int)i % d) - d * 0.5f) / (d * 0.5f)) * 0.12f;
            grlist[i].x *= 1.0f - v + noise(grlist[i].y * 0.5f) * v;
            if (grlist[i].x < xmin) xmin = grlist[i].x;
            if (grlist[i].x > xmax) xmax = grlist[i].x;
            if (grlist[i].y < ymin) ymin = grlist[i].y;
            if (grlist[i].y > ymax) ymax = grlist[i].y;
        }

        poly(grlist, xoff, yoff, PAPER_COLOR, ColorRGBA(0, 0, 0, 0), 0, true);
        stroke(grlist, xoff, yoff, ColorRGBA(100, 100, 100, 0.2f), 3.0f);

        // Flat mount decorations (rocks, pines, pavilion)
        float ymid = (ymin + ymax) * 0.5f;
        int rcnt = (int)(rand01() * 3.0f) + 1;
        for (int j = 0; j < rcnt; j++) {
            rock(xoff + normRand(xmin, xmax), yoff + ymid + normRand(-8, 12), rand01() * 100.0f,
                 15.0f + rand01() * 20.0f, 20.0f + rand01() * 25.0f, 20, 2);
        }
        int tcnt = randChoice({1, 2});
        for (int i = 0; i < tcnt; i++) {
            tree04(xoff + normRand(xmin * 0.7f, xmax * 0.7f), yoff + ymid + 15.0f, normRand(140, 220), 5.0f);
        }
        if (rand01() < 0.35f) {
            arch02(xoff + normRand(xmin * 0.5f, xmax * 0.5f), yoff + ymid + 15.0f, normRand(80, 120), 18.0f, 1, 0.6f, 5.0f);
        }
    }

    void distMount(float xoff, float yoff, float seed, float hei = 150.0f, float len = 1000.0f) {
        int seg = 5;
        float span = 10.0f;
        int numBlocks = (int)(len / span / seg);
        for (int i = 0; i < numBlocks; i++) {
            std::vector<Pt> pt;
            for (int j = 0; j <= seg; j++) {
                float k = (float)(i * seg + j);
                float x = xoff + k * span;
                float y = yoff - hei * noise(k * 0.05f, seed) * powf(sinf(((float)M_PI * k) / (len / span)), 0.5f);
                pt.push_back(Pt(x, y));
            }
            for (int j = seg / 2; j >= 0; j--) {
                float k = (float)(i * seg + j * 2);
                float x = xoff + k * span;
                float y = yoff + 24.0f * noise(k * 0.05f, 2.0f, seed) * sinf(((float)M_PI * k) / (len / span));
                pt.push_back(Pt(x, y));
            }
            Pt m = midPtList(pt);
            uint8_t c = (uint8_t)std::min(250, std::max(180, (int)(noise(m.x * 0.02f, m.y * 0.02f, yoff) * 55.0f + 195.0f)));
            ColorRGBA col(c, (uint8_t)(c * 0.96f), (uint8_t)(c * 0.88f), 0.75f);
            poly(pt, 0, 0, col, col, 1.0f, false);
        }
    }

    void water(float xoff, float yoff, float seed) {
        float hei = 2.0f, len = 700.0f;
        int clu = 8;
        float yk = 0;
        for (int i = 0; i < clu; i++) {
            std::vector<Pt> pt;
            float xk = (rand01() - 0.5f) * (len / 8.0f);
            yk += rand01() * 5.0f;
            float lk = len * 0.25f + rand01() * (len * 0.25f);
            float reso = 8.0f;
            for (float j = -lk; j < lk; j += reso) {
                pt.push_back(Pt(j + xk, sinf(j * 0.2f) * hei * noise(j * 0.1f, seed) - 20.0f + yk));
            }
            stroke(pt, xoff, yoff, ColorRGBA(100, 100, 100, 0.25f + rand01() * 0.25f), 1.0f);
        }
    }
};

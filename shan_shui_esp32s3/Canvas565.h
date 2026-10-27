#pragma once
#include <Arduino.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>

// ============================================================================
// Canvas565: 16-bit RGB565 Framebuffer with Alpha-Blended Scanline Polygon
// & Anti-Aliased / Alpha-Blended Line Rasterizer for Shan-Shui-Inf
// ============================================================================

struct Pt {
    float x, y;
    Pt() : x(0), y(0) {}
    Pt(float _x, float _y) : x(_x), y(_y) {}
};

struct ColorRGBA {
    uint8_t r, g, b;
    float a; // 0.0 to 1.0
    ColorRGBA() : r(0), g(0), b(0), a(0.0f) {}
    ColorRGBA(uint8_t _r, uint8_t _g, uint8_t _b, float _a = 1.0f)
        : r(_r), g(_g), b(_b), a(_a) {}
};

// Paper color matching shan-shui-inf's parchment (245, 233, 208)
static const ColorRGBA PAPER_COLOR(245, 233, 208, 1.0f);

class Canvas565 {
public:
    int width;
    int height;
    uint16_t* buf;
    uint16_t* bgTile; // 64x64 paper texture tile
    float scale;
    float camX;
    float camY;

    // Reusable scanline edge buffer
    std::vector<float> nodeX;
    std::vector<Pt> spts;

    Canvas565(int w, int h) : width(w), height(h), scale(0.34f), camX(0), camY(75) {
        buf = (uint16_t*)ps_malloc(w * h * sizeof(uint16_t));
        if (!buf) buf = (uint16_t*)malloc(w * h * sizeof(uint16_t));
        bgTile = (uint16_t*)malloc(64 * 64 * sizeof(uint16_t));
        nodeX.reserve(256);
        spts.reserve(256);
    }

    ~Canvas565() {
        if (buf) free(buf);
        if (bgTile) free(bgTile);
    }

    static inline uint16_t to565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

    // Blend src (r,g,b) with alpha (0..255) onto dst RGB565 pixel
    static inline uint16_t blend565(uint16_t dst, uint8_t sr, uint8_t sg, uint8_t sb, uint8_t alpha) {
        if (alpha == 0) return dst;
        if (alpha >= 252) return to565(sr, sg, sb);
        uint8_t dr = (dst >> 8) & 0xF8;
        uint8_t dg = (dst >> 3) & 0xFC;
        uint8_t db = (dst << 3) & 0xF8;

        uint16_t inv = 256 - alpha;
        uint8_t rr = (sr * alpha + dr * inv) >> 8;
        uint8_t rg = (sg * alpha + dg * inv) >> 8;
        uint8_t rb = (sb * alpha + db * inv) >> 8;
        return to565(rr, rg, rb);
    }

    void initPaperTexture(std::function<float(float, float)> noiseFunc, std::function<float()> randFunc) {
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                float c = 245.0f + noiseFunc(x * 0.2f, y * 0.2f) * 10.0f - randFunc() * 16.0f;
                if (c > 255.0f) c = 255.0f;
                if (c < 200.0f) c = 200.0f;
                uint8_t r = (uint8_t)c;
                uint8_t g = (uint8_t)(c * 0.95f);
                uint8_t b = (uint8_t)(c * 0.85f);
                uint16_t col = to565(r, g, b);
                bgTile[y * 64 + x] = col;
                bgTile[y * 64 + (63 - x)] = col;
                bgTile[(63 - y) * 64 + x] = col;
                bgTile[(63 - y) * 64 + (63 - x)] = col;
            }
        }
    }

    void clearWithPaper(int scrollPxOffset) {
        for (int y = 0; y < height; y++) {
            const uint16_t* tileRow = &bgTile[(y & 63) * 64];
            uint16_t* dstRow = &buf[y * width];
            for (int x = 0; x < width; x++) {
                dstRow[x] = tileRow[(x + scrollPxOffset) & 63];
            }
        }
    }

    inline void blendHLine(int x0, int x1, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool isPaperFill = false) {
        if ((unsigned)y >= (unsigned)height) return;
        if (x0 < 0) x0 = 0;
        if (x1 >= width) x1 = width - 1;
        if (x0 > x1) return;
        uint16_t* row = &buf[y * width];
        if (isPaperFill) {
            int pxOff = (int)(camX * scale);
            const uint16_t* tileRow = &bgTile[(y & 63) * 64];
            for (int x = x0; x <= x1; x++) {
                row[x] = tileRow[(x + pxOff) & 63];
            }
        } else if (a >= 252) {
            uint16_t col = to565(r, g, b);
            for (int x = x0; x <= x1; x++) row[x] = col;
        } else {
            for (int x = x0; x <= x1; x++) {
                row[x] = blend565(row[x], r, g, b, a);
            }
        }
    }

    // World to screen transformation
    inline Pt toScreen(const Pt& p, float xof = 0, float yof = 0) const {
        return Pt(((p.x + xof) - camX) * scale, ((p.y + yof) - camY) * scale);
    }

    void drawLineScreen(float x0, float y0, float x1, float y1, const ColorRGBA& col, bool isPaper = false) {
        uint8_t a = (uint8_t)(col.a * 255.0f);
        if (a == 0 && !isPaper) return;

        int ix0 = (int)roundf(x0), iy0 = (int)roundf(y0);
        int ix1 = (int)roundf(x1), iy1 = (int)roundf(y1);

        // Trivial reject
        if ((ix0 < 0 && ix1 < 0) || (ix0 >= width && ix1 >= width) ||
            (iy0 < 0 && iy1 < 0) || (iy0 >= height && iy1 >= height)) return;

        // Clamp long lines to avoid long loops on out-of-bounds segments
        int dx = abs(ix1 - ix0), sx = ix0 < ix1 ? 1 : -1;
        int dy = -abs(iy1 - iy0), sy = iy0 < iy1 ? 1 : -1;
        if (dx + (-dy) > 1000) return;

        int err = dx + dy, e2;

        int pxOff = (int)(camX * scale);
        while (true) {
            if ((unsigned)ix0 < (unsigned)width && (unsigned)iy0 < (unsigned)height) {
                if (isPaper) {
                    buf[iy0 * width + ix0] = bgTile[(iy0 & 63) * 64 + ((ix0 + pxOff) & 63)];
                } else {
                    buf[iy0 * width + ix0] = blend565(buf[iy0 * width + ix0], col.r, col.g, col.b, a);
                }
            }
            if (ix0 == ix1 && iy0 == iy1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; ix0 += sx; }
            if (e2 <= dx) { err += dx; iy0 += sy; }
        }
    }

    void drawPolylineWorld(const std::vector<Pt>& plist, float xof, float yof, const ColorRGBA& col, float wid = 1.0f, bool isPaper = false) {
        if (plist.size() < 2) return;
        if (col.a <= 0.001f && !isPaper) return;

        for (size_t i = 0; i + 1 < plist.size(); i++) {
            Pt p0 = toScreen(plist[i], xof, yof);
            Pt p1 = toScreen(plist[i + 1], xof, yof);
            drawLineScreen(p0.x, p0.y, p1.x, p1.y, col, isPaper);
            if (wid * scale > 1.3f) {
                drawLineScreen(p0.x, p0.y + 1, p1.x, p1.y + 1, col, isPaper);
            }
        }
    }

    void fillPolygonWorld(const std::vector<Pt>& plist, float xof, float yof, const ColorRGBA& fil, const ColorRGBA& str, float wid = 0.0f, bool isPaperFill = false) {
        size_t n = plist.size();
        if (n == 0) return;
        if (n < 3) {
            if (str.a > 0.001f && wid > 0.0f) {
                drawPolylineWorld(plist, xof, yof, str, wid, false);
            }
            return;
        }

        // Transform vertices & compute bounding box using instance member spts (thread-safe!)
        float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
        spts.resize(n);
        for (size_t i = 0; i < n; i++) {
            Pt sp = toScreen(plist[i], xof, yof);
            spts[i] = sp;
            if (sp.x < minX) minX = sp.x;
            if (sp.x > maxX) maxX = sp.x;
            if (sp.y < minY) minY = sp.y;
            if (sp.y > maxY) maxY = sp.y;
        }

        // Cull off-screen polygons
        if (maxX < 0 || minX >= width || maxY < 0 || minY >= height) return;

        uint8_t fa = (uint8_t)(fil.a * 255.0f);
        if (fa > 0 || isPaperFill) {
            int yStart = std::max(0, (int)floorf(minY));
            int yEnd   = std::min(height - 1, (int)ceilf(maxY));

            if (yStart == yEnd) {
                int xStart = std::max(0, (int)roundf(minX));
                int xEnd   = std::min(width - 1, (int)roundf(maxX));
                blendHLine(xStart, xEnd, yStart, fil.r, fil.g, fil.b, fa, isPaperFill);
            } else {
                for (int y = yStart; y <= yEnd; y++) {
                    float fy = (float)y + 0.5f;
                    nodeX.clear();
                    size_t j = n - 1;
                    for (size_t i = 0; i < n; i++) {
                        float yi = spts[i].y, yj = spts[j].y;
                        if ((yi < fy && yj >= fy) || (yj < fy && yi >= fy)) {
                            nodeX.push_back(spts[i].x + (fy - yi) / (yj - yi) * (spts[j].x - spts[i].x));
                        }
                        j = i;
                    }
                    std::sort(nodeX.begin(), nodeX.end());
                    for (size_t i = 0; i + 1 < nodeX.size(); i += 2) {
                        int x0 = (int)roundf(nodeX[i]);
                        int x1 = (int)roundf(nodeX[i + 1]);
                        blendHLine(x0, x1, y, fil.r, fil.g, fil.b, fa, isPaperFill);
                    }
                }
            }
        }

        if (str.a > 0.001f && wid > 0.0f) {
            for (size_t i = 0; i + 1 < n; i++) {
                drawLineScreen(spts[i].x, spts[i].y, spts[i + 1].x, spts[i + 1].y, str, false);
            }
        }
    }
};

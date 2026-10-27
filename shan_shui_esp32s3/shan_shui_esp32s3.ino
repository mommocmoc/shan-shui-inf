#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "Canvas565.h"
#include "ShanShuiEngine.h"
#include "JingyeongEngine.h"

// ============================================================================
// Waveshare ESP32-S3-LCD-2 Hardware Pin Configuration
// Display: 2.0" IPS 240x320 (ST7789T3, SPI) -> Used in Landscape 320x240
// Hardware Button: BOOT button is on GPIO 0 (Active LOW with internal pullup)
// Touch (optional/fallback): CST816D (I2C: SDA=48, SCL=47, INT=46)
// ============================================================================
#define LCD_SCLK 39
#define LCD_MOSI 38
#define LCD_MISO 40
#define LCD_DC   42
#define LCD_CS   45
#define LCD_RST  -1
#define LCD_BL   1

#define PIN_BOOT_BTN 0

#define TP_SDA   48
#define TP_SCL   47
#define TP_INT   46
#define CST816_ADDR 0x15

Arduino_DataBus *bus = new Arduino_ESP32SPI(
    LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO, FSPI);

Arduino_GFX *gfx = new Arduino_ST7789(
    bus, LCD_RST, 1 /* rotation */, true /* IPS */, 240, 320);

static const int SCREEN_W = 320;
static const int SCREEN_H = 240;

// ============================================================================
// Dual Mode System:
// MODE 0: Chinese Shan-Shui (산수화 - 황공망 부춘산거도 스타일 남종화)
// MODE 1: Korean Jingyeong (진경산수화 - 겸재 정선 금강전도/인왕제색도 스타일)
// ============================================================================
enum ArtStyle {
    STYLE_SHAN_SHUI = 0,
    STYLE_JINGYEONG = 1
};

volatile ArtStyle currentStyle = STYLE_SHAN_SHUI;

Canvas565 *tiles[3] = { nullptr, nullptr, nullptr };
uint16_t *frameBuf = nullptr;
ShanShuiEngine engine;
JingyeongEngine krEngine(&engine);

enum ItemTag {
    // Mode 0: Shan-Shui
    TAG_DISTMOUNT = 0,
    TAG_WATER     = 1,
    TAG_MOUNT     = 2,
    TAG_FLATMOUNT = 3,
    TAG_BOAT      = 4,
    // Mode 1: Jingyeong
    TAG_KR_GRANITE  = 10,
    TAG_KR_HILL     = 11,
    TAG_KR_PAVILION = 12,
    TAG_KR_PINE     = 13
};

struct PlannedItem {
    ItemTag tag;
    float x;
    float y;
    float seed;
    float arg1;
    float arg2;
    bool rendered;
};

std::vector<PlannedItem> items;
float plannedMaxX = 0.0f;

static const float WORLD_SCALE = 0.34f;
static const float WORLD_CAM_Y = 75.0f;

void planChunk(float xmin, float xmax) {
    auto chadd = [](const PlannedItem& item, float mind = 10.0f) -> bool {
        for (const auto& existing : items) {
            if (existing.tag == item.tag && fabsf(existing.x - item.x) < mind) {
                return false;
            }
        }
        items.push_back(item);
        return true;
    };

    if (currentStyle == STYLE_SHAN_SHUI) {
        // --- Chinese Shan-Shui Planning ---
        auto ns = [](float x) {
            return std::max(engine.noise(x * 0.03f) - 0.55f, 0.0f) * 2.0f;
        };
        auto yr = [](float x) {
            return engine.noise(x * 0.01f, (float)M_PI);
        };

        float xstep = 5.0f;
        int numSteps = (int)((xmax - xmin) / xstep) + 4;
        std::vector<int> planmtx(numSteps, 0);

        for (float i = xmin; i < xmax; i += xstep) {
            if (fmodf(fabsf(i), 850.0f) < xstep) {
                PlannedItem distItem = {
                    TAG_DISTMOUNT, i, 250.0f - engine.rand01() * 40.0f,
                    engine.rand01() * 100.0f, engine.randChoice({500.0f, 850.0f, 1150.0f}), 150.0f, false
                };
                chadd(distItem, 380.0f);
            }
        }

        for (float i = xmin; i < xmax; i += xstep) {
            float maxJ = yr(i) * 480.0f;
            for (float j = 0; j < maxJ; j += 30.0f) {
                float z0 = ns(i);
                bool isMax = (z0 > 0.25f);
                if (isMax) {
                    for (float k = i - 2.0f * xstep; k <= i + 2.0f * xstep; k += xstep) {
                        if (ns(k) > z0) { isMax = false; break; }
                    }
                }
                if (isMax) {
                    float xof = i + 2.0f * (engine.rand01() - 0.5f) * 350.0f;
                    float yof = j + 300.0f;
                    float mseed = (i + j) * 0.1f + engine.rand01() * 10.0f;
                    PlannedItem mItem = { TAG_MOUNT, xof, yof, mseed, 0, 0, false };
                    if (chadd(mItem, 130.0f)) {
                        items.push_back({ TAG_WATER, xof, yof, mseed, 0, 0, false });
                        int k0 = std::max(0, (int)((xof - 180.0f - xmin) / xstep));
                        int k1 = std::min(numSteps - 1, (int)((xof + 180.0f - xmin) / xstep));
                        for (int k = k0; k <= k1; k++) planmtx[k]++;
                    }
                }
            }
        }

        int mountCount = 0;
        for (const auto& it : items) {
            if (it.tag == TAG_MOUNT && it.x >= xmin && it.x < xmax) mountCount++;
        }
        while (mountCount < 2) {
            float xof = xmin + (xmax - xmin) * (0.15f + 0.7f * engine.rand01());
            float yof = 340.0f + engine.rand01() * 260.0f;
            float mseed = engine.rand01() * 50.0f;
            PlannedItem mItem = { TAG_MOUNT, xof, yof, mseed, 0, 0, false };
            if (chadd(mItem, 120.0f)) {
                items.push_back({ TAG_WATER, xof, yof, mseed, 0, 0, false });
            }
            mountCount++;
        }

        for (float i = xmin; i < xmax; i += xstep) {
            int idx = std::min(numSteps - 1, std::max(0, (int)((i - xmin) / xstep)));
            if (planmtx[idx] == 0 && engine.rand01() < 0.03f) {
                int cnt = (int)(1 + engine.rand01() * 2.0f);
                for (int j = 0; j < cnt; j++) {
                    PlannedItem fmItem = {
                        TAG_FLATMOUNT, i + (engine.rand01() - 0.5f) * 350.0f,
                        680.0f - j * 50.0f, engine.rand01() * 6.28f,
                        500.0f + engine.rand01() * 350.0f, 0.5f + engine.rand01() * 0.2f, false
                    };
                    chadd(fmItem, 200.0f);
                }
            }
        }

        for (float i = xmin; i < xmax; i += xstep) {
            if (engine.rand01() < 0.18f) {
                float by = 320.0f + engine.rand01() * 380.0f;
                PlannedItem bItem = {
                    TAG_BOAT, i, by, engine.rand01(), by / 800.0f, engine.rand01() > 0.5f ? 1.0f : 0.0f, false
                };
                chadd(bItem, 280.0f);
            }
        }
    } else {
        // --- Korean Jingyeong (진경산수화) Planning ---
        // 1. Cleaved Granite Peaks (금강산/인왕산식 수직준 암봉)
        float gx = xmin + (xmax - xmin) * (0.2f + 0.6f * engine.rand01());
        PlannedItem peak = {
            TAG_KR_GRANITE, gx, 420.0f + engine.rand01() * 120.0f,
            engine.rand01() * 100.0f, engine.normRand(260.0f, 360.0f), engine.normRand(260.0f, 340.0f), false
        };
        chadd(peak, 260.0f);

        // 2. Rolling Earthen Hills with dotting (미점준 토산)
        float hx = xmin + (xmax - xmin) * engine.rand01();
        PlannedItem hill = {
            TAG_KR_HILL, hx, 620.0f + engine.rand01() * 80.0f,
            engine.rand01() * 100.0f, engine.normRand(350.0f, 500.0f), engine.normRand(80.0f, 130.0f), false
        };
        chadd(hill, 220.0f);

        // 3. Korean Mountain Pavilion (조선 정자와 선비)
        if (engine.rand01() < 0.35f) {
            PlannedItem pav = {
                TAG_KR_PAVILION, xmin + (xmax - xmin) * (0.3f + 0.4f * engine.rand01()),
                580.0f + engine.rand01() * 60.0f, 0, 0, 0, false
            };
            chadd(pav, 350.0f);
        }

        // 4. Clustered Korean Red Pines (적송/해송 숲)
        int pineCount = 2 + (int)(engine.rand01() * 3.0f);
        for (int p = 0; p < pineCount; p++) {
            float px = xmin + (xmax - xmin) * engine.rand01();
            float py = 540.0f + engine.rand01() * 130.0f;
            PlannedItem pine = {
                TAG_KR_PINE, px, py, 0, engine.normRand(110.0f, 180.0f), engine.normRand(5.0f, 8.0f), false
            };
            chadd(pine, 80.0f);
        }
    }
}

void drawPlannedItem(const PlannedItem& it) {
    switch (it.tag) {
        // Mode 0: Shan-Shui
        case TAG_DISTMOUNT:
            engine.distMount(it.x, it.y, it.seed, it.arg2, it.arg1);
            break;
        case TAG_WATER:
            engine.water(it.x, it.y, it.seed);
            break;
        case TAG_MOUNT:
            engine.mountain(it.x, it.y, it.seed);
            break;
        case TAG_FLATMOUNT:
            engine.flatMount(it.x, it.y, it.seed, it.arg1, 100.0f, it.arg2);
            break;
        case TAG_BOAT:
            engine.boat01(it.x, it.y, it.arg1, it.arg2 > 0.5f);
            break;

        // Mode 1: Jingyeong
        case TAG_KR_GRANITE:
            krEngine.granitePeak(it.x, it.y, it.seed, it.arg1, it.arg2);
            break;
        case TAG_KR_HILL:
            krEngine.earthenHill(it.x, it.y, it.seed, it.arg1, it.arg2);
            break;
        case TAG_KR_PAVILION:
            krEngine.koreanPavilion(it.x, it.y);
            break;
        case TAG_KR_PINE:
            krEngine.koreanPine(it.x, it.y, it.arg1, it.arg2);
            break;
    }
}

void renderWindow(Canvas565* target, float camX, int sliceScreenX, int sliceScreenW, int globalPxScroll) {
    target->scale = WORLD_SCALE;
    target->camX = camX;
    target->camY = WORLD_CAM_Y;

    float viewWorldW = (float)target->width / WORLD_SCALE;
    while (plannedMaxX < camX + viewWorldW + 800.0f) {
        planChunk(plannedMaxX, plannedMaxX + 512.0f);
        plannedMaxX += 512.0f;
    }

    // Clear slice with seamless parchment texture
    for (int y = 0; y < target->height; y++) {
        const uint16_t* tileRow = &target->bgTile[(y & 63) * 64];
        uint16_t* dstRow = &target->buf[y * target->width];
        for (int x = sliceScreenX; x < sliceScreenX + sliceScreenW; x++) {
            dstRow[x] = tileRow[(x + globalPxScroll) & 63];
        }
    }

    std::vector<const PlannedItem*> visible;
    visible.reserve(64);
    for (const auto& it : items) {
        float margin = (it.tag == TAG_DISTMOUNT) ? 1300.0f : 450.0f;
        if (it.x + margin >= camX && it.x - margin <= camX + viewWorldW) {
            visible.push_back(&it);
        }
    }

    std::sort(visible.begin(), visible.end(), [](const PlannedItem* a, const PlannedItem* b) {
        float ya = (a->tag == TAG_WATER) ? (a->y - 10000.0f) :
                   (a->tag == TAG_DISTMOUNT) ? (a->y - 5000.0f) :
                   (a->tag == TAG_KR_GRANITE) ? (a->y - 3000.0f) : a->y;
        float yb = (b->tag == TAG_WATER) ? (b->y - 10000.0f) :
                   (b->tag == TAG_DISTMOUNT) ? (b->y - 5000.0f) :
                   (b->tag == TAG_KR_GRANITE) ? (b->y - 3000.0f) : b->y;
        return ya < yb;
    });

    uint64_t savedPrng = engine.prng_s;
    engine.canvas = target;
    krEngine.canvas = target;

    for (const auto* it : visible) {
        uint32_t itemHash = (uint32_t)(it->x * 13.37f + it->y * 7.13f + it->seed * 997.0f) ^ ((uint32_t)it->tag * 0x9e3779b9U);
        uint64_t y = (itemHash * 2654435761ULL + 54321ULL) % engine.prng_m;
        while (y % engine.prng_p == 0 || y % engine.prng_q == 0 || y <= 1) {
            y = (y + 1) % engine.prng_m;
        }
        engine.prng_s = y;
        drawPlannedItem(*it);
        vTaskDelay(1);
    }

    engine.prng_s = savedPrng;

    items.erase(std::remove_if(items.begin(), items.end(), [camX](const PlannedItem& it) {
        float margin = (it.tag == TAG_DISTMOUNT) ? 1400.0f : 550.0f;
        return (it.x + margin) < camX;
    }), items.end());
}

int tileIndexBase = 0;
volatile int nextTileToRender = -1;
volatile bool tileRenderBusy = false;
volatile uint32_t currentSeed = 20261027;

void renderTileSync(int slot, int globalTileIdx) {
    float tileWorldW = (float)SCREEN_W / WORLD_SCALE;
    float camX = globalTileIdx * tileWorldW;
    int globalPxScroll = globalTileIdx * SCREEN_W;
    renderWindow(tiles[slot], camX, 0, SCREEN_W, globalPxScroll);
}

void core0RenderTask(void *param) {
    while (true) {
        if (tileRenderBusy && nextTileToRender >= 0) {
            int globalIdx = nextTileToRender;
            renderTileSync(2, globalIdx);
            nextTileToRender = -1;
            tileRenderBusy = false;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void composeFrame(int localPxOffset) {
    const uint16_t* src0 = tiles[0]->buf;
    const uint16_t* src1 = tiles[1]->buf;
    int w0 = SCREEN_W - localPxOffset;
    int w1 = localPxOffset;

    for (int y = 0; y < SCREEN_H; y++) {
        uint16_t* dst = &frameBuf[y * SCREEN_W];
        memcpy(dst, &src0[y * SCREEN_W + localPxOffset], w0 * sizeof(uint16_t));
        if (w1 > 0) {
            memcpy(&dst[w0], &src1[y * SCREEN_W], w1 * sizeof(uint16_t));
        }
    }
}

void resetLandscape(ArtStyle newStyle) {
    while (tileRenderBusy) delay(2);
    currentStyle = newStyle;
    currentSeed = (uint32_t)esp_random();
    engine.seedPRNG(currentSeed);
    items.clear();
    plannedMaxX = 0.0f;
    tileIndexBase = 0;
    renderTileSync(0, 0);
    renderTileSync(1, 1);
    nextTileToRender = 2;
    tileRenderBusy = true;
}

// Fixed smooth scroll speed without speed-cycling UI clutter
const float scrollSpeedPx = 1.0f;

// Hardware BOOT Button (GPIO 0) handling:
// - Short press (< 800ms): Toggle Style (Chinese Shan-Shui <-> Korean Jingyeong)
// - Long press (>= 800ms): Re-seed current landscape with new random seed
bool lastBtnState = HIGH;
uint32_t btnPressTime = 0;

void checkButtons() {
    bool currentBtn = digitalRead(PIN_BOOT_BTN);
    
    // Falling edge: button pressed
    if (lastBtnState == HIGH && currentBtn == LOW) {
        btnPressTime = millis();
    }
    // Rising edge: button released
    else if (lastBtnState == LOW && currentBtn == HIGH) {
        uint32_t duration = millis() - btnPressTime;
        if (duration >= 50) { // Debounce threshold
            if (duration >= 800) {
                // Long press: Re-seed current style
                resetLandscape(currentStyle);
            } else {
                // Short click: Switch Art Style
                resetLandscape(currentStyle == STYLE_SHAN_SHUI ? STYLE_JINGYEONG : STYLE_SHAN_SHUI);
            }
        }
    }
    lastBtnState = currentBtn;
}

void setup() {
    // Configure BOOT button as input with internal pullup
    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);

    gfx->begin();
    gfx->fillScreen(0xF75A);

    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);

    gfx->setTextColor(0x3186);
    gfx->setTextSize(2);
    gfx->setCursor(55, 110);
    gfx->println("Shan-Shui & Jingyeong");

    items.reserve(128);

    for (int i = 0; i < 3; i++) {
        tiles[i] = new Canvas565(SCREEN_W, SCREEN_H);
        if (!tiles[i] || !tiles[i]->buf) while (true) delay(1000);
    }
    frameBuf = (uint16_t*)heap_caps_malloc(SCREEN_W * SCREEN_H * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!frameBuf) frameBuf = (uint16_t*)ps_malloc(SCREEN_W * SCREEN_H * sizeof(uint16_t));
    if (!frameBuf) frameBuf = (uint16_t*)malloc(SCREEN_W * SCREEN_H * sizeof(uint16_t));

    currentSeed = 16888888;
    engine.seedPRNG(currentSeed);
    tiles[0]->initPaperTexture(
        [](float x, float y) { return engine.noise(x, y); },
        []() { return engine.rand01(); }
    );
    memcpy(tiles[1]->bgTile, tiles[0]->bgTile, 64 * 64 * sizeof(uint16_t));
    memcpy(tiles[2]->bgTile, tiles[0]->bgTile, 64 * 64 * sizeof(uint16_t));

    renderTileSync(0, 0);
    renderTileSync(1, 1);

    xTaskCreatePinnedToCore(core0RenderTask, "ShanShuiGen", 16384, NULL, 0, NULL, 0);
    nextTileToRender = 2;
    tileRenderBusy = true;
}

float scrollAccum = 0.0f;

void loop() {
    checkButtons();

    scrollAccum += scrollSpeedPx;

    int localPx = (int)scrollAccum;
    if (localPx >= SCREEN_W) {
        while (tileRenderBusy) delay(1);
        Canvas565* old0 = tiles[0];
        tiles[0] = tiles[1];
        tiles[1] = tiles[2];
        tiles[2] = old0;

        tileIndexBase++;
        scrollAccum -= (float)SCREEN_W;
        localPx = (int)scrollAccum;

        nextTileToRender = tileIndexBase + 2;
        tileRenderBusy = true;
    }

    composeFrame(localPx);

    // Direct, ultra-clean 60fps DMA push without ANY UI banner flicker!
    gfx->draw16bitRGBBitmap(0, 0, frameBuf, SCREEN_W, SCREEN_H);
}

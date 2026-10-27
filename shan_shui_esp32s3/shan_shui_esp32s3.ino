#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "Canvas565.h"
#include "ShanShuiEngine.h"
#include "MarioWorldEngine.h"

// ============================================================================
// Waveshare ESP32-S3-LCD-2 Hardware Pin Configuration
// Display: 2.0" IPS 240x320 (ST7789T3, SPI) -> Used in Landscape 320x240
// Hardware Button: BOOT button on GPIO 0
// ============================================================================
#define LCD_SCLK 39
#define LCD_MOSI 38
#define LCD_MISO 40
#define LCD_DC   42
#define LCD_CS   45
#define LCD_RST  -1
#define LCD_BL   1

#define PIN_BOOT_BTN 0

Arduino_DataBus *bus = new Arduino_ESP32SPI(
    LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO, FSPI);

Arduino_GFX *gfx = new Arduino_ST7789(
    bus, LCD_RST, 1 /* rotation */, true /* IPS */, 240, 320);

static const int SCREEN_W = 320;
static const int SCREEN_H = 240;

// ============================================================================
// Dual Mode System:
// MODE 0: Chinese Traditional Shan-Shui ({Shan, Shui}* 원작 수묵 산수화)
// MODE 1: Super Mario World (슈퍼마리오 월드 - 16-bit 레트로 횡스크롤 풍경)
// ============================================================================
enum ArtStyle {
    STYLE_SHAN_SHUI   = 0,
    STYLE_SUPER_MARIO = 1
};

volatile ArtStyle currentStyle = STYLE_SHAN_SHUI;

Canvas565 *tiles[3] = { nullptr, nullptr, nullptr };
uint16_t *frameBuf = nullptr;
ShanShuiEngine engine;
MarioWorldEngine marioEngine(&engine);

enum ItemTag {
    // Mode 0: Shan-Shui
    TAG_DISTMOUNT = 0,
    TAG_WATER     = 1,
    TAG_MOUNT     = 2,
    TAG_FLATMOUNT = 3,
    TAG_BOAT      = 4,
    // Mode 1: Super Mario World
    TAG_MARIO_CLOUD    = 20,
    TAG_MARIO_HILL_BG  = 21,
    TAG_MARIO_HILL_FG  = 22,
    TAG_MARIO_PIPE     = 23,
    TAG_MARIO_BLOCKS   = 24,
    TAG_MARIO_MUSHROOM = 25
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
        // --- Mode 0: Shan-Shui Planning ---
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
        // --- Mode 1: Super Mario World Planning ---
        // 1. Smiling White Clouds in the sky (y: 110..180 world coords)
        for (float cx = xmin + 80.0f; cx < xmax; cx += 240.0f) {
            if (engine.rand01() < 0.85f) {
                PlannedItem cloud = {
                    TAG_MARIO_CLOUD, cx + engine.normRand(-40.0f, 40.0f),
                    engine.normRand(110.0f, 175.0f), 0,
                    (float)engine.randChoice({2, 3, 3, 4}), 0, false
                };
                chadd(cloud, 180.0f);
            }
        }

        // 2. Large Background Rolling Hills with Eyes (y: 650 world coords)
        float bgHillX = xmin + (xmax - xmin) * (0.2f + 0.6f * engine.rand01());
        PlannedItem bgHill = {
            TAG_MARIO_HILL_BG, bgHillX, 650.0f, 0,
            engine.normRand(420.0f, 600.0f), engine.normRand(260.0f, 380.0f), false
        };
        chadd(bgHill, 350.0f);

        // 3. Foreground Hills / Bushes with Eyes
        float fgHillX = xmin + (xmax - xmin) * engine.rand01();
        PlannedItem fgHill = {
            TAG_MARIO_HILL_FG, fgHillX, 650.0f, 0,
            engine.normRand(220.0f, 340.0f), engine.normRand(120.0f, 190.0f), false
        };
        chadd(fgHill, 240.0f);

        // 4. Warp Pipes (초록색 토관 - y: 650)
        if (engine.rand01() < 0.65f) {
            float px = xmin + (xmax - xmin) * (0.15f + 0.7f * engine.rand01());
            PlannedItem pipe = {
                TAG_MARIO_PIPE, px, 650.0f, 0,
                (float)engine.randChoice({40, 55, 75}), 0, false
            };
            chadd(pipe, 200.0f);
        }

        // 5. Floating Blocks: [?] Question Mark & Brick Blocks
        if (engine.rand01() < 0.75f) {
            float bx = xmin + (xmax - xmin) * (0.2f + 0.6f * engine.rand01());
            float by = engine.normRand(360.0f, 440.0f);
            int blockCount = engine.randChoice({1, 3, 4});
            PlannedItem blocks = {
                TAG_MARIO_BLOCKS, bx, by, 0, (float)blockCount, 0, false
            };
            chadd(blocks, 220.0f);
        }

        // 6. Mushroom Platforms
        if (engine.rand01() < 0.35f) {
            float mx = xmin + (xmax - xmin) * (0.2f + 0.6f * engine.rand01());
            PlannedItem mush = {
                TAG_MARIO_MUSHROOM, mx, 650.0f, 0, engine.normRand(120.0f, 170.0f), 0, false
            };
            chadd(mush, 300.0f);
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

        // Mode 1: Super Mario World
        case TAG_MARIO_CLOUD:
            marioEngine.drawCloud(it.x, it.y, (int)it.arg1);
            break;
        case TAG_MARIO_HILL_BG:
            marioEngine.drawMarioHill(it.x, it.y, it.arg1, it.arg2, true);
            break;
        case TAG_MARIO_HILL_FG:
            marioEngine.drawMarioHill(it.x, it.y, it.arg1, it.arg2, true);
            break;
        case TAG_MARIO_PIPE:
            marioEngine.drawPipe(it.x, it.y, (int)it.arg1);
            break;
        case TAG_MARIO_BLOCKS: {
            int cnt = (int)it.arg1;
            for (int b = 0; b < cnt; b++) {
                float ox = (b - cnt * 0.5f) * 60.0f;
                bool isQ = (b % 2 == 1 || cnt == 1);
                marioEngine.drawBlock(it.x + ox, it.y, isQ);
            }
            break;
        }
        case TAG_MARIO_MUSHROOM:
            marioEngine.drawMushroomPlatform(it.x, it.y, it.arg1);
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

    if (currentStyle == STYLE_SHAN_SHUI) {
        // Clear slice with seamless parchment paper texture
        for (int y = 0; y < target->height; y++) {
            const uint16_t* tileRow = &target->bgTile[(y & 63) * 64];
            uint16_t* dstRow = &target->buf[y * target->width];
            for (int x = sliceScreenX; x < sliceScreenX + sliceScreenW; x++) {
                dstRow[x] = tileRow[(x + globalPxScroll) & 63];
            }
        }
    } else {
        // Clear slice with crisp 16-bit Super Mario Cyan Sky (#5C94FC)
        uint16_t skyColor = Canvas565::to565(92, 148, 252);
        for (int y = 0; y < target->height; y++) {
            uint16_t* dstRow = &target->buf[y * target->width];
            for (int x = sliceScreenX; x < sliceScreenX + sliceScreenW; x++) {
                dstRow[x] = skyColor;
            }
        }
    }

    std::vector<const PlannedItem*> visible;
    visible.reserve(64);
    for (const auto& it : items) {
        float margin = (it.tag == TAG_DISTMOUNT || it.tag == TAG_MARIO_HILL_BG) ? 1300.0f : 450.0f;
        if (it.x + margin >= camX && it.x - margin <= camX + viewWorldW) {
            visible.push_back(&it);
        }
    }

    std::sort(visible.begin(), visible.end(), [](const PlannedItem* a, const PlannedItem* b) {
        float ya = (a->tag == TAG_WATER || a->tag == TAG_MARIO_CLOUD) ? -10000.0f :
                   (a->tag == TAG_DISTMOUNT || a->tag == TAG_MARIO_HILL_BG) ? -5000.0f : a->y;
        float yb = (b->tag == TAG_WATER || b->tag == TAG_MARIO_CLOUD) ? -10000.0f :
                   (b->tag == TAG_DISTMOUNT || b->tag == TAG_MARIO_HILL_BG) ? -5000.0f : b->y;
        return ya < yb;
    });

    uint64_t savedPrng = engine.prng_s;
    engine.canvas = target;
    marioEngine.canvas = target;

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

    // Mario Mode: Stamp the iconic checkered soil & green grass ground floor!
    if (currentStyle == STYLE_SUPER_MARIO) {
        marioEngine.drawGround(sliceScreenX, sliceScreenW);
    }

    engine.prng_s = savedPrng;

    items.erase(std::remove_if(items.begin(), items.end(), [camX](const PlannedItem& it) {
        float margin = (it.tag == TAG_DISTMOUNT || it.tag == TAG_MARIO_HILL_BG) ? 1400.0f : 550.0f;
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

// 1.5px per frame gives a brisk, authentic Mario sidescroll feel!
const float scrollSpeedPx = 1.5f;

bool lastBtnState = HIGH;
uint32_t btnPressTime = 0;

void checkButtons() {
    bool currentBtn = digitalRead(PIN_BOOT_BTN);
    
    if (lastBtnState == HIGH && currentBtn == LOW) {
        btnPressTime = millis();
    } else if (lastBtnState == LOW && currentBtn == HIGH) {
        uint32_t duration = millis() - btnPressTime;
        if (duration >= 50) {
            if (duration >= 800) {
                // Long press: Re-seed current landscape
                resetLandscape(currentStyle);
            } else {
                // Short click: Switch Art Style (Shan-Shui <-> Super Mario World)
                resetLandscape(currentStyle == STYLE_SHAN_SHUI ? STYLE_SUPER_MARIO : STYLE_SHAN_SHUI);
            }
        }
    }
    lastBtnState = currentBtn;
}

void setup() {
    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);

    gfx->begin();
    gfx->fillScreen(0xF75A);

    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);

    gfx->setTextColor(0x3186);
    gfx->setTextSize(2);
    gfx->setCursor(45, 110);
    gfx->println("Shan-Shui & Mario");

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
    gfx->draw16bitRGBBitmap(0, 0, frameBuf, SCREEN_W, SCREEN_H);
}

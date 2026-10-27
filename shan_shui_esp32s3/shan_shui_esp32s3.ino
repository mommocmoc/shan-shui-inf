#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "Canvas565.h"
#include "ShanShuiEngine.h"

// ============================================================================
// Waveshare ESP32-S3-LCD-2 / ESP32-S3-Touch-LCD-2 Hardware Pin Configuration
// Display: 2.0" IPS 240x320 (ST7789T3, SPI) -> Used in Landscape 320x240
// Touch:   CST816D (I2C: SDA=48, SCL=47, INT=46)
// ============================================================================
#define LCD_SCLK 39
#define LCD_MOSI 38
#define LCD_MISO 40
#define LCD_DC   42
#define LCD_CS   45
#define LCD_RST  -1
#define LCD_BL   1

#define TP_SDA   48
#define TP_SCL   47
#define TP_INT   46
#define CST816_ADDR 0x15

// Display & SPI Bus
// IMPORTANT: On ESP32-S3 Arduino Core 3.x, FSPI (=0) is index 0 of _spi_bus_array!
// Passing HSPI (=2) causes an out-of-bounds access on _spi_bus_array[2]!
Arduino_DataBus *bus = new Arduino_ESP32SPI(
    LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO, FSPI);

// Rotation = 1 -> 320x240 Landscape scroll
Arduino_GFX *gfx = new Arduino_ST7789(
    bus, LCD_RST, 1 /* rotation */, true /* IPS */, 240, 320);

static const int SCREEN_W = 320;
static const int SCREEN_H = 240;

// ============================================================================
// Architecture: PSRAM Multi-Tile Ring Buffer + Dual-Core 60FPS Scroll Engine
// - Core 0 (Generator Worker): Pre-renders upcoming 320x240 landscape tiles in
//   PSRAM using ShanShuiEngine (Perlin noise mountains, trees, pavilions, boats).
// - Core 1 (Display Engine): Smoothly slides a 320x240 window at 1~2px per frame
//   across the PSRAM ring buffer and pushes to ST7789 via 40MHz SPI.
// ============================================================================

Canvas565 *tiles[3] = { nullptr, nullptr, nullptr };
uint16_t *frameBuf = nullptr; // 320x240 contiguous RGB565 frame buffer in internal SRAM for fast SPI transfer
ShanShuiEngine engine;

enum ItemTag {
    TAG_DISTMOUNT = 0,
    TAG_WATER     = 1,
    TAG_MOUNT     = 2,
    TAG_FLATMOUNT = 3,
    TAG_BOAT      = 4
};

struct PlannedItem {
    ItemTag tag;
    float x;
    float y;
    float seed;
    float arg1; // wid or len or sca
    float arg2; // cho or fli
    bool rendered;
};

std::vector<PlannedItem> items;
float plannedMaxX = 0.0f;

// World coordinate system:
// Original shan-shui-inf viewport is ~700 units tall (y: 80..780).
// With scale = 0.34f, 700 * 0.34 = 238 pixels -> fits 240px height!
static const float WORLD_SCALE = 0.34f;
static const float WORLD_CAM_Y = 75.0f;

// Plan new terrain items in [xmin, xmax] world coordinates
void planChunk(float xmin, float xmax) {
    auto ns = [](float x) {
        return std::max(engine.noise(x * 0.03f) - 0.55f, 0.0f) * 2.0f;
    };
    auto yr = [](float x) {
        return engine.noise(x * 0.01f, (float)M_PI);
    };

    auto chadd = [](const PlannedItem& item, float mind = 10.0f) -> bool {
        for (const auto& existing : items) {
            if (existing.tag == item.tag && fabsf(existing.x - item.x) < mind) {
                return false;
            }
        }
        items.push_back(item);
        return true;
    };

    float xstep = 5.0f;
    int numSteps = (int)((xmax - xmin) / xstep) + 4;
    std::vector<int> planmtx(numSteps, 0);

    // 1. Distant mountains
    for (float i = xmin; i < xmax; i += xstep) {
        if (fmodf(fabsf(i), 850.0f) < xstep) {
            PlannedItem distItem = {
                TAG_DISTMOUNT,
                i,
                250.0f - engine.rand01() * 40.0f,
                engine.rand01() * 100.0f,
                engine.randChoice({500.0f, 850.0f, 1150.0f}),
                150.0f,
                false
            };
            chadd(distItem, 380.0f);
        }
    }

    // 2. Main mountains + water
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
                    PlannedItem wItem = { TAG_WATER, xof, yof, mseed, 0, 0, false };
                    items.push_back(wItem);
                    int k0 = std::max(0, (int)((xof - 180.0f - xmin) / xstep));
                    int k1 = std::min(numSteps - 1, (int)((xof + 180.0f - xmin) / xstep));
                    for (int k = k0; k <= k1; k++) planmtx[k]++;
                }
            }
        }
    }

    // Guarantee at least 1~2 mountains per chunk so there's never an empty screen
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

    // 3. Flat mounts in open water gaps
    for (float i = xmin; i < xmax; i += xstep) {
        int idx = std::min(numSteps - 1, std::max(0, (int)((i - xmin) / xstep)));
        if (planmtx[idx] == 0 && engine.rand01() < 0.03f) {
            int cnt = (int)(1 + engine.rand01() * 2.0f);
            for (int j = 0; j < cnt; j++) {
                PlannedItem fmItem = {
                    TAG_FLATMOUNT,
                    i + (engine.rand01() - 0.5f) * 350.0f,
                    680.0f - j * 50.0f,
                    engine.rand01() * 6.28f,
                    500.0f + engine.rand01() * 350.0f,
                    0.5f + engine.rand01() * 0.2f,
                    false
                };
                chadd(fmItem, 200.0f);
            }
        }
    }

    // 4. Fishing boats in the river
    for (float i = xmin; i < xmax; i += xstep) {
        if (engine.rand01() < 0.18f) {
            float by = 320.0f + engine.rand01() * 380.0f;
            PlannedItem bItem = {
                TAG_BOAT,
                i,
                by,
                engine.rand01(),
                by / 800.0f,
                engine.rand01() > 0.5f ? 1.0f : 0.0f,
                false
            };
            chadd(bItem, 280.0f);
        }
    }
}

void drawPlannedItem(const PlannedItem& it) {
    switch (it.tag) {
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
    }
}

// Render a viewport window [camX, camX + viewWorldW] into a target Canvas565
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

    // Sort back-to-front by painter's algorithm (matching shan-shui-inf chunkloader y-sorting)
    std::sort(visible.begin(), visible.end(), [](const PlannedItem* a, const PlannedItem* b) {
        float ya = (a->tag == TAG_WATER) ? (a->y - 10000.0f) :
                   (a->tag == TAG_DISTMOUNT) ? (a->y - 5000.0f) : a->y;
        float yb = (b->tag == TAG_WATER) ? (b->y - 10000.0f) :
                   (b->tag == TAG_DISTMOUNT) ? (b->y - 5000.0f) : b->y;
        return ya < yb;
    });

    uint64_t savedPrng = engine.prng_s;
    engine.canvas = target;

    for (const auto* it : visible) {
        uint32_t itemHash = (uint32_t)(it->x * 13.37f + it->y * 7.13f + it->seed * 997.0f) ^ ((uint32_t)it->tag * 0x9e3779b9U);
        uint64_t y = (itemHash * 2654435761ULL + 54321ULL) % engine.prng_m;
        while (y % engine.prng_p == 0 || y % engine.prng_q == 0 || y <= 1) {
            y = (y + 1) % engine.prng_m;
        }
        engine.prng_s = y;
        drawPlannedItem(*it);
        // Yield 1 tick between items so Core 0 IDLE task always feeds Task Watchdog
        vTaskDelay(1);
    }

    engine.prng_s = savedPrng;

    // Aggressively prune items that have scrolled past the left margin on every tile!
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

// Compose 320x240 frame from tiles[0] and tiles[1] at local pixel offset [0..319]
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

// --- Touch Input (CST816D) ---
float scrollSpeedPx = 1.0f;
bool paused = false;
uint32_t lastTouchMs = 0;

void checkTouch() {
    Wire.beginTransmission(CST816_ADDR);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0) return;
    if (Wire.requestFrom(CST816_ADDR, 5) == 5) {
        uint8_t fingers = Wire.read();
        uint8_t xh = Wire.read();
        uint8_t xl = Wire.read();
        uint8_t yh = Wire.read();
        uint8_t yl = Wire.read();
        if (fingers > 0 && (millis() - lastTouchMs > 350)) {
            lastTouchMs = millis();
            uint16_t rawX = ((xh & 0x0F) << 8) | xl;
            uint16_t rawY = ((yh & 0x0F) << 8) | yl;
            if (rawX < 80 && rawY < 80) {
                while (tileRenderBusy) delay(2);
                currentSeed = (uint32_t)esp_random();
                engine.seedPRNG(currentSeed);
                items.clear();
                plannedMaxX = 0.0f;
                tileIndexBase = 0;
                renderTileSync(0, 0);
                renderTileSync(1, 1);
                nextTileToRender = 2;
                tileRenderBusy = true;
            } else {
                if (paused) {
                    paused = false;
                    scrollSpeedPx = 1.0f;
                } else if (scrollSpeedPx < 1.5f) {
                    scrollSpeedPx = 2.0f;
                } else if (scrollSpeedPx < 2.8f) {
                    scrollSpeedPx = 3.5f;
                } else {
                    paused = true;
                }
            }
        }
    }
}

void setup() {
    // Init Display using official Waveshare sequence
    gfx->begin();
    gfx->fillScreen(0xF75A); // Warm parchment color

    // Turn on Backlight
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);

    // Show brief startup title on screen
    gfx->setTextColor(0x3186);
    gfx->setTextSize(2);
    gfx->setCursor(65, 110);
    gfx->println("Shan Shui Inf...");

    // Init Touch I2C
    Wire.begin(TP_SDA, TP_SCL, 400000);

    // Pre-reserve item vector capacity so it never reallocates on heap
    items.reserve(128);

    // Allocate 3 conveyor tiles (320x240 each) in PSRAM + output framebuffer in internal RAM
    for (int i = 0; i < 3; i++) {
        tiles[i] = new Canvas565(SCREEN_W, SCREEN_H);
        if (!tiles[i] || !tiles[i]->buf) {
            while (true) delay(1000);
        }
    }
    frameBuf = (uint16_t*)heap_caps_malloc(SCREEN_W * SCREEN_H * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!frameBuf) frameBuf = (uint16_t*)ps_malloc(SCREEN_W * SCREEN_H * sizeof(uint16_t));
    if (!frameBuf) frameBuf = (uint16_t*)malloc(SCREEN_W * SCREEN_H * sizeof(uint16_t));

    // Seed PRNG & initialize paper texture across all 3 tiles
    currentSeed = 16888888;
    engine.seedPRNG(currentSeed);
    tiles[0]->initPaperTexture(
        [](float x, float y) { return engine.noise(x, y); },
        []() { return engine.rand01(); }
    );
    memcpy(tiles[1]->bgTile, tiles[0]->bgTile, 64 * 64 * sizeof(uint16_t));
    memcpy(tiles[2]->bgTile, tiles[0]->bgTile, 64 * 64 * sizeof(uint16_t));

    // Render initial two screens (Tile 0 and Tile 1)
    renderTileSync(0, 0);
    renderTileSync(1, 1);

    // Start Core 0 background tile renderer at priority 0 (shares cleanly with IDLE0 WDT)
    xTaskCreatePinnedToCore(core0RenderTask, "ShanShuiGen", 16384, NULL, 0, NULL, 0);
    nextTileToRender = 2;
    tileRenderBusy = true;
}

float scrollAccum = 0.0f;

void loop() {
    checkTouch();

    if (!paused) {
        scrollAccum += scrollSpeedPx;
    }

    int localPx = (int)scrollAccum;
    if (localPx >= SCREEN_W) {
        while (tileRenderBusy) {
            delay(1);
        }
        // Shift conveyor belt: Tile 0 <- Tile 1, Tile 1 <- Tile 2, Tile 2 <- old Tile 0
        Canvas565* old0 = tiles[0];
        tiles[0] = tiles[1];
        tiles[1] = tiles[2];
        tiles[2] = old0;

        tileIndexBase++;
        scrollAccum -= (float)SCREEN_W;
        localPx = (int)scrollAccum;

        // Immediately schedule next horizon tile on Core 0
        nextTileToRender = tileIndexBase + 2;
        tileRenderBusy = true;
    }

    // Compose current 320x240 viewport & push to ST7789
    composeFrame(localPx);
    gfx->draw16bitRGBBitmap(0, 0, frameBuf, SCREEN_W, SCREEN_H);
}

# {Shan, Shui}* — ESP32-S3 Standalone Edition

> **Ported to ESP32-S3 (Waveshare ESP32-S3-LCD-2) with Google Antigravity + Gemini 4 Argon**
>
> An embedded C++ port of [LingDong-'s `{Shan, Shui}*` (`shan-shui-inf`)](https://github.com/LingDong-/shan-shui-inf) that procedurally generates and infinitely scrolls traditional Chinese landscape paintings in real time on a 2-inch IPS display (`320×240` RGB565, ST7789T3) powered by an **ESP32-S3** dual-core microcontroller.

---

## ✨ ESP32-S3 Port Highlights

The original `shan-shui-inf` renders thousands of translucent SVG polygons in a desktop browser DOM. Running the procedural Perlin-noise mountain, tree, architecture, and brush-stroke algorithms directly per frame on a microcontroller would drop below 1–3 FPS.

Using **Antigravity + Gemini 4 Argon**, this project ports the procedural generation pipeline to standalone C++ (`Arduino-ESP32` + `Arduino_GFX`) and pairs it with a **Dual-Core PSRAM Tile Conveyor Engine** for smooth, uninterrupted **60 FPS auto-scrolling** with **dual procedural visual styles**:

1. **Dual Procedural Visual Engines**
   - **Original Shan-Shui Style ([`shan_shui_esp32s3/ShanShuiEngine.h`](shan_shui_esp32s3/ShanShuiEngine.h))**: Blum-Blum-Shub PRNG, 4-Octave Perlin Noise, layered mountain peaks with cunfa (皴法) ink-wash texturing, footings, plateaus, misty foliage, cliffside pines, pagodas, robed boatmen, and rippling water.
   - **Super Mario World Style ([`shan_shui_esp32s3/MarioWorldEngine.h`](shan_shui_esp32s3/MarioWorldEngine.h))**: 16-bit retro procedural landscape featuring bright cyan sky (`#5C94FC`), iconic smiling clouds with eyes, rolling green hills with friendly eyes, mushroom platforms, question mark (`[?]`) and brick blocks, green warp pipes, and checkered soil & grass ground floor.
2. **RGB565 Scanline Polygon & Ink-Wash Alpha Rasterizer ([`shan_shui_esp32s3/Canvas565.h`](shan_shui_esp32s3/Canvas565.h))**
   - Custom software scanline polygon filler and Bresenham line rasterizer with real-time **alpha blending** over a procedurally generated 64×64 seamless rice-paper (`PAPER_COLOR`) texture tile.
3. **Dual-Core PSRAM Tile Conveyor Belt ([`shan_shui_esp32s3/shan_shui_esp32s3.ino`](shan_shui_esp32s3/shan_shui_esp32s3.ino))**
   - **8MB Octal PSRAM Ring Buffer**: Maintains three `320×240` RGB565 landscape tiles (`tiles[0..2]`) in PSRAM plus an internal DMA-capable SRAM framebuffer (`frameBuf`).
   - **Core 0 (Generator Worker)**: Pre-renders the upcoming horizon tile (`tiles[2]`) in the background while yielding cleanly to the FreeRTOS idle watchdog.
   - **Core 1 (60 FPS Scroll Engine)**: Composes a sliding `320×240` viewport across `tiles[0]` and `tiles[1]` in `<0.2 ms` via `memcpy` and streams it over `FSPI` to the ST7789T3 display without any UI flickering.
4. **Interactive Controls (Touch & Hardware BOOT Button)**
   - **Tap Screen (CST816D)**: Instant **Pause / Resume** toggle without any flickering HUD overlay.
   - **BOOT Button Short Press (< 800ms)**: Toggle art styles instantly between Traditional Shan-Shui and Super Mario World.
   - **BOOT Button Long Press (≥ 800ms)**: Re-seed the landscape with a new random seed.

---

## 🛠️ Hardware & Pinout

* **Board**: [Waveshare ESP32-S3-LCD-2 / ESP32-S3-Touch-LCD-2](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-2)
* **MCU**: ESP32-S3 (Dual-Core Xtensa LX7 @ 240MHz, 8MB Octal PSRAM, 16MB Flash)
* **Display**: 2.0" IPS LCD (`240×320`, ST7789T3 SPI driver, used in `320×240` landscape mode)
* **Touch Controller**: CST816D Capacitive Touch (Tap anywhere to Pause / Resume)
* **Hardware Button**: BOOT button on `GPIO 0` (Short click: style switch / Long click: re-seed)

| Signal | ESP32-S3 GPIO | Description |
| :--- | :--- | :--- |
| `LCD_SCLK` | `GPIO 39` | SPI Clock (`FSPI`) |
| `LCD_MOSI` | `GPIO 38` | SPI MOSI |
| `LCD_MISO` | `GPIO 40` | SPI MISO |
| `LCD_DC` | `GPIO 42` | Data / Command |
| `LCD_CS` | `GPIO 45` | Chip Select |
| `LCD_BL` | `GPIO 1` | Backlight Control |
| `TP_SDA` | `GPIO 48` | Touch I2C SDA |
| `TP_SCL` | `GPIO 47` | Touch I2C SCL |

---

## 🚀 Build & Flash (`arduino-cli`)

### Prerequisites
- `esp32:esp32` Arduino Core (`>= 3.x`)
- `GFX Library for Arduino` (`Arduino_GFX`)

### Compile & Upload
```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=default,FlashSize=16M,PSRAM=opi \
  -u -p /dev/cu.usbmodem21101 \
  ./shan_shui_esp32s3
```

---

## 📜 Original Web Version (`{Shan, Shui}*`)

Procedurally-generated vector-format infinitely-scrolling Chinese landscape for the browser by [LingDong-](https://github.com/LingDong-/shan-shui-inf).
Generate your own on https://lingdong-.github.io/shan-shui-inf/ (or [Alternative link](https://shan-shui-inf.glitch.me)).

Some examples:
![Screenshot1](/screenshots/screen001.jpg?raw=true "")
![Screenshot2](/screenshots/screen002.jpg?raw=true "")

{Shan, Shui}\* is inspired by [traditional Chinese landscape scrolls](https://en.wikipedia.org/wiki/Shan_shui) (such as [this](https://en.wikipedia.org/wiki/Dwelling_in_the_Fuchun_Mountains) and [this](https://en.wikipedia.org/wiki/Wang_Ximeng)) and uses noises and mathematical functions to model the mountains and trees from scratch.

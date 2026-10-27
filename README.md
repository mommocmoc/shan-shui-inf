# {Shan, Shui}* — ESP32-S3 Standalone Edition

> **Ported to ESP32-S3 (Waveshare ESP32-S3-LCD-2) with Google Antigravity + Gemini 4 Argon**
>
> An embedded C++ port of [LingDong-'s `{Shan, Shui}*` (`shan-shui-inf`)](https://github.com/LingDong-/shan-shui-inf) that procedurally generates and infinitely scrolls traditional Chinese landscape paintings in real time on a 2-inch IPS display (`320×240` RGB565, ST7789T3) powered by an **ESP32-S3** dual-core microcontroller.

---

## ✨ ESP32-S3 Port Highlights

The original `shan-shui-inf` renders thousands of translucent SVG polygons in a desktop browser DOM. Running the procedural Perlin-noise mountain, tree, architecture, and brush-stroke algorithms directly per frame on a microcontroller would drop below 1–3 FPS.

Using **Antigravity + Gemini 4 Argon**, this project ports the full procedural generation pipeline to standalone C++ (`Arduino-ESP32` + `Arduino_GFX`) and pairs it with a **Dual-Core PSRAM Tile Conveyor Engine** for smooth, uninterrupted **60 FPS auto-scrolling**:

1. **Full C++ Procedural Shan-Shui Generator ([`shan_shui_esp32s3/ShanShuiEngine.h`](shan_shui_esp32s3/ShanShuiEngine.h))**
   - **Blum-Blum-Shub PRNG & 4-Octave Perlin Noise**: Deterministic seed-based landscape generation matching the original JS implementation.
   - **Landforms & Cunfa (皴法) Brushwork**: Layered distant mountains (`distMount`), main peaks (`mountain`) with hundreds of textured ink strokes (`texture`), mountain feet (`foot`), flat-topped plateaus (`flatMount`), rocks (`rock`), and rippling water (`water`).
   - **Flora, Architecture & Figures**: Ridge/mist foliage (`tree01`, `tree02`, `tree03`), gnarly cliffside pines (`tree04`), multi-story pavilions (`arch02`), mountain-peak pagodas (`arch03`), and river boats with robed oarsmen (`boat01`, `man`).
2. **RGB565 Scanline Polygon & Ink-Wash Alpha Rasterizer ([`shan_shui_esp32s3/Canvas565.h`](shan_shui_esp32s3/Canvas565.h))**
   - Custom software scanline polygon filler and Bresenham line rasterizer with real-time **alpha blending** over a procedurally generated 64×64 seamless rice-paper (`PAPER_COLOR`) texture tile.
3. **Dual-Core PSRAM Tile Conveyor Belt ([`shan_shui_esp32s3/shan_shui_esp32s3.ino`](shan_shui_esp32s3/shan_shui_esp32s3.ino))**
   - **8MB Octal PSRAM Ring Buffer**: Maintains three `320×240` RGB565 landscape tiles (`tiles[0..2]`) in PSRAM plus an internal DMA-capable SRAM framebuffer (`frameBuf`).
   - **Core 0 (Generator Worker)**: Pre-renders the upcoming horizon tile (`tiles[2]`) in the background while yielding cleanly to the FreeRTOS idle watchdog.
   - **Core 1 (60 FPS Scroll Engine)**: Composes a sliding `320×240` viewport across `tiles[0]` and `tiles[1]` in `<0.2 ms` via `memcpy` and streams it over `FSPI` to the ST7789T3 display.

---

## 🛠️ Hardware & Pinout

* **Board**: [Waveshare ESP32-S3-LCD-2 / ESP32-S3-Touch-LCD-2](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-2)
* **MCU**: ESP32-S3 (Dual-Core Xtensa LX7 @ 240MHz, 8MB Octal PSRAM, 16MB Flash)
* **Display**: 2.0" IPS LCD (`240×320`, ST7789T3 SPI driver, used in `320×240` landscape mode)
* **Touch (Optional)**: CST816D Capacitive Touch (`I2C`)
  * **Tap anywhere**: Cycle scroll speed (`1.0x` → `2.0x` → `3.5x` → `Pause`)
  * **Tap top-left corner**: Generate a brand-new landscape with a new random seed

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

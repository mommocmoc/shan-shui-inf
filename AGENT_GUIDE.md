# 🤖 AGENT_GUIDE.md — {Shan, Shui}* ESP32-S3 Pomodoro Builder

> **For AI Coding Agents (Google Antigravity, Claude Code, Cursor, Codex):**
> When a user asks you to set up, compile, flash, or customize this project (`shan-shui-inf`), follow this step-by-step automated workflow so that even a non-developer can get their **Shan-Shui & 16-Bit Mario Pomodoro Desk Timer** running on their **Waveshare ESP32-S3-Touch-LCD-2** board in under 15 minutes.

---

## 🎯 Target Hardware & Architecture Summary

- **Board:** Waveshare ESP32-S3-Touch-LCD-2 (ESP32-S3 Dual-Core @ 240MHz, **8MB Octal PSRAM**, 16MB Flash)
- **Display:** 2.0" IPS LCD (`320×240` landscape mode, `ST7789T3` SPI driver on `FSPI`: SCLK=39, MOSI=38, MISO=40, DC=42, CS=45, BL=1)
- **Touch Controller:** `CST816D` Capacitive Touch (I2C: SDA=48, SCL=47)
- **Hardware Button:** `BOOT` button (`GPIO 0` — Short press: toggle Shan-Shui ↔ Super Mario World; Long press ≥800ms: re-seed & reset timer)
- **Firmware Directory:** `./shan_shui_esp32s3/`
  - [`shan_shui_esp32s3.ino`](shan_shui_esp32s3/shan_shui_esp32s3.ino): Dual-Core 8MB PSRAM Tile Conveyor Engine (Core 0 background generator + Core 1 60FPS scroll & touch/button loop)
  - [`ShanShuiEngine.h`](shan_shui_esp32s3/ShanShuiEngine.h): Traditional ink-wash landscape procedural generator (Perlin noise mountains, cunfa strokes, pines, pagodas, boatmen)
  - [`MarioWorldEngine.h`](shan_shui_esp32s3/MarioWorldEngine.h): 16-bit Super Mario World procedural generator (cyan sky, smiling clouds/hills, `[?]` blocks, warp pipes)
  - [`PomodoroTimer.h`](shan_shui_esp32s3/PomodoroTimer.h): 20-minute Pomodoro state machine (`POMO_IDLE`, `POMO_FOCUS`, `POMO_PAUSED`, `POMO_DONE`), 2px top progress hairline, and completion seal/emblem renderer
  - [`Canvas565.h`](shan_shui_esp32s3/Canvas565.h): RGB565 scanline polygon rasterizer with rice-paper texture alpha blending

---

## 🚀 Automated 4-Step Setup & Flash Procedure

When the user asks you to install or flash this project to their board, execute the following steps automatically:

### Step 1. Check & Install `arduino-cli`
1. Check if `arduino-cli` is installed:
   ```bash
   which arduino-cli
   ```
2. If not installed:
   - **macOS (Homebrew):** `brew install arduino-cli`
   - **Windows (winget):** `winget install ArduinoSA.CLI`
   - **Linux / Fallback:** `curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh`

### Step 2. Install ESP32 Core & `Arduino_GFX` Library
Run these commands to ensure the ESP32-S3 core and display driver library are installed:
```bash
arduino-cli config init --overwrite || true
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "GFX Library for Arduino"
```

### Step 3. Detect Connected ESP32-S3 Serial Port
List connected serial ports to find the Waveshare ESP32-S3 board:
```bash
arduino-cli board list
```
- **macOS:** Look for `/dev/cu.usbmodem*` or `/dev/cu.wchusbserial*`
- **Windows:** Look for `COM*` (e.g., `COM3`, `COM4`)
- **Linux:** Look for `/dev/ttyACM*` or `/dev/ttyUSB*`
- *If no port is detected, politely ask the user to plug in their Waveshare ESP32-S3-Touch-LCD-2 board using a data-capable USB-C cable (or hold `BOOT` while plugging in if the board is in a boot loop).*

### Step 4. Compile & Flash with Octal PSRAM Enabled
> ⚠️ **CRITICAL:** The Waveshare ESP32-S3-Touch-LCD-2 uses **8MB Octal PSRAM (`PSRAM=opi`)**. Always pass `PSRAM=opi` in `--fqbn`, otherwise `ps_malloc()` for the 3-tile conveyor buffer will fail.

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=default,FlashSize=16M,PSRAM=opi \
  -u -p <DETECTED_SERIAL_PORT> \
  ./shan_shui_esp32s3
```

---

## 🎨 Customization Guide (When the User Asks to Personalize)

1. **Change Pomodoro Focus Duration (Default: 20 Minutes):**
   - Open [`shan_shui_esp32s3/PomodoroTimer.h`](shan_shui_esp32s3/PomodoroTimer.h)
   - Update `FOCUS_DURATION_MS` (e.g. `25UL * 60UL * 1000UL` for 25 minutes) and update the badge strings (`"25M FOCUS"`, `"25M REST"`, `"* 25 MIN COMPLETE! *"`).
2. **Adjust Scroll Speed (Default: 60 FPS, 1px/frame):**
   - Open [`shan_shui_esp32s3/shan_shui_esp32s3.ino`](shan_shui_esp32s3/shan_shui_esp32s3.ino)
   - Adjust the frame delay or scroll step inside `loop()` to make the landscape flow slower and more meditative or faster.
3. **Change Default Boot Style (Shan-Shui vs. Super Mario World):**
   - In [`shan_shui_esp32s3/shan_shui_esp32s3.ino`](shan_shui_esp32s3/shan_shui_esp32s3.ino), change `currentStyle = STYLE_SHAN_SHUI;` to `STYLE_MARIO` if the user wants Mario World at boot.

# ESP32-DOOM

A port of PrBoom (Doom engine) to the ESP32-WROVER module with external PSRAM. Play the classic DOOM on an ESP32 with either a direct SPI LCD display or stream video over WiFi.

![ESP32-DOOM](https://img.shields.io/badge/ESP32-DOOM-red?style=for-the-badge)
![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.3-blue?style=for-the-badge)
![License](https://img.shields.io/badge/License-GPL%20v2-green?style=for-the-badge)

## Demo

**On-device capture (SPI LCD):**

<img src="assets/demp.gif" alt="ESP32-DOOM on-device demo" width="480" />

**Web streaming preview (browser):**

<img src="assets/web-demo.gif" alt="ESP32-DOOM web preview" width="640" />

## Features

- **Dual Display Modes**:
  - **SPI LCD**: Direct output to ST7789V (240×280) or ILI9341 (320×240) displays
  - **WiFi Streaming**: Stream video over HTTP SSE to a Python client
- **Serial Keyboard Input**: Low-latency input via serial port (115200 baud)
- **Full DOOM Gameplay**: Complete shareware DOOM experience
- **PSRAM Support**: Utilizes 8MB external PSRAM for game data

## Hardware Requirements

### Minimum
- **ESP32-WROVER** module (with 8MB PSRAM)
- USB cable for programming and serial input

### For LCD Display Mode
- **ST7789V** 240×280 SPI TFT display (recommended), or
- **ILI9341** 320×240 SPI TFT display

### Wiring (ST7789V to ESP32-WROVER)

| Display Pin | ESP32 Pin | Description |
|-------------|-----------|-------------|
| GND         | GND       | Ground      |
| VCC         | 3.3V      | Power (3.3V)|
| SCL         | GPIO 18   | SPI Clock   |
| SDA         | GPIO 23   | SPI MOSI    |
| RES         | GPIO 21   | Reset       |
| DC          | GPIO 22   | Data/Command|
| CS          | GPIO 5    | Chip Select |
| BLK         | 3.3V      | Backlight   |

## Software Requirements

- [ESP-IDF v5.3+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
- Python 3.8+ (for the player client)
- DOOM WAD file (shareware `DOOM1.WAD` included in `data/`)

## Quick Start

### 1. Clone and Setup

```bash
git clone https://github.com/AmirhoseinMasoumi/ESP32-DOOM.git
cd ESP32-DOOM
```

### 2. Configure WiFi (for streaming mode)

Edit `sdkconfig.defaults` or use `idf.py menuconfig`:
```
CONFIG_WIFI_SSID="YourWiFiName"
CONFIG_WIFI_PASSWORD="YourPassword"
```

### 3. Build and Flash

**Using PowerShell (Windows):**
```powershell
.\make.ps1 build
.\make.ps1 flash
```

**Using Make (Linux/macOS):**
```bash
make build
make flash
```

**Using ESP-IDF directly:**
```bash
idf.py build
idf.py -p COM3 flash
```

### 4. Upload WAD File

```powershell
.\upload_wad.ps1
```

Or manually upload `data/DOOM1.WAD` to the ESP32's SPIFFS partition.

### 5. Run the Player Client

**For LCD display (keys only):**
```bash
pip install -r requirements.txt
python doom_player.py --keys-only --port COM3
```

**For WiFi streaming:**
```bash
python doom_player.py --port COM3 --ip 192.168.1.xxx
```

## Controls

| Key | Action |
|-----|--------|
| ↑ / W | Move Forward |
| ↓ / S | Move Backward |
| ← / A | Turn Left |
| → / D | Turn Right |
| Ctrl | Fire |
| Space | Use/Open |
| Shift | Run |
| Alt | Strafe |
| Tab | Automap |
| Escape | Menu |
| Enter | Select |
| 1-7 | Weapon Select |

## Configuration

### Display Type

Edit `prboom-esp32-compat/spi_lcd.c`:
```c
#define CONFIG_HW_LCD_TYPE  1   // 0=ILI9341 320x240, 1=ST7789V 240x280
```

### GPIO Pins

```c
#define CONFIG_HW_LCD_MOSI_GPIO  23
#define CONFIG_HW_LCD_CLK_GPIO   18
#define CONFIG_HW_LCD_CS_GPIO    5
#define CONFIG_HW_LCD_DC_GPIO    22
#define CONFIG_HW_LCD_RESET_GPIO 21
```

### SPI Clock Speed

Default: 40MHz (stable). Can try up to 80MHz with good wiring:
```c
.clock_speed_hz = 40000000,
```

## Project Structure

```
ESP32-DOOM/
├── prboom/              # PrBoom DOOM engine source
├── prboom-esp32-compat/ # ESP32 platform layer
│   ├── spi_lcd.c        # SPI display driver
│   ├── doom_stream.c    # Video streaming & serial input
│   └── ...
├── prboom-wad-tables/   # WAD data tables
├── main/                # ESP32 main component
├── data/                # WAD files
│   └── DOOM1.WAD        # Shareware DOOM
├── doom_player.py       # Python client (display + input)
├── make.ps1             # PowerShell build script
├── upload_wad.ps1       # WAD upload script
└── requirements.txt     # Python dependencies
```

## Performance

- **Resolution**: 320×200 (scaled to display)
- **Frame Rate**: ~6-10 FPS (limited by game engine, not display)
- **Memory**: Uses ~4MB PSRAM for game data

## Troubleshooting

### No display output
- Check wiring connections
- Verify `CONFIG_HW_LCD_TYPE` matches your display
- Try reducing SPI clock to 26MHz

### Distorted/garbled display
- Reduce SPI clock speed
- Check for loose connections
- Verify 3.3V power supply is stable

### Keys not working
- Ensure serial port is correct (`--port COM3`)
- Check baud rate is 115200
- Close any other serial monitors

### WiFi streaming issues
- Verify WiFi credentials in `sdkconfig`
- Check ESP32 IP address in serial output
- Ensure PC and ESP32 are on same network

## Credits

- **DOOM**: id Software
- **PrBoom**: PrBoom team
- **ESP32 Port**: Based on esp32-doom by [Espressif](https://github.com/espressif)

## License

This project is licensed under the GNU General Public License v2.0 - see the [COPYING](prboom/COPYING) file for details.

DOOM® is a registered trademark of id Software LLC.

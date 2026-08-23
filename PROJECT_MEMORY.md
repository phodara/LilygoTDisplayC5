<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# Project Memory

## Board

- LilyGO T-Display C5, ESP32-C5 based.
- Built-in display is ST7789, configured through LovyanGFX.
- Current project uses PlatformIO with Arduino framework.
- USB-C is used for programming, serial monitor, and power.

## Current Firmware

- Main app is a portable WiFi analyzer.
- It scans nearby WiFi networks and shows SSID, RSSI, channel, security, channel load, and battery status.
- The two top buttons are used for selection, view cycling, BLE scan-mode toggling, and WiFi/Bluetooth mode switching.
- Button command details are documented in `docs/button-commands.md`.

## Known Pins In This Repo

These are defined in `include/pin_config.h`.

| Function | GPIO |
| --- | ---: |
| LCD SCK | 7 |
| LCD MOSI | 9 |
| LCD CS | 26 |
| LCD DC | 8 |
| LCD RST | 23 |
| LCD Backlight | 25 |
| I2C SDA | 2 |
| I2C SCL | 3 |
| Top Button Previous | 0 |
| Top Button Next | 28 |
| PMU INT | 10 |
| Touch INT | 27 |
| Touch RST | 24 |

## Small Connectors Near USB-C

The two small connectors on either side of the USB-C port are useful expansion ports.

Left connector:

```text
GND
5V
3V3
TXD
RXD
```

This is useful for UART devices such as GPS modules, serial sensors, RFID/NFC readers, or a TTL magnetic stripe reader.

Right connector:

```text
GND
3V3
SDA
SCL
```

This is useful for I2C sensors such as BME280, SHT31, BMP280, BH1750, SGP30, SCD40/SCD41, RTC modules, and Qwiic/Stemma QT boards if the wiring matches.

## Project Ideas Discussed

- Add an environmental panel with temperature, humidity, pressure, air quality, or CO2 over I2C.
- Add a buzzer or LED indicator for WiFi signal strength.
- Add GPS over UART and log WiFi scan results by location.
- Add a UART magnetic stripe reader for safe test-card experiments. Avoid storing or displaying full payment/access card data.
- Try ESP32 Marauder on the board. ESP32-C5 support exists, but the LilyGO T-Display C5 display/buttons would probably need custom board support for a polished build.
- Build an IR remote learner/emulator using an IR receiver and IR LED transmitter on spare GPIO pins.
- Add configurable LCD backlight brightness using the AW9364 1-wire dimming driver on GPIO 25. See `docs/display-power-management.md`.

## Safety And Wiring Notes

- ESP32 GPIO logic is 3.3V.
- Do not feed 5V signals into GPIO pins.
- Use 3V3 for most sensors unless a module explicitly needs 5V and has 3.3V-safe logic.
- Avoid reusing pins already used by the display, I2C bus, buttons, PMU interrupt, or touch hardware unless intentionally sharing a bus.
- For I2C devices, multiple modules can share SDA/SCL if their addresses do not conflict.

## Git Notes

- Remote is `origin` at `https://github.com/phodara/LilygoTDisplayC5`.
- Main branch was checked and was synced with `origin/main` when this note was created.
- VS Code may show the repo as changed if `.vscode/extensions.json` has local edits.

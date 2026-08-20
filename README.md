# LilyGO T-Display C5 WiFi Analyzer

PlatformIO WiFi analyzer for the LilyGO T-Display C5.

## What It Does

- Initializes Serial over USB CDC at `115200`.
- Turns on the LCD backlight.
- Initializes the ST7789 display through `LovyanGFX`.
- Starts I2C on the board pins.
- Scans nearby WiFi networks every few seconds.
- Shows SSID, RSSI, signal bar, channel, band, and security.
- Draws a compact RSSI history graph for the selected access point.
- Uses the two top buttons to move previous/next through network details.
- Prints the scan table to Serial Monitor.

Passive WiFi scans can see RSSI, channel, band, and security, but they do not reliably expose internet throughput or AP channel width. The display therefore shows channel load as the practical bandwidth-health indicator and marks raw scan bandwidth as `n/a`.

## Feature Notes

- [Bluetooth feature plan](docs/bluetooth-feature.md)
- [Bluetooth scanner feature plan](docs/bluetooth-scanner.md)
- [RSSI reference](docs/rssi-reference.md)

## Build

```sh
pio run
```

## Upload

```sh
pio run --target upload
```

If upload does not start automatically, hold `BOOT`, tap `RST`, release `RST`, then release `BOOT`.

## Monitor

```sh
pio device monitor
```

## Key Pins

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
| Touch INT | 27 |
| Touch RST | 24 |

The pin values are kept in `include/pin_config.h`.

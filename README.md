

<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# LilyGO T-Display C5 WiFi Analyzer

PlatformIO WiFi analyzer for the LilyGO T-Display C5.
<p align="center">
  <img src="https://github.com/user-attachments/assets/04c630fe-b077-48b6-a377-c4726b4ede97" width="500" alt="LilyGO T-Display C5 WiFi Analyzer">
</p>

## What It Does

- Initializes Serial over USB CDC at `115200`.
- Turns on the LCD backlight.
- Initializes the ST7789 display through `LovyanGFX`.
- Starts I2C on the board pins.
- Scans nearby WiFi networks every few seconds.
- Shows SSID, RSSI, signal bar, channel, band, and security.
- Draws a compact RSSI history graph for the selected access point.
- Scans Bluetooth Low Energy devices in a separate Bluetooth mode.
- Supports passive and active BLE scan modes.
- Keeps intermittent BLE devices visible briefly after their last advertisement.
- De-duplicates named BLE devices that rotate addresses.
- Reduces display flicker by redrawing only changed screen regions.
- Uses the upper button to scroll through the current WiFi/BLE list.
- Uses the lower button to toggle detail/RSSI history for the highlighted item.
- Uses both buttons held together to switch between WiFi and Bluetooth modes.
- Prints the scan table to Serial Monitor.

Passive WiFi scans can see RSSI, channel, band, and security, but they do not reliably expose internet throughput or AP channel width. The display therefore shows channel load as the practical bandwidth-health indicator and marks raw scan bandwidth as `n/a`.

## Button Controls

| Control | WiFi mode | Bluetooth mode |
| --- | --- | --- |
| Upper button | Select next access point | Select next BLE device |
| Lower button | Toggle RSSI history / BSSID detail | Toggle RSSI history / address detail |
| Both buttons tapped | No action | Toggle passive/active BLE scan |
| Both buttons held | Switch to Bluetooth mode | Switch to WiFi mode |

The selected item wraps around when it reaches the end of the list. The both-button mode switch uses a short hold to avoid accidental mode changes while scrolling.

## Version 2 Improvements

Version 2 adds a practical Bluetooth scanner mode and display redraw improvements:

- BLE scanner mode shows nearby BLE names, address details, RSSI, signal bars, last-seen age, and RSSI history.
- Passive scan mode listens only for advertisements.
- Active scan mode requests scan-response data, which can reveal names for some BLE devices.
- BLE entries remain visible for 60 seconds after last seen, which helps with low-power devices that advertise intermittently.
- Named BLE devices are de-duplicated when they appear with changing private addresses.
- Display flicker is reduced by avoiding full-screen clears during routine updates.

## Feature Notes

- [Changelog](CHANGELOG.md)
- [Bluetooth feature plan](docs/bluetooth-feature.md)
- [Bluetooth scanner feature plan](docs/bluetooth-scanner.md)
- [Display flicker fix](docs/display-flicker-fix.md)
- [RSSI reference](docs/rssi-reference.md)
- [WiFi region behavior](docs/wifi-region-behavior.md)

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

## Release Helper

After updating code and documentation for a new version, the reusable release helper can commit, tag, and push the staged release files:

```sh
./scripts/release.sh v2.0.1 "Release v2.0.1 BLE scanner refinements"
```

The script expects a semantic version tag such as `v2.0.1`. Edit `scripts/release.sh` if a future release needs to include different files.

## License

Copyright (c) 2026 Paul Hodara.

This project is licensed under the MIT License. See [LICENSE](LICENSE).

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

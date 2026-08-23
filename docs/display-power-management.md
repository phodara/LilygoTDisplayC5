<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# Display Power Management

## Current Firmware Behavior

The current firmware turns the LCD backlight fully on during startup:

```cpp
pinMode(LCD_BLK_POWER, OUTPUT);
digitalWrite(LCD_BLK_POWER, HIGH);
```

`LCD_BLK_POWER` is GPIO 25. In LilyGO's T-Display C5 schematic this signal is
`LCD_BLK`, connected to the `EN` pin of an AW9364DNR backlight LED driver.

## Hardware Notes

- The display is a 1.9 inch ST7789 SPI LCD.
- The LCD module datasheet lists the backlight as 3 white LEDs.
- The module backlight table gives `IL = 60 mA` for the listed luminance case.
- The module absolute maximum table lists backlight forward current at `80 mA`.
- The board schematic uses an AW9364DNR 4-channel parallel LED driver.
- The AW9364DNR supports up to 4 LEDs, up to 20 mA per channel, with 16 current
  steps from 20 mA down to 1.25 mA.

Because the board has a dedicated backlight driver, brightness should probably
be implemented with the AW9364's 1-wire pulse-count dimming protocol instead of
plain LEDC PWM on GPIO 25.

## Battery Runtime Estimates

Runtime gain depends on total board current, not just backlight current:

```text
runtime_gain = old_current / new_current - 1
```

If full brightness is treated as roughly 60 to 80 mA of backlight current:

| Brightness | Backlight Current | Current Saved | Estimated Runtime Gain |
| ---: | ---: | ---: | ---: |
| 75% | 45 to 60 mA | 15 to 20 mA | about 6% to 13% |
| 50% | 30 to 40 mA | 30 to 40 mA | about 13% to 29% |

For this WiFi/BLE scanner firmware, a reasonable working estimate is:

- 75% brightness may add roughly 8% to 10% more battery runtime.
- 50% brightness may add roughly 18% to 22% more battery runtime.

These are estimates. The ESP32-C5 radio current varies a lot by mode: Espressif
lists WiFi RX around 87 to 113 mA, WiFi TX peaks around 238 to 403 mA, and BLE RX
around 79 mA. The scanner's real average current will depend on scan cadence,
2.4 GHz vs 5 GHz activity, BLE activity, CPU frequency, screen redraw frequency,
and battery regulator efficiency.

## Future Implementation Notes

- Add a backlight brightness setting with named levels such as 100%, 75%, 50%,
  and maybe 25%.
- Use the AW9364 1-wire dimming sequence on GPIO 25.
- Keep startup at a readable default, likely 75% or 100%.
- Consider saving the selected brightness once a persistent settings mechanism
  exists.
- Validate on hardware with a USB power meter or inline battery current meter.

## Source Trail

- LilyGO T-Display C5 product page: display is 1.9 inch ST7789, 170 x 320,
  4-wire SPI, 3.3 V.
- LilyGO T-Display C5 upstream repo: `hardware/T-Display C5_20260519.pdf`
  shows `LCD_BLK` on GPIO 25 connected to AW9364DNR `EN`, with LEDK1 through
  LEDK4 on the driver outputs.
- LilyGO T-Display C5 upstream repo: `doc/HD19006C30-V3规格书(1).pdf` gives the
  LCD module backlight current notes.
- Awinic AW9364DNR product page and datasheet: 4-channel 1-wire dimming LED
  driver, 16 current steps, max 20 mA per channel.
- Espressif ESP32-C5-WROOM-1 datasheet: RF active-mode current ranges used for
  whole-device estimate context.

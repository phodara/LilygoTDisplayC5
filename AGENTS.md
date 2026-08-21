# Agent Instructions

Before making code or documentation changes, read `PROJECT_MEMORY.md` for board details, pin mappings, expansion notes, and project history.

## Project Context

- This is a PlatformIO/Arduino project for the LilyGO T-Display C5.
- The current firmware is a portable WiFi analyzer using the built-in ST7789 display and two top buttons.
- Pin definitions are centralized in `include/pin_config.h`.

## Hardware Cautions

- ESP32-C5 GPIO logic is 3.3V.
- Do not feed 5V signals into GPIO pins.
- Avoid reusing pins already assigned to the LCD, buttons, PMU, touch hardware, or I2C bus unless the change intentionally shares that bus.
- Prefer the repo's existing LovyanGFX and PlatformIO patterns when adding features.

## Workflow

- Keep changes scoped and avoid unrelated refactors.
- Do not revert user changes unless explicitly asked.
- Build or test with PlatformIO when code changes affect firmware behavior.

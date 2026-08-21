<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# Display Flicker Fix

## Original Problem

The display flickered because every screen refresh started by clearing the
entire ST7789 panel to black, then redrawing the UI directly to the LCD over
SPI. On this board the redraw was visible: the black clear reached the screen
before the header, rows, detail panel, and RSSI graph were drawn back in.

The worst offenders were the old full-screen clears in the WiFi and Bluetooth
screen renderers:

- `drawAnalyzer()` previously called `tft.fillScreen(TFT_BLACK)` before drawing
  the WiFi analyzer.
- `drawBleScanner()` previously called `tft.fillScreen(TFT_BLACK)` before
  drawing the Bluetooth scanner.

Those calls were removed. In the current source, `drawAnalyzer()` is at
`src/main.cpp:623` and `drawBleScanner()` is at `src/main.cpp:750`; neither
function clears the whole panel anymore.

## Failed Experiment: Full-Screen Sprite

A full-screen `LGFX_Sprite` framebuffer was tried first. The idea was to draw a
complete frame off-screen, then push the finished image to the LCD in one shot.
That build compiled and uploaded, but the physical display stayed completely
black. The sprite path was rolled back immediately.

The working fix avoids a full-screen sprite and keeps direct LCD drawing, but
draws less.

## Current Fix

### 1. Redraw Only The Regions That Need Clearing

The screen is now split into a header and a body.

- `drawHeader()` clears and redraws only the top 28 pixels:
  `src/main.cpp:462`.
- `drawWifiBody()` clears only the WiFi table strip before drawing rows and the
  detail panel: `src/main.cpp:599`.
- `drawBleBody()` clears only the Bluetooth table strip before drawing rows and
  the detail panel: `src/main.cpp:726`.

The detail panels still clear their own bounded lower regions:

- WiFi details clear from `panelY` to the bottom in `drawDetails()`:
  `src/main.cpp:516`.
- Bluetooth details clear from `panelY` to the bottom in `drawBleDetails()`:
  `src/main.cpp:649`.

This removes the original full-panel black flash while still preventing stale
text from remaining in the table and detail areas.

### 2. Add Redraw Entry Points

The code now has three display update levels declared at `src/main.cpp:139`:

- `drawCurrentScreen()` redraws header plus body.
- `drawCurrentHeader()` redraws only the header.
- `drawCurrentBody()` redraws only the body.

Their implementations are at:

- `drawCurrentScreen()`: `src/main.cpp:756`.
- `drawCurrentHeader()`: `src/main.cpp:767`.
- `drawCurrentBody()`: `src/main.cpp:774`.

All three wrap drawing in `tft.startWrite()` and `tft.endWrite()` so each redraw
uses one LovyanGFX write transaction instead of many separate display
transactions.

### 3. Stop Battery Updates From Redrawing The Whole Screen

Battery polling used to trigger a full `drawCurrentScreen()` every battery
interval. That caused visible redraws even when only the battery label changed.

`updateBattery()` now stores the previous battery percent and charging state,
then redraws only the header when either of those visible header values changes:
`src/main.cpp:362`.

The header-only redraw is triggered at `src/main.cpp:387`.

### 4. Use Header-Only Redraws For Scan Status Changes

Starting a scan only changes the header status text from the count to
`Scanning`, so scan start no longer redraws the body.

- WiFi scan start calls `drawCurrentHeader()` at `src/main.cpp:866`.
- Bluetooth scan start calls `drawCurrentHeader()` at `src/main.cpp:885`.

Scan completion still uses a full redraw because counts, selected indexes, rows,
details, and graph data can all change. The WiFi completion redraw is at
`src/main.cpp:849`. The Bluetooth completion redraw is in the BLE scan callback
at `src/main.cpp:315`.

### 5. Use Body-Only Redraws For Detail Toggle

Toggling between RSSI history and MAC/address detail does not change the header.
`toggleDetailView()` now calls `drawCurrentBody()` at `src/main.cpp:947`.

### 6. Remove A Redundant Button-Arming Redraw

The button arming path did a full screen redraw after the startup arm delay, but
it did not change any visible state. That redraw was removed. The button arming
path now logs the button state and returns at `src/main.cpp:1005`.

## Result

The display still redraws directly to the LCD, but much less often and with
smaller cleared regions. The visible black flash from full-screen clears is gone,
and remaining flicker is reduced because status, battery, and detail-only
changes no longer repaint the entire UI.

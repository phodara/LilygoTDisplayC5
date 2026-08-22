<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# Changelog

## Unreleased

### Firmware

- Added a third WiFi lower-button mode: a full AP list where the highlighted AP becomes the selected AP for RSSI history and BSSID detail views.
- Clarified WiFi BSSID labeling, including same-channel `BSSIDs` count, a BSSID suffix on the RSSI history panel, and removal of duplicate-looking detail mode labels.
- Added BSSID values to WiFi scan serial output.
- Changed WiFi scan pacing so the next scan waits 10 seconds after results are processed and redrawn, and failed scans exit the scanning state cleanly before retrying later.
- Fixed BLE scan duration to 3000 ms and updated the Bluetooth header to show a yellow `Scan` activity label while keeping the green `Active`/`Passive` mode label visible.

## v2.0.0

### Firmware

- Added Bluetooth Low Energy scanner mode alongside the WiFi analyzer.
- Added passive and active BLE scan modes.
- Added a Bluetooth-mode control for scan mode:
  - Tap both buttons together to toggle passive/active BLE scanning.
  - Hold both buttons to switch between WiFi and Bluetooth modes.
- Added BLE scan mode text to the Bluetooth detail screen.
- Increased BLE device retention to 60 seconds so intermittent devices do not disappear immediately between advertisements.
- Preserved a previously seen BLE name when later packets from the same device omit the name.
- Added name-based BLE de-duplication for named devices that advertise with changing private addresses.
- Reduced display flicker by removing full-screen clears and redrawing only the header, body, or detail regions that changed.
- Reduced unnecessary display redraws for battery updates, scan status changes, and detail-view toggles.

### Workspace

- Cleared VS Code workspace extension recommendations.
- Added a workspace setting to ignore extension recommendation popups.

## v1.0.0

- Initial LilyGO T-Display C5 WiFi analyzer release.

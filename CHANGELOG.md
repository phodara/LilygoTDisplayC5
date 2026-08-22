<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# Changelog

## Unreleased

### Firmware

- Clarified WiFi channel crowding by renaming the display label from `load` to `BSSIDs`.
- Added a BSSID suffix to the WiFi RSSI history panel header while keeping the full BSSID on the BSSID detail panel.
- Removed duplicate-looking mode labels from detail panels:
  - Removed `MAC` from the right side of the WiFi BSSID detail panel.
  - Removed `ADDR` from the right side of the Bluetooth address detail panel.
- Added BSSID values to WiFi scan serial output.
- Changed WiFi scan pacing so the next scan waits 10 seconds after results are processed and redrawn, preventing an immediate rescan after a long scan.
- Added WiFi scan failure handling so failed scans exit the scanning state cleanly before retrying later.

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

<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# Changelog

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

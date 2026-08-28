<!-- Copyright (c) 2026 Paul Hodara. MIT License. -->

# Button Commands

The LilyGO T-Display C5 has two top buttons used by the firmware:

| Button | GPIO | Firmware name | Main action |
| --- | ---: | --- | --- |
| Upper button | 28 | `upper GPIO28` | Select the next item |
| Lower button | 0 | `lower GPIO0` | Cycle the current screen view |

Buttons are active-low: a pressed button reads `LOW`. After boot, the firmware
waits about 2 seconds before accepting button input so a held boot/programming
button does not immediately trigger app navigation.

## Quick Reference

| Control | WiFi mode | Bluetooth mode |
| --- | --- | --- |
| Tap upper | Select next access point | Select next BLE device |
| Tap lower | Cycle RSSI history, BSSID detail, full AP list | Cycle RSSI history, address detail, manufacturer data, full BLE list |
| Tap both together | No action | Toggle passive/active BLE scan |
| Hold both together | Switch to Bluetooth mode | Switch to WiFi mode |

The selected item wraps to the first row after reaching the end of the list.

## WiFi Mode

WiFi mode is the default startup mode. The header says `Wifi`.

### Select Access Points

Tap the upper button to move the highlight to the next access point.

If the selected access point is near the bottom of the visible table, the list
scrolls to keep the selected row visible. When the selection reaches the last
scanned access point, the next upper-button tap wraps back to the first access
point.

Changing the selected access point clears the RSSI history graph for the previous
selection so the graph can refill for the newly selected BSSID.

### Cycle WiFi Views

Tap the lower button to cycle through the WiFi detail views:

| Step | View | What it shows |
| ---: | --- | --- |
| 1 | RSSI history | Short AP list plus RSSI graph for the selected access point |
| 2 | BSSID detail | Selected SSID, full BSSID/MAC, vendor/OUI hint, detailed security, hidden flag, band, channel, same-channel BSSID count, RSSI, and signal percentage. See `docs/wifi-security-labels.md` and `docs/wifi-vendor-lookup.md` for label meanings. |
| 3 | Full AP list | Full-screen list of scanned access points |

After the full AP list, the next lower-button tap returns to RSSI history.

### Switch To Bluetooth Mode

Hold both buttons together for about 700 ms. The firmware stops the current WiFi
scan state if needed, switches to Bluetooth mode, redraws the screen, and starts
a BLE scan if one is not already running.

A quick both-button tap in WiFi mode is ignored.

## Bluetooth Mode

Bluetooth mode scans nearby BLE advertisements. The header says `BLE` and
shows the BLE scan mode as `Passive` or `Active`.

### Select BLE Devices

Tap the upper button to move the highlight to the next detected BLE device.

The selected device wraps back to the first device after the last detected device.
Changing the selected BLE device clears the BLE RSSI history graph so it can
refill for the new selection.

### Cycle Bluetooth Views

Tap the lower button to cycle through the Bluetooth detail views:

| Step | View | What it shows |
| ---: | --- | --- |
| 1 | RSSI history | Short BLE list plus RSSI graph for the selected device |
| 2 | Address detail | Selected BLE name, address type, full address, RSSI, signal percentage, last-seen age, connectable/beacon status, manufacturer/company hint, and service UUID hint |
| 3 | Manufacturer data | Selected BLE name, manufacturer/company hint, service UUID hint, and manufacturer data as wrapped hexadecimal text |
| 4 | Full BLE list | Full-screen list of detected BLE devices |

After the full BLE list, the next lower-button tap returns to RSSI history.

### Toggle Passive And Active BLE Scanning

Tap both buttons together briefly while in Bluetooth mode to toggle the BLE scan
mode:

- Passive mode listens for normal BLE advertisements.
- Active mode requests scan-response data, which can reveal names for some BLE
  devices.

When the scan mode changes, the firmware stops any scan in progress, redraws the
screen, and starts a new BLE scan using the selected mode.

### Switch Back To WiFi Mode

Hold both buttons together for about 700 ms. The firmware stops any BLE scan in
progress, switches back to WiFi mode, redraws the screen, and starts a WiFi scan
if there are no results yet or if the next scheduled WiFi scan is due.

## Timing Notes

- Button debounce is about 220 ms.
- A both-button hold is detected after about 700 ms.
- Both-button tap is detected when both buttons were pressed together and then
  released before the hold threshold.
- If both buttons are held long enough to switch app modes, the same press will
  not also trigger a single-button action.

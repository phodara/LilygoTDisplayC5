# Bluetooth Scanner Feature Plan

The LilyGO T-Display C5 can also work as a Bluetooth Low Energy scanner. Instead of using BLE as a control channel, this mode listens for nearby BLE advertisements and shows nearby devices with signal strength.

This would make the project feel like a pocket RF field tool:

- Scan nearby Bluetooth devices.
- Show device name, address, RSSI, and advertisement type.
- Sort devices by strongest signal.
- Track one selected Bluetooth device over time.
- Draw an RSSI history graph like the current WiFi analyzer.

## What BLE Scanning Can See

BLE scanning can usually detect:

- Phones, watches, tablets, and laptops.
- Headphones, speakers, and earbuds.
- Smart home sensors.
- ESP32 BLE beacons.
- Fitness trackers and heart-rate sensors.
- Tags and other small beacon devices.

Some devices rotate their Bluetooth addresses for privacy, so a device may not always keep the same address. Device names are also optional, so many scan results will appear unnamed.

## First Version

The first version should mirror the current WiFi analyzer layout.

Displayed fields:

| Field | Meaning |
| --- | --- |
| Name | Advertised BLE device name, or `<unnamed>`. |
| Address | BLE MAC/address shown in detail view. |
| RSSI | Signal strength in dBm. |
| Signal bar | Visual signal strength. |
| Type | Public, random, or unknown address type if available. |
| Seen | Time since the device was last seen. |

Controls:

| Control | Action |
| --- | --- |
| Previous button | Move to the next detected BLE device. |
| Next button | Move to the previous detected BLE device. |
| Both buttons | Toggle between RSSI graph and address/detail view. |

## Suggested Display Modes

The project could eventually have two top-level modes:

| Mode | Purpose |
| --- | --- |
| WiFi Analyzer | Current WiFi scan view. |
| BLE Scanner | Nearby Bluetooth device scan view. |

A long press or both-button hold could switch between modes. For the first experiment, it may be easier to make BLE scanning a separate firmware branch or compile-time option.

## Data Model

BLE devices need a small structure similar to `NetworkInfo`:

```cpp
struct BleDeviceInfo {
  String name;
  String address;
  int32_t rssi = -127;
  uint32_t lastSeenMs = 0;
  bool connectable = false;
};
```

Keep a fixed-size list, sorted by strongest RSSI:

```cpp
static constexpr uint8_t MAX_BLE_DEVICES = 32;
static BleDeviceInfo bleDevices[MAX_BLE_DEVICES];
static int bleDeviceCount = 0;
static int selectedBleDevice = 0;
```

## PlatformIO Dependency

Use NimBLE-Arduino:

```ini
lib_deps =
  lovyan03/LovyanGFX
  h2zero/NimBLE-Arduino
```

NimBLE supports both BLE servers and BLE scanning, so it can serve the control feature and scanner feature later.

## Implementation Outline

Start with passive BLE scanning:

```cpp
#include <NimBLEDevice.h>

NimBLEScan *bleScan = nullptr;

class BleScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *device) override {
    String address = device->getAddress().toString().c_str();
    String name = device->haveName() ? device->getName().c_str() : "";
    int32_t rssi = device->getRSSI();

    updateBleDevice(address, name, rssi, device->isConnectable());
  }
};

void initBleScanner()
{
  NimBLEDevice::init("LilyGO-BLE-Scanner");

  bleScan = NimBLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new BleScanCallbacks());
  bleScan->setActiveScan(false);
  bleScan->setInterval(100);
  bleScan->setWindow(80);
}

void startBleScan()
{
  if (!bleScan) {
    return;
  }

  bleScan->start(3, false);
}
```

`updateBleDevice()` should:

- Find an existing device by address.
- Update its name, RSSI, and `lastSeenMs`.
- Add it if the address has not been seen before.
- Drop the weakest or oldest entry if the list is full.
- Sort the list by RSSI.
- Preserve the selected device if it is still present.

## Scan Timing

BLE scanning and WiFi scanning both use the radio, so they should not run at the same time.

Possible schedules:

| Approach | Behavior |
| --- | --- |
| Separate modes | WiFi mode scans WiFi only, BLE mode scans BLE only. |
| Alternating scans | Scan WiFi, then scan BLE, then repeat. |
| Manual scan | Scan only when the user presses a button or sends a BLE command. |

The simplest and most reliable first version is a separate BLE Scanner mode.

## RSSI Reference

Bluetooth RSSI behaves like WiFi RSSI: closer to zero is stronger.

- `-40 dBm` = very strong, nearby.
- `-60 dBm` = usable nearby signal.
- `-75 dBm` = weak or farther away.
- `-90 dBm` = barely visible.

RSSI jumps around indoors because bodies, walls, and device orientation affect the signal. The graph should be treated as a rough proximity clue, not a precise distance meter.

## Testing

Use known nearby Bluetooth devices:

1. Flash the BLE scanner firmware.
2. Open Serial Monitor.
3. Put a phone, headphones, or ESP32 BLE beacon near the LilyGO.
4. Confirm new devices appear on the display.
5. Move the device closer and farther away.
6. Confirm RSSI changes in the expected direction.
7. Let the scan run for a few minutes and confirm stale entries do not dominate the list.

For deeper testing, run a BLE advertising app on a phone or another ESP32 so the device name is predictable.

## Later Ideas

- Track a selected BLE device and beep or flash when its RSSI gets stronger.
- Add a "last seen" timer for each device.
- Add filters for named devices only.
- Add a known-device list for phones, watches, headphones, or sensors.
- Add BLE beacon mode so the LilyGO can advertise its own presence.
- Add a combined WiFi/BLE dashboard for general nearby-radio awareness.

Keep the first scanner passive and display-focused. Connecting to random BLE devices is not needed for discovery and can make scanning slower or less predictable.

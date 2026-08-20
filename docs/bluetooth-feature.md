# Bluetooth Feature Plan

This device should use Bluetooth Low Energy (BLE) rather than classic Bluetooth Serial. BLE fits the ESP32-C5 well and works cleanly with phone apps, laptops, and small control panels.

The Bluetooth feature can turn the WiFi analyzer into a small local API:

- The LilyGO advertises as `LilyGO-WiFi-Analyzer`.
- A phone or laptop connects over BLE.
- The device exposes readable status characteristics.
- The phone writes short command strings to control the analyzer.
- The device sends notifications when the selected network or scan data changes.

## First Version

The first useful version should mirror the controls already available on the two top buttons.

Supported BLE commands:

| Command | Action |
| --- | --- |
| `NEXT` | Move to the next detected network. |
| `PREV` | Move to the previous detected network. |
| `SCAN` | Start a new WiFi scan. |
| `DETAIL` | Toggle between RSSI history and MAC/detail view. |

The device should notify connected clients with the current selected network status after scans and after selection changes.

Example status payload:

```json
{"ssid":"MyWifi","bssid":"aa:bb:cc:dd:ee:ff","rssi":-61,"channel":6,"battery":84}
```

## Suggested BLE Shape

Use one custom BLE service with two characteristics.

| Item | UUID | Properties | Purpose |
| --- | --- | --- | --- |
| WiFi Analyzer Service | `12345678-1234-1234-1234-1234567890ab` | Service | Groups the analyzer API. |
| Status Characteristic | `12345678-1234-1234-1234-1234567890ac` | Read, Notify | Publishes selected network and battery status. |
| Command Characteristic | `12345678-1234-1234-1234-1234567890ad` | Write | Receives commands from phone/laptop. |

The UUIDs can be changed later, but they should stay stable once a phone app or script depends on them.

## PlatformIO Dependency

Add NimBLE-Arduino alongside LovyanGFX:

```ini
lib_deps =
  lovyan03/LovyanGFX
  h2zero/NimBLE-Arduino
```

NimBLE is preferred because it is lighter than the older ESP32 BLE Arduino API and is a good fit for simple services and characteristics.

## Implementation Outline

Add BLE setup code to `src/main.cpp`:

```cpp
#include <NimBLEDevice.h>

NimBLECharacteristic *statusCharacteristic = nullptr;

class CommandCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *characteristic) override {
    std::string command = characteristic->getValue();

    if (command == "NEXT") {
      adjustSelection(1, "BLE");
    } else if (command == "PREV") {
      adjustSelection(-1, "BLE");
    } else if (command == "SCAN") {
      startScan();
    } else if (command == "DETAIL") {
      macDetailView = !macDetailView;
      drawAnalyzer();
    }
  }
};

void initBluetooth()
{
  NimBLEDevice::init("LilyGO-WiFi-Analyzer");

  NimBLEServer *server = NimBLEDevice::createServer();
  NimBLEService *service = server->createService("12345678-1234-1234-1234-1234567890ab");

  statusCharacteristic = service->createCharacteristic(
    "12345678-1234-1234-1234-1234567890ac",
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  NimBLECharacteristic *commandCharacteristic = service->createCharacteristic(
    "12345678-1234-1234-1234-1234567890ad",
    NIMBLE_PROPERTY::WRITE
  );

  commandCharacteristic->setCallbacks(new CommandCallbacks());

  service->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(service->getUUID());
  advertising->start();
}
```

Call `initBluetooth()` from `setup()` after Serial starts and before the first scan.

Add a helper to publish the current status:

```cpp
void updateBluetoothStatus()
{
  if (!statusCharacteristic || networkCount == 0) {
    return;
  }

  const NetworkInfo &network = networks[selectedNetwork];

  String status = "{";
  status += "\"ssid\":\"" + displaySsid(network) + "\",";
  status += "\"bssid\":\"" + network.bssid + "\",";
  status += "\"rssi\":" + String(network.rssi) + ",";
  status += "\"channel\":" + String(network.channel) + ",";
  status += "\"battery\":" + String(batteryPercent);
  status += "}";

  statusCharacteristic->setValue(status.c_str());
  statusCharacteristic->notify();
}
```

Call `updateBluetoothStatus()` after:

- `collectScanResults()` updates the network list.
- `adjustSelection()` changes the selected network.
- `DETAIL` toggles the detail view, if the phone UI needs to track that mode.

## Testing

Use a BLE scanner app such as nRF Connect or LightBlue.

1. Flash the firmware.
2. Open the BLE scanner app.
3. Connect to `LilyGO-WiFi-Analyzer`.
4. Subscribe to the status characteristic.
5. Write `NEXT`, `PREV`, `SCAN`, or `DETAIL` to the command characteristic.
6. Confirm the display updates and the status notification changes.

Serial Monitor should still be used during early testing so command handling and scan timing are visible.

## Later Ideas

Once the first version works, Bluetooth can grow into a richer control channel:

- `SET_INTERVAL:10000` to change WiFi scan frequency.
- `SET_BRIGHTNESS:80` to adjust the display backlight.
- `LOCK_BSSID:aa:bb:cc:dd:ee:ff` to track a specific access point.
- A phone dashboard that graphs RSSI history.
- A BLE beacon scanner mode for nearby Bluetooth device RSSI.
- Presence detection for known phones, watches, or headphones.

Keep the first protocol simple and text-based. It is easy to test manually, easy to debug over Serial, and flexible enough for a future phone app.

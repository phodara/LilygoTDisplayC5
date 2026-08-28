#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/gpio.h>

#include "axp2602.h"
#include "pin_config.h"

class TDisplayC5 : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel;
  lgfx::Bus_SPI bus;

public:
  TDisplayC5()
  {
    {
      auto cfg = bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = LCD_SCK;
      cfg.pin_mosi = LCD_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = LCD_DC;
      bus.config(cfg);
      panel.setBus(&bus);
    }

    {
      auto cfg = panel.config();
      cfg.pin_cs = LCD_CS;
      cfg.pin_rst = LCD_RST;
      cfg.pin_busy = -1;
      cfg.panel_width = LCD_WIDTH;
      cfg.panel_height = LCD_HEIGHT;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.offset_x = 35;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel.config(cfg);
    }

    setPanel(&panel);
  }
};

struct NetworkInfo {
  String ssid;
  String bssid;
  int32_t rssi = -127;
  int32_t channel = 0;
  wifi_auth_mode_t auth = WIFI_AUTH_OPEN;
  bool hidden = false;
};

struct ButtonState {
  uint8_t pin = 0;
  const char *label = "";
  bool wasPressed = false;
};

struct BleDeviceInfo {
  String name;
  String address;
  String manufacturerDataHex;
  String serviceUuid;
  int32_t rssi = -127;
  uint32_t lastSeenMs = 0;
  uint16_t manufacturerCompanyId = 0;
  uint16_t manufacturerDataBytes = 0;
  uint8_t serviceUuidCount = 0;
  uint8_t addressType = 0;
  bool hasManufacturerCompanyId = false;
  bool connectable = false;
};

struct BleCompanyName {
  uint16_t id;
  const char *name;
};

enum class AppMode {
  Wifi,
  Bluetooth,
};

enum class WifiDetailMode {
  History,
  Bssid,
  List,
};

enum class BleDetailMode {
  History,
  Address,
  MfgData,
  List,
};

TDisplayC5 tft;
AXP2602 pmu(AXP2602_I2C_ADDR_DEFAULT, Wire);
Preferences batteryPrefs;

static constexpr uint8_t MAX_NETWORKS = 24;
static constexpr uint8_t MAX_BLE_DEVICES = 32;
static constexpr uint8_t HISTORY_SAMPLES = 40;
static constexpr uint32_t SCAN_INTERVAL_MS = 10000;
static constexpr uint32_t BLE_SCAN_INTERVAL_MS = 5000;
static constexpr uint32_t BLE_SCAN_DURATION_MS = 3000;
static constexpr uint32_t BLE_STALE_MS = 60000;
static constexpr uint32_t BLE_HISTORY_INTERVAL_MS = 700;
static constexpr uint32_t BOOT_SCREEN_MS = 3000;
static constexpr uint32_t BUTTON_DEBOUNCE_MS = 220;
static constexpr uint32_t MODE_HOLD_MS = 700;
static constexpr uint32_t BATTERY_INTERVAL_MS = 5000;
static constexpr uint32_t BATTERY_STORE_INTERVAL_MS = 300000;
static constexpr uint8_t BATTERY_SAMPLE_COUNT = 5;
static constexpr uint32_t BATTERY_SAMPLE_DELAY_MS = 12;
static constexpr int BATTERY_BOOT_REUSE_TOLERANCE = 4;
static constexpr float BATTERY_CHARGE_ON_CURRENT_A = 0.04f;
static constexpr float BATTERY_CHARGE_OFF_CURRENT_A = 0.005f;
static constexpr float BATTERY_FULL_VOLTAGE = 4.18f;
static constexpr int HEADER_WIFI_SCAN_X = 48;
static constexpr int HEADER_WIFI_BATTERY_X = 98;
static constexpr int HEADER_BLE_SCAN_MODE_X = 160;
static constexpr const char *APP_NAME = "PocketProwler";
static constexpr const char *APP_VERSION = "2.1.0";
static constexpr const char *APP_COPYRIGHT = "Copyright (c) 2026 Paul Hodara";
static constexpr BleCompanyName BLE_COMPANY_NAMES[] = {
  {0x0000, "Ericsson"},
  {0x0001, "Nokia"},
  {0x0002, "Intel"},
  {0x0006, "Microsoft"},
  {0x0008, "Motorola"},
  {0x0009, "Infineon"},
  {0x000A, "Qualcomm"},
  {0x000D, "TI"},
  {0x000F, "Broadcom"},
  {0x001D, "Qualcomm"},
  {0x0025, "NXP"},
  {0x0030, "STMicro"},
  {0x0036, "Renesas"},
  {0x003A, "Panasonic"},
  {0x0046, "MediaTek"},
  {0x004C, "Apple"},
  {0x0055, "Plantronics"},
  {0x0057, "Harman"},
  {0x0059, "Nordic"},
  {0x005A, "EM Micro"},
  {0x005C, "Belkin"},
  {0x005D, "Realtek"},
  {0x0065, "HP"},
  {0x0067, "GN Hearing"},
  {0x0068, "GM"},
  {0x006B, "Polar"},
  {0x0075, "Samsung"},
  {0x0077, "Laird"},
  {0x0078, "Nike"},
  {0x0087, "Garmin"},
  {0x0089, "GN Hearing"},
  {0x0094, "Airoha"},
  {0x009E, "Bose"},
  {0x009F, "Suunto"},
  {0x00B9, "JCI"},
  {0x00C3, "adidas"},
  {0x00C4, "LG"},
  {0x00CC, "Beats"},
  {0x00CD, "Microchip"},
  {0x00D0, "Dexcom"},
  {0x00D1, "Polar"},
  {0x00D2, "Renesas"},
  {0x00D7, "Qualcomm"},
  {0x00D8, "Qualcomm"},
  {0x00E0, "Google"},
  {0x0103, "B&O"},
  {0x0107, "Demant"},
  {0x010E, "Audi"},
  {0x011A, "Qualcomm"},
  {0x011F, "VW"},
  {0x0120, "Porsche"},
  {0x0124, "HID"},
  {0x012D, "Sony"},
  {0x012E, "ASSA"},
  {0x0131, "Cypress"},
  {0x013B, "Allegion"},
  {0x014B, "MasterLock"},
  {0x0154, "Pebble"},
  {0x0155, "Netatmo"},
  {0x0157, "Huami"},
  {0x015D, "Estimote"},
  {0x015E, "Unikey"},
  {0x0164, "Yeelink"},
  {0x0165, "Milwaukee"},
  {0x0170, "Roche"},
  {0x0171, "Amazon"},
  {0x0178, "Casio"},
  {0x017C, "Mercedes"},
  {0x018E, "Google"},
  {0x01AB, "Meta"},
  {0x01D1, "August"},
  {0x01DA, "Logitech"},
  {0x01DD, "Philips"},
  {0x022B, "Tesla"},
  {0x027D, "Huawei"},
  {0x0282, "Sonova"},
  {0x0295, "Sivantos"},
  {0x02A6, "Bosch"},
  {0x02C5, "Lenovo"},
  {0x02DE, "Samsung"},
  {0x02E5, "Espressif"},
  {0x02F2, "GoPro"},
  {0x038F, "Xiaomi"},
  {0x03FF, "Withings"},
  {0x041E, "Dell"},
  {0x0494, "Sennheiser"},
  {0x0499, "Ruuvi"},
  {0x0526, "Honeywell"},
  {0x058E, "Meta"},
  {0x060F, "Signify"},
  {0x067C, "Tile"},
  {0x0723, "Ford"},
  {0x0977, "Toyota"},
  {0x0A2A, "Yamaha"},
  {0x0BA3, "Sonova"},
  {0x0BDE, "Yale"},
  {0x0CC2, "Anker"},
  {0x0EC5, "Alibaba"},
  {0x0FA6, "Ugreen"},
  {0x1040, "RaspberryPi"},
};

static NetworkInfo networks[MAX_NETWORKS];
static BleDeviceInfo bleDevices[MAX_BLE_DEVICES];
static ButtonState upperButton = {BUTTON_BOOT, "upper GPIO28", false};
static ButtonState lowerButton = {BUTTON_PIN, "lower GPIO0", false};
static int32_t rssiHistory[HISTORY_SAMPLES];
static int32_t bleRssiHistory[HISTORY_SAMPLES];
static uint8_t historyCount = 0;
static uint8_t bleHistoryCount = 0;
static String trackedBssid;
static String trackedBleAddress;
static AppMode appMode = AppMode::Wifi;
static int networkCount = 0;
static int bleDeviceCount = 0;
static int selectedNetwork = 0;
static int selectedBleDevice = 0;
static bool scanning = false;
static bool bleScanning = false;
static bool bleActiveScan = false;
static bool buttonsArmed = false;
static WifiDetailMode wifiDetailMode = WifiDetailMode::History;
static BleDetailMode bleDetailMode = BleDetailMode::History;
static bool bothButtonsWasPressed = false;
static bool pmuReady = false;
static uint32_t lastScanMs = 0;
static uint32_t lastBleScanMs = 0;
static uint32_t lastBleHistoryMs = 0;
static uint32_t lastButtonMs = 0;
static uint32_t bothButtonsPressedAtMs = 0;
static uint32_t buttonsArmAtMs = 0;
static uint32_t lastBatteryMs = 0;
static uint32_t lastBatteryStoreMs = 0;
static uint32_t nextWifiScanMs = 0;
static uint32_t scanCount = 0;
static uint32_t bleScanCount = 0;
static float batteryVoltage = 0.0f;
static float batteryCurrent = 0.0f;
static int batteryPercent = -1;
static int lastStoredBatteryPercent = -1;
static int storedBatteryPercent = -1;
static bool batteryCharging = false;
static bool batteryPrefsReady = false;

void drawAnalyzer();
void drawBootScreen();
void drawCurrentScreen();
void drawCurrentHeader();
void drawCurrentBody();
void startWifiScan();
void startBleScan();

NimBLEScan *bleScan = nullptr;

int visibleRowCount()
{
  return min(networkCount, 2);
}

int detailPanelY()
{
  return 34 + (max(1, visibleRowCount()) * 22);
}

bool timeReached(uint32_t deadlineMs)
{
  return static_cast<int32_t>(millis() - deadlineMs) >= 0;
}

void scheduleNextWifiScan()
{
  nextWifiScanMs = millis() + SCAN_INTERVAL_MS;
}

void clearHistory()
{
  historyCount = 0;
  trackedBssid = networkCount > 0 ? networks[selectedNetwork].bssid : "";
}

void appendHistory(int32_t rssi)
{
  if (historyCount < HISTORY_SAMPLES) {
    rssiHistory[historyCount] = rssi;
    historyCount++;
    return;
  }

  for (uint8_t i = 1; i < HISTORY_SAMPLES; i++) {
    rssiHistory[i - 1] = rssiHistory[i];
  }
  rssiHistory[HISTORY_SAMPLES - 1] = rssi;
}

void clearBleHistory()
{
  bleHistoryCount = 0;
  trackedBleAddress = bleDeviceCount > 0 ? bleDevices[selectedBleDevice].address : "";
}

void appendBleHistory(int32_t rssi)
{
  const uint32_t now = millis();
  if (now - lastBleHistoryMs < BLE_HISTORY_INTERVAL_MS) {
    return;
  }

  lastBleHistoryMs = now;
  if (bleHistoryCount < HISTORY_SAMPLES) {
    bleRssiHistory[bleHistoryCount] = rssi;
    bleHistoryCount++;
    return;
  }

  for (uint8_t i = 1; i < HISTORY_SAMPLES; i++) {
    bleRssiHistory[i - 1] = bleRssiHistory[i];
  }
  bleRssiHistory[HISTORY_SAMPLES - 1] = rssi;
}

String displayBleName(const BleDeviceInfo &device)
{
  if (device.name.length() > 0) {
    return device.name;
  }

  if (device.address.length() >= 5) {
    return "BLE " + device.address.substring(device.address.length() - 5);
  }

  return "<unnamed>";
}

const char *bleScanModeLabel()
{
  return bleActiveScan ? "active" : "passive";
}

const char *bleAddressTypeLabel(uint8_t addressType)
{
  switch (addressType) {
    case 0:
      return "public";
    case 1:
      return "random";
    case 2:
      return "pub-id";
    case 3:
      return "rnd-id";
    default:
      return "addr";
  }
}

String compactBleUuid(const String &uuid)
{
  if (uuid.length() <= 8) {
    return uuid;
  }

  return uuid.substring(0, 8);
}

String bytesToHex(const std::string &data)
{
  static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
  String hex;
  hex.reserve(data.length() * 2);
  for (size_t i = 0; i < data.length(); i++) {
    const uint8_t value = static_cast<uint8_t>(data[i]);
    hex += HEX_DIGITS[value >> 4];
    hex += HEX_DIGITS[value & 0x0F];
  }
  return hex;
}

String formatCompanyId(uint16_t companyId)
{
  char label[7];
  snprintf(label, sizeof(label), "0x%04X", companyId);
  return String(label);
}

const char *lookupBleCompanyName(uint16_t companyId)
{
  for (const BleCompanyName &company : BLE_COMPANY_NAMES) {
    if (company.id == companyId) {
      return company.name;
    }
  }

  return nullptr;
}

String bleManufacturerLabel(const BleDeviceInfo &device)
{
  if (device.manufacturerDataBytes == 0) {
    return "--";
  }

  if (!device.hasManufacturerCompanyId) {
    return "data";
  }

  const char *companyName = lookupBleCompanyName(device.manufacturerCompanyId);
  if (companyName) {
    return companyName;
  }

  return formatCompanyId(device.manufacturerCompanyId);
}

String bleAdvertisementSummary(const BleDeviceInfo &device)
{
  String summary = "ADV";
  summary += " MFG ";
  summary += bleManufacturerLabel(device);
  if (device.manufacturerDataBytes > 0) {
    summary += " " + String(device.manufacturerDataBytes) + "B";
  }
  summary += " SVC ";
  if (device.serviceUuidCount > 0) {
    summary += compactBleUuid(device.serviceUuid);
    if (device.serviceUuidCount > 1) {
      summary += "+" + String(device.serviceUuidCount - 1);
    }
  } else {
    summary += "--";
  }
  return summary;
}

void sortBleDevices()
{
  for (int i = 0; i < bleDeviceCount - 1; i++) {
    for (int j = i + 1; j < bleDeviceCount; j++) {
      if (bleDevices[j].rssi > bleDevices[i].rssi) {
        BleDeviceInfo temp = bleDevices[i];
        bleDevices[i] = bleDevices[j];
        bleDevices[j] = temp;
      }
    }
  }
}

void pruneBleDevices()
{
  const uint32_t now = millis();
  for (int i = 0; i < bleDeviceCount;) {
    if (now - bleDevices[i].lastSeenMs > BLE_STALE_MS) {
      for (int j = i + 1; j < bleDeviceCount; j++) {
        bleDevices[j - 1] = bleDevices[j];
      }
      bleDeviceCount--;
      continue;
    }
    i++;
  }

  if (selectedBleDevice >= bleDeviceCount) {
    selectedBleDevice = 0;
  }
}

void updateBleDevice(const String &address,
                     const String &name,
                     int32_t rssi,
                     bool connectable,
                     uint8_t addressType,
                     bool hasManufacturerCompanyId,
                     uint16_t manufacturerCompanyId,
                     uint16_t manufacturerDataBytes,
                     const String &manufacturerDataHex,
                     uint8_t serviceUuidCount,
                     const String &serviceUuid)
{
  const uint32_t now = millis();
  String selectedAddress = bleDeviceCount > 0 ? bleDevices[selectedBleDevice].address : trackedBleAddress;
  int index = -1;
  for (int i = 0; i < bleDeviceCount; i++) {
    if (bleDevices[i].address == address) {
      index = i;
      break;
    }
  }

  if (index < 0 && name.length() > 0) {
    for (int i = 0; i < bleDeviceCount; i++) {
      if (bleDevices[i].name == name) {
        index = i;
        break;
      }
    }
  }

  if (index < 0) {
    if (bleDeviceCount < MAX_BLE_DEVICES) {
      index = bleDeviceCount++;
    } else {
      index = bleDeviceCount - 1;
    }
  }

  bleDevices[index].address = address;
  if (name.length() > 0 || bleDevices[index].name.length() == 0) {
    bleDevices[index].name = name;
  }
  bleDevices[index].rssi = rssi;
  bleDevices[index].lastSeenMs = now;
  bleDevices[index].connectable = connectable;
  bleDevices[index].addressType = addressType;
  bleDevices[index].hasManufacturerCompanyId = hasManufacturerCompanyId;
  bleDevices[index].manufacturerCompanyId = manufacturerCompanyId;
  bleDevices[index].manufacturerDataBytes = manufacturerDataBytes;
  bleDevices[index].manufacturerDataHex = manufacturerDataHex;
  bleDevices[index].serviceUuidCount = serviceUuidCount;
  bleDevices[index].serviceUuid = serviceUuid;

  sortBleDevices();

  if (selectedAddress.length() > 0) {
    for (int i = 0; i < bleDeviceCount; i++) {
      if (bleDevices[i].address == selectedAddress) {
        selectedBleDevice = i;
        break;
      }
    }
  }

  if (bleDeviceCount > 0 && trackedBleAddress.length() == 0) {
    trackedBleAddress = bleDevices[selectedBleDevice].address;
  }

  if (bleDeviceCount > 0 && bleDevices[selectedBleDevice].address == trackedBleAddress) {
    appendBleHistory(bleDevices[selectedBleDevice].rssi);
  }
}

class BleScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *device) override
  {
    String address = device->getAddress().toString().c_str();
    String name = device->haveName() ? device->getName().c_str() : "";
    bool hasManufacturerCompanyId = false;
    uint16_t manufacturerCompanyId = 0;
    uint16_t manufacturerDataBytes = 0;
    String manufacturerDataHex;
    const uint8_t manufacturerDataCount = device->getManufacturerDataCount();
    for (uint8_t i = 0; i < manufacturerDataCount; i++) {
      const std::string manufacturerData = device->getManufacturerData(i);
      if (!hasManufacturerCompanyId && manufacturerData.length() >= 2) {
        manufacturerCompanyId = static_cast<uint8_t>(manufacturerData[0]) |
                                (static_cast<uint16_t>(static_cast<uint8_t>(manufacturerData[1])) << 8);
        hasManufacturerCompanyId = true;
      }
      manufacturerDataBytes += manufacturerData.length();
      manufacturerDataHex += bytesToHex(manufacturerData);
    }

    const uint8_t serviceUuidCount = device->getServiceUUIDCount();
    String serviceUuid = serviceUuidCount > 0 ? device->getServiceUUID(0).toString().c_str() : "";
    updateBleDevice(address,
                    name,
                    device->getRSSI(),
                    device->isConnectable(),
                    device->getAddressType(),
                    hasManufacturerCompanyId,
                    manufacturerCompanyId,
                    manufacturerDataBytes,
                    manufacturerDataHex,
                    serviceUuidCount,
                    serviceUuid);
  }

  void onScanEnd(const NimBLEScanResults &results, int reason) override
  {
    (void)results;
    (void)reason;
    bleScanning = false;
    lastBleScanMs = millis();
    bleScanCount++;
    pruneBleDevices();
    Serial.printf("BLE scan %lu complete: %d devices\n",
                  static_cast<unsigned long>(bleScanCount),
                  bleDeviceCount);
    if (appMode == AppMode::Bluetooth) {
      drawCurrentScreen();
    }
  }
};

static BleScanCallbacks bleCallbacks;

int graphYForRssi(int32_t rssi, int graphTop, int graphHeight)
{
  const int clamped = constrain(rssi, -95, -35);
  return map(clamped, -95, -35, graphTop + graphHeight - 1, graphTop);
}

uint16_t rssiColor(int32_t rssi)
{
  if (rssi >= -55) {
    return TFT_GREEN;
  }
  if (rssi >= -68) {
    return TFT_YELLOW;
  }
  if (rssi >= -78) {
    return TFT_ORANGE;
  }
  return TFT_RED;
}

int signalPercent(int32_t rssi)
{
  if (rssi <= -95) {
    return 0;
  }
  if (rssi >= -35) {
    return 100;
  }
  return map(rssi, -95, -35, 0, 100);
}

String batteryLabel()
{
  if (!pmuReady || batteryPercent < 0) {
    return "BAT --% --.--V";
  }

  return "BAT " + String(batteryPercent) + "% " + String(batteryVoltage, 2) + "V";
}

void readBatterySample(float &voltage, float &current, uint8_t &soc)
{
  uint8_t socSamples[BATTERY_SAMPLE_COUNT];
  float voltageSum = 0.0f;
  float currentSum = 0.0f;

  for (uint8_t i = 0; i < BATTERY_SAMPLE_COUNT; i++) {
    voltageSum += pmu.getBatteryVoltage();
    currentSum += pmu.getBatteryCurrent();
    socSamples[i] = pmu.getSOC();
    if (i + 1 < BATTERY_SAMPLE_COUNT) {
      delay(BATTERY_SAMPLE_DELAY_MS);
    }
  }

  for (uint8_t i = 1; i < BATTERY_SAMPLE_COUNT; i++) {
    const uint8_t value = socSamples[i];
    int j = i - 1;
    while (j >= 0 && socSamples[j] > value) {
      socSamples[j + 1] = socSamples[j];
      j--;
    }
    socSamples[j + 1] = value;
  }

  voltage = voltageSum / BATTERY_SAMPLE_COUNT;
  current = currentSum / BATTERY_SAMPLE_COUNT;
  soc = socSamples[BATTERY_SAMPLE_COUNT / 2];
}

int estimateBatteryPercentFromVoltage(float voltage)
{
  if (voltage >= BATTERY_FULL_VOLTAGE) return 100;
  if (voltage >= 4.10f) return map(static_cast<int>(voltage * 1000), 4100, 4180, 90, 100);
  if (voltage >= 3.98f) return map(static_cast<int>(voltage * 1000), 3980, 4100, 75, 90);
  if (voltage >= 3.85f) return map(static_cast<int>(voltage * 1000), 3850, 3980, 55, 75);
  if (voltage >= 3.75f) return map(static_cast<int>(voltage * 1000), 3750, 3850, 35, 55);
  if (voltage >= 3.65f) return map(static_cast<int>(voltage * 1000), 3650, 3750, 20, 35);
  if (voltage >= 3.50f) return map(static_cast<int>(voltage * 1000), 3500, 3650, 8, 20);
  if (voltage >= 3.30f) return map(static_cast<int>(voltage * 1000), 3300, 3500, 0, 8);
  return 0;
}

int normalizeBatteryPercent(uint8_t rawSoc, float voltage, bool charging)
{
  int percent = constrain(static_cast<int>(rawSoc), 0, 100);

  if (percent >= 99 || (voltage >= BATTERY_FULL_VOLTAGE && percent >= 95)) {
    return 100;
  }

  if (percent == 0 && voltage > 3.50f) {
    percent = estimateBatteryPercentFromVoltage(voltage);
  }

  if (charging && voltage >= 4.15f && percent >= 96) {
    return 100;
  }

  return percent;
}

bool chargingStateFromCurrent(float current)
{
  if (batteryCharging) {
    return current > BATTERY_CHARGE_OFF_CURRENT_A;
  }
  return current > BATTERY_CHARGE_ON_CURRENT_A;
}

int stabilizeBatteryPercent(int candidatePercent, bool charging)
{
  if (candidatePercent == 100) {
    return 100;
  }

  if (batteryPercent < 0) {
    if (storedBatteryPercent >= 0 && abs(candidatePercent - storedBatteryPercent) <= BATTERY_BOOT_REUSE_TOLERANCE) {
      return storedBatteryPercent;
    }
    return candidatePercent;
  }

  if (batteryPercent == 100 && candidatePercent >= 96) {
    return 100;
  }

  const int delta = candidatePercent - batteryPercent;
  if (abs(delta) <= 1) {
    return batteryPercent;
  }

  if (charging && delta < 0 && abs(delta) <= 3) {
    return batteryPercent;
  }

  if (!charging && delta > 0 && delta <= 3) {
    return batteryPercent;
  }

  return candidatePercent;
}

void storeBatteryPercentIfNeeded()
{
  if (!batteryPrefsReady || batteryPercent < 0 || batteryPercent == lastStoredBatteryPercent) {
    return;
  }

  const uint32_t now = millis();
  if (lastStoredBatteryPercent >= 0 &&
      abs(batteryPercent - lastStoredBatteryPercent) < 5 &&
      now - lastBatteryStoreMs < BATTERY_STORE_INTERVAL_MS) {
    return;
  }

  batteryPrefs.putInt("pct", batteryPercent);
  lastStoredBatteryPercent = batteryPercent;
  lastBatteryStoreMs = now;
}

void updateBattery(bool forceRedraw = false)
{
  const uint32_t now = millis();
  if (!forceRedraw && now - lastBatteryMs < BATTERY_INTERVAL_MS) {
    return;
  }

  lastBatteryMs = now;
  if (!pmuReady) {
    return;
  }

  const int previousPercent = batteryPercent;
  const bool previousCharging = batteryCharging;

  uint8_t rawSoc = 0;
  readBatterySample(batteryVoltage, batteryCurrent, rawSoc);
  batteryCharging = chargingStateFromCurrent(batteryCurrent);
  batteryPercent = stabilizeBatteryPercent(normalizeBatteryPercent(rawSoc, batteryVoltage, batteryCharging), batteryCharging);
  storeBatteryPercentIfNeeded();

  if (forceRedraw) {
    return;
  }

  if (batteryPercent != previousPercent || batteryCharging != previousCharging) {
    drawCurrentHeader();
  }
}

void drawChargingBolt(int x, int y, uint16_t color, uint16_t bg)
{
  tft.fillRect(x, y, 9, 16, bg);
  tft.fillTriangle(x + 5, y, x, y + 8, x + 5, y + 8, color);
  tft.fillTriangle(x + 3, y + 7, x + 8, y + 7, x + 3, y + 15, color);
}

void drawBatteryStatus(int x, const String &label)
{
  tft.setTextColor(pmuReady ? TFT_GREEN : TFT_DARKGREY, TFT_NAVY);
  if (batteryCharging) {
    drawChargingBolt(x - 11, 6, TFT_YELLOW, TFT_NAVY);
  }
  tft.drawString(label, x, 6);
}

void drawBatteryStatus(int x)
{
  drawBatteryStatus(x, batteryLabel());
}

const char *bandLabel(int32_t channel)
{
  return channel > 14 ? "5G" : "2.4G";
}

const char *securityLabel(wifi_auth_mode_t auth)
{
  switch (auth) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2-E";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/3";
    default:
      return "SEC";
  }
}

const char *securityDetailLabel(wifi_auth_mode_t auth)
{
  switch (auth) {
    case WIFI_AUTH_WPA_PSK:
      return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2-PSK";
    default:
      return securityLabel(auth);
  }
}

String displaySsid(const NetworkInfo &network)
{
  if (network.ssid.length() == 0) {
    return network.hidden ? "<hidden>" : "<blank>";
  }
  return network.ssid;
}

String fitText(String text, size_t maxChars)
{
  if (text.length() <= maxChars) {
    return text;
  }
  return text.substring(0, maxChars - 1) + "~";
}

String bssidSuffix(const String &bssid)
{
  if (bssid.length() <= 8) {
    return bssid;
  }
  return bssid.substring(bssid.length() - 8);
}

int channelLoad(int32_t channel)
{
  int count = 0;
  for (int i = 0; i < networkCount; i++) {
    if (networks[i].channel == channel) {
      count++;
    }
  }
  return count;
}

void drawSignalBar(int x, int y, int w, int h, int32_t rssi)
{
  const int pct = signalPercent(rssi);
  const int fillW = (w * pct) / 100;
  tft.drawRect(x, y, w, h, TFT_DARKGREY);
  tft.fillRect(x + 1, y + 1, max(0, fillW - 2), h - 2, rssiColor(rssi));
}

void drawHeader()
{
  tft.fillRect(0, 0, tft.width(), 28, TFT_NAVY);
  String status;
  if (appMode == AppMode::Wifi) {
    if (networkCount > 0) {
      status = String(selectedNetwork + 1) + "/" + String(networkCount) + " APs";
    } else {
      status = "0 APs";
    }
  } else {
    if (bleDeviceCount > 0) {
      status = String(selectedBleDevice + 1) + "/" + String(bleDeviceCount) + " BLE";
    } else {
      status = "0 BLE";
    }
  }
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  const char *modeTitle = appMode == AppMode::Wifi ? "Wifi" : "BLE";
  tft.drawString(modeTitle, 8, 6);

  const bool activeScan = appMode == AppMode::Wifi ? scanning : bleScanning;
  if (activeScan) {
    tft.setTextColor(TFT_YELLOW, TFT_NAVY);
    const int scanX = appMode == AppMode::Bluetooth ? (8 + tft.textWidth(modeTitle) + HEADER_BLE_SCAN_MODE_X) / 2 : HEADER_WIFI_SCAN_X;
    tft.drawString("Scan", scanX, 6);
    if (appMode == AppMode::Bluetooth) {
      tft.setTextColor(TFT_GREEN, TFT_NAVY);
      tft.drawString(bleActiveScan ? "Active" : "Passive", HEADER_BLE_SCAN_MODE_X, 6);
    } else {
      drawBatteryStatus(HEADER_WIFI_BATTERY_X);
    }
  } else if (appMode == AppMode::Bluetooth) {
    tft.setTextColor(TFT_GREEN, TFT_NAVY);
    tft.drawString(bleActiveScan ? "Active" : "Passive", HEADER_BLE_SCAN_MODE_X, 6);
  } else {
    drawBatteryStatus(HEADER_WIFI_BATTERY_X);
  }

  tft.setTextColor(TFT_CYAN, TFT_NAVY);
  tft.drawRightString(status, tft.width() - 8, 6);
}

void drawNetworkRow(int row, int networkIndex, int startY = 34, int rowHeight = 22)
{
  const NetworkInfo &network = networks[networkIndex];
  const int y = startY + (row * rowHeight);
  const bool selected = networkIndex == selectedNetwork;
  const uint16_t bg = selected ? TFT_DARKCYAN : TFT_BLACK;

  tft.fillRect(0, y - 2, tft.width(), rowHeight - 1, bg);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString(fitText(displaySsid(network), 15), 8, y);

  drawSignalBar(138, y + 3, 52, 10, network.rssi);

  tft.setTextColor(rssiColor(network.rssi), bg);
  tft.drawRightString(String(network.rssi), 232, y);

  tft.setTextColor(TFT_LIGHTGREY, bg);
  tft.drawRightString(String(network.channel), 266, y);
  tft.drawRightString(bandLabel(network.channel), 312, y);
}

void drawDetails()
{
  const int panelY = detailPanelY();
  tft.fillRect(0, panelY, tft.width(), tft.height() - panelY, TFT_BLACK);
  tft.drawFastHLine(0, panelY, tft.width(), TFT_DARKGREY);

  if (networkCount == 0) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(scanning ? "Scanning for nearby networks..." : "No networks found yet", 8, panelY + 14);
    return;
  }

  const NetworkInfo &network = networks[selectedNetwork];
  const int load = channelLoad(network.channel);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(fitText(displaySsid(network), 21), 8, panelY + 6);

  if (wifiDetailMode == WifiDetailMode::History) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawRightString("BSSID " + bssidSuffix(network.bssid), tft.width() - 8, panelY + 6);
  }

  if (wifiDetailMode == WifiDetailMode::Bssid) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("BSSID", 8, panelY + 24);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(network.bssid, 76, panelY + 24);

    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("SEC " + String(securityDetailLabel(network.auth)) + "  hidden " + String(network.hidden ? "yes" : "no"), 8, panelY + 42);
    tft.drawString(String(bandLabel(network.channel)) + " ch " + String(network.channel) + "  BSSIDs " + String(load), 8, panelY + 60);
    tft.drawString("RSSI " + String(network.rssi) + " dBm  " + String(signalPercent(network.rssi)) + "%", 8, panelY + 78);
    return;
  }

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("RSSI " + String(network.rssi) + " dBm  " + String(signalPercent(network.rssi)) + "%", 8, panelY + 20);
  String details = String(bandLabel(network.channel)) + " ch " + String(network.channel) + "  " + securityLabel(network.auth) +
                   "  BSSIDs " + String(load);
  tft.drawString(details,
                 8,
                 panelY + 36);

  const int graphX = 8;
  const int graphY = panelY + 54;
  const int graphW = tft.width() - 16;
  const int graphH = tft.height() - graphY - 4;
  if (graphH < 18) {
    return;
  }

  tft.drawRect(graphX, graphY, graphW, graphH, TFT_DARKGREY);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("-35", graphX + 3, graphY + 2);
  tft.drawString("-95", graphX + 3, graphY + graphH - 16);

  if (historyCount < 2) {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("history fills each scan", graphX + graphW / 2, graphY + graphH / 2 - 8);
    return;
  }

  const int plotX = graphX + 28;
  const int plotW = graphW - 34;
  const int plotTop = graphY + 3;
  const int plotH = graphH - 6;
  const int start = historyCount > HISTORY_SAMPLES ? historyCount - HISTORY_SAMPLES : 0;
  const int samples = historyCount - start;

  for (int i = 1; i < samples; i++) {
    const int x0 = plotX + ((i - 1) * (plotW - 1)) / max(1, samples - 1);
    const int x1 = plotX + (i * (plotW - 1)) / max(1, samples - 1);
    const int y0 = graphYForRssi(rssiHistory[start + i - 1], plotTop, plotH);
    const int y1 = graphYForRssi(rssiHistory[start + i], plotTop, plotH);
    tft.drawLine(x0, y0, x1, y1, rssiColor(rssiHistory[start + i]));
  }
}

void drawWifiList()
{
  tft.fillRect(0, 28, tft.width(), tft.height() - 28, TFT_BLACK);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("SSID", 8, 29);
  tft.drawString("Signal", 138, 29);
  tft.drawRightString("dBm", 232, 29);
  tft.drawRightString("Ch", 266, 29);
  tft.drawRightString("Band", 312, 29);

  if (networkCount == 0) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(scanning ? "Scanning for nearby networks..." : "No networks found yet", 8, 54);
    return;
  }

  const int rowHeight = 20;
  const int maxRows = max(1, static_cast<int>((tft.height() - 34) / rowHeight));
  const int visibleRows = min(networkCount, maxRows);
  int firstNetwork = 0;
  if (selectedNetwork >= visibleRows) {
    firstNetwork = selectedNetwork - visibleRows + 1;
  }

  for (int i = 0; i < visibleRows; i++) {
    drawNetworkRow(i, firstNetwork + i, 34, rowHeight);
  }
}

void drawWifiBody()
{
  if (wifiDetailMode == WifiDetailMode::List) {
    drawWifiList();
    return;
  }

  tft.fillRect(0, 28, tft.width(), detailPanelY() - 28, TFT_BLACK);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("SSID", 8, 29);
  tft.drawString("Signal", 138, 29);
  tft.drawRightString("dBm", 232, 29);
  tft.drawRightString("Ch", 266, 29);
  tft.drawRightString("Band", 312, 29);

  const int visibleRows = visibleRowCount();
  int firstNetwork = 0;
  if (selectedNetwork >= visibleRows) {
    firstNetwork = selectedNetwork - visibleRows + 1;
  }

  for (int i = 0; i < visibleRows; i++) {
    drawNetworkRow(i, firstNetwork + i);
  }

  drawDetails();
}

void drawAnalyzer()
{
  drawHeader();
  drawWifiBody();
}

void drawBootScreen()
{
  tft.startWrite();
  tft.fillScreen(TFT_BLACK);

  const int w = tft.width();
  const int h = tft.height();
  const uint16_t bands[] = {TFT_NAVY, TFT_BLUE, TFT_DARKCYAN, TFT_GREEN, TFT_YELLOW, TFT_ORANGE, TFT_RED};
  const int bandCount = sizeof(bands) / sizeof(bands[0]);
  const int bandH = max(1, h / bandCount);
  for (int i = 0; i < bandCount; i++) {
    tft.fillRect(0, i * bandH, w, bandH + 1, bands[i]);
  }

  tft.fillRect(18, 22, w - 36, h - 44, TFT_BLACK);
  tft.drawRect(18, 22, w - 36, h - 44, TFT_CYAN);
  tft.drawRect(22, 26, w - 44, h - 52, TFT_DARKGREY);

  const int centerX = w / 2;
  for (int i = 0; i < 5; i++) {
    const int barH = 10 + (i * 8);
    const uint16_t color = i < 2 ? TFT_RED : (i < 4 ? TFT_YELLOW : TFT_GREEN);
    tft.fillRect(36 + (i * 13), 118 - barH, 8, barH, color);
  }
  for (int i = 0; i < 5; i++) {
    const int barH = 10 + (i * 8);
    const uint16_t color = i < 2 ? TFT_RED : (i < 4 ? TFT_YELLOW : TFT_GREEN);
    tft.fillRect(w - 44 - (i * 13), 118 - barH, 8, barH, color);
  }

  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFont(&fonts::Font4);
  tft.drawString(APP_NAME, centerX, 58);

  const int appNameW = tft.textWidth(APP_NAME);
  const int markX = centerX + (appNameW / 2) + 9;
  const int markY = 45;
  tft.drawCircle(markX, markY, 7, TFT_WHITE);
  tft.setFont(&fonts::Font0);
  tft.drawString("R", markX + 1, markY);

  tft.setFont(&fonts::Font2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(String("Version ") + APP_VERSION, centerX, 92);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(APP_COPYRIGHT, centerX, 126);

  tft.setTextDatum(textdatum_t::top_left);
  tft.endWrite();
}

void drawBleRow(int row, int deviceIndex, int startY = 34, int rowHeight = 20)
{
  const BleDeviceInfo &device = bleDevices[deviceIndex];
  const int y = startY + (row * rowHeight);
  const bool selected = deviceIndex == selectedBleDevice;
  const uint16_t bg = selected ? TFT_DARKCYAN : TFT_BLACK;

  tft.fillRect(0, y - 2, tft.width(), rowHeight - 1, bg);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString(fitText(displayBleName(device), 22), 8, y);

  drawSignalBar(184, y + 3, 48, 10, device.rssi);

  tft.setTextColor(rssiColor(device.rssi), bg);
  tft.drawRightString(String(device.rssi), 276, y);

  tft.setTextColor(TFT_LIGHTGREY, bg);
  tft.drawRightString(String((millis() - device.lastSeenMs) / 1000) + "s", 314, y);
}

void drawBleDetails(int panelY)
{
  tft.fillRect(0, panelY, tft.width(), tft.height() - panelY, TFT_BLACK);
  tft.drawFastHLine(0, panelY, tft.width(), TFT_DARKGREY);

  if (bleDeviceCount == 0) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(bleScanning ? "Scanning for BLE devices..." : "No BLE devices found", 8, panelY + 14);
    tft.drawString(String("Scan mode ") + bleScanModeLabel(), 8, panelY + 32);
    return;
  }

  const BleDeviceInfo &device = bleDevices[selectedBleDevice];
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(fitText(displayBleName(device), 21), 8, panelY + 6);

  if (bleDetailMode == BleDetailMode::History) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawRightString(String(selectedBleDevice + 1) + "/" + String(bleDeviceCount), tft.width() - 8, panelY + 6);
  }

  if (bleDetailMode == BleDetailMode::Address) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(String("ADDR ") + bleAddressTypeLabel(device.addressType), 8, panelY + 24);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(device.address, 112, panelY + 24);

    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(String("RSSI ") + String(device.rssi) + " dBm  " + String(signalPercent(device.rssi)) + "%", 8, panelY + 42);
    tft.drawString(String("Seen ") + String((millis() - device.lastSeenMs) / 1000) + "s  " +
                     (device.connectable ? "connectable" : "beacon"),
                   8,
                   panelY + 60);
    tft.drawString(bleAdvertisementSummary(device), 8, panelY + 78);
    return;
  }

  if (bleDetailMode == BleDetailMode::MfgData) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    String mfgTitle = "MFG " + bleManufacturerLabel(device);
    if (device.manufacturerDataBytes > 0) {
      mfgTitle += " " + String(device.manufacturerDataBytes) + "B";
    }
    tft.drawString(fitText(mfgTitle, 23), 8, panelY + 24);
    if (device.serviceUuidCount > 0) {
      tft.drawRightString("SVC " + compactBleUuid(device.serviceUuid), tft.width() - 8, panelY + 24);
    }

    if (device.manufacturerDataHex.length() == 0) {
      tft.drawString("No manufacturer data", 8, panelY + 42);
      return;
    }

    static constexpr int HEX_CHARS_PER_LINE = 32;
    int y = panelY + 42;
    for (int start = 0; start < device.manufacturerDataHex.length() && y < tft.height() - 2; start += HEX_CHARS_PER_LINE) {
      tft.drawString(device.manufacturerDataHex.substring(start, start + HEX_CHARS_PER_LINE), 8, y);
      y += 16;
    }
    return;
  }

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(String("RSSI ") + String(device.rssi) + " dBm  " + String(signalPercent(device.rssi)) + "%", 8, panelY + 20);
  tft.drawString(String("Seen ") + String((millis() - device.lastSeenMs) / 1000) + "s  " +
                   (device.connectable ? "connectable" : "beacon") + "  " +
                   bleScanModeLabel(),
                 8,
                 panelY + 36);

  const int graphX = 8;
  const int graphY = panelY + 54;
  const int graphW = tft.width() - 16;
  const int graphH = tft.height() - graphY - 4;
  if (graphH < 18) {
    return;
  }

  tft.drawRect(graphX, graphY, graphW, graphH, TFT_DARKGREY);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("-35", graphX + 3, graphY + 2);
  tft.drawString("-95", graphX + 3, graphY + graphH - 16);

  if (bleHistoryCount < 2) {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("history fills each scan", graphX + graphW / 2, graphY + graphH / 2 - 8);
    return;
  }

  const int plotX = graphX + 28;
  const int plotW = graphW - 34;
  const int plotTop = graphY + 3;
  const int plotH = graphH - 6;
  for (int i = 1; i < bleHistoryCount; i++) {
    const int x0 = plotX + ((i - 1) * (plotW - 1)) / max(1, bleHistoryCount - 1);
    const int x1 = plotX + (i * (plotW - 1)) / max(1, bleHistoryCount - 1);
    const int y0 = graphYForRssi(bleRssiHistory[i - 1], plotTop, plotH);
    const int y1 = graphYForRssi(bleRssiHistory[i], plotTop, plotH);
    tft.drawLine(x0, y0, x1, y1, rssiColor(bleRssiHistory[i]));
  }
}

void drawBleList()
{
  tft.fillRect(0, 28, tft.width(), tft.height() - 28, TFT_BLACK);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("BLE name", 8, 29);
  tft.drawString("Signal", 184, 29);
  tft.drawRightString("dBm", 276, 29);
  tft.drawRightString("Seen", 314, 29);

  if (bleDeviceCount == 0) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(bleScanning ? "Scanning for BLE devices..." : "No BLE devices found", 8, 54);
    tft.drawString(String("Scan mode ") + bleScanModeLabel(), 8, 72);
    return;
  }

  const int rowHeight = 20;
  const int maxRows = max(1, static_cast<int>((tft.height() - 34) / rowHeight));
  const int visibleRows = min(bleDeviceCount, maxRows);
  int firstDevice = 0;
  if (selectedBleDevice >= visibleRows) {
    firstDevice = selectedBleDevice - visibleRows + 1;
  }

  for (int i = 0; i < visibleRows; i++) {
    drawBleRow(i, firstDevice + i, 34, rowHeight);
  }
}

void drawBleBody()
{
  if (bleDetailMode == BleDetailMode::List) {
    drawBleList();
    return;
  }

  const int maxVisibleRows = bleDetailMode == BleDetailMode::Address || bleDetailMode == BleDetailMode::MfgData ? 2 : 3;
  const int visibleRows = min(bleDeviceCount, maxVisibleRows);
  const int panelY = 36 + (max(1, visibleRows) * 20);
  tft.fillRect(0, 28, tft.width(), panelY - 28, TFT_BLACK);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("BLE name", 8, 29);
  tft.drawString("Signal", 184, 29);
  tft.drawRightString("dBm", 276, 29);
  tft.drawRightString("Seen", 314, 29);

  int firstDevice = 0;
  if (selectedBleDevice >= visibleRows) {
    firstDevice = selectedBleDevice - visibleRows + 1;
  }

  for (int i = 0; i < visibleRows; i++) {
    drawBleRow(i, firstDevice + i);
  }

  drawBleDetails(panelY);
}

void drawBleScanner()
{
  drawHeader();
  drawBleBody();
}

void drawCurrentScreen()
{
  tft.startWrite();
  if (appMode == AppMode::Wifi) {
    drawAnalyzer();
  } else {
    drawBleScanner();
  }
  tft.endWrite();
}

void drawCurrentHeader()
{
  tft.startWrite();
  drawHeader();
  tft.endWrite();
}

void drawCurrentBody()
{
  tft.startWrite();
  if (appMode == AppMode::Wifi) {
    drawWifiBody();
  } else {
    drawBleBody();
  }
  tft.endWrite();
}

void collectScanResults(int found)
{
  networkCount = min(found, static_cast<int>(MAX_NETWORKS));

  for (int i = 0; i < networkCount; i++) {
    networks[i].ssid = WiFi.SSID(i);
    networks[i].bssid = WiFi.BSSIDstr(i);
    networks[i].rssi = WiFi.RSSI(i);
    networks[i].channel = WiFi.channel(i);
    networks[i].auth = WiFi.encryptionType(i);
    networks[i].hidden = networks[i].ssid.length() == 0;
  }

  for (int i = 0; i < networkCount - 1; i++) {
    for (int j = i + 1; j < networkCount; j++) {
      if (networks[j].rssi > networks[i].rssi) {
        NetworkInfo temp = networks[i];
        networks[i] = networks[j];
        networks[j] = temp;
      }
    }
  }

  if (selectedNetwork >= networkCount) {
    selectedNetwork = 0;
  }

  if (networkCount > 0) {
    if (trackedBssid.length() == 0) {
      trackedBssid = networks[selectedNetwork].bssid;
    }

    int trackedIndex = -1;
    for (int i = 0; i < networkCount; i++) {
      if (networks[i].bssid == trackedBssid) {
        trackedIndex = i;
        break;
      }
    }

    if (trackedIndex >= 0) {
      selectedNetwork = trackedIndex;
      appendHistory(networks[trackedIndex].rssi);
    }
  } else {
    clearHistory();
  }

  WiFi.scanDelete();
  scanCount++;
  scanning = false;

  Serial.printf("Scan %lu complete: %d networks\n", static_cast<unsigned long>(scanCount), networkCount);
  for (int i = 0; i < networkCount; i++) {
    Serial.printf("%2d  %4ld dBm  ch %2ld  %-4s  %-8s  %s  %s\n",
                  i + 1,
                  static_cast<long>(networks[i].rssi),
                  static_cast<long>(networks[i].channel),
                  bandLabel(networks[i].channel),
                  securityLabel(networks[i].auth),
                  networks[i].bssid.c_str(),
                  displaySsid(networks[i]).c_str());
  }

  if (appMode == AppMode::Wifi) {
    drawCurrentScreen();
  }

  lastScanMs = millis();
  scheduleNextWifiScan();
}

void finishFailedWifiScan()
{
  WiFi.scanDelete();
  scanning = false;
  Serial.println("WiFi scan failed");
  if (appMode == AppMode::Wifi) {
    drawCurrentScreen();
  }
  lastScanMs = millis();
  scheduleNextWifiScan();
}

void startWifiScan()
{
  if (bleScanning && bleScan) {
    bleScan->stop();
    bleScanning = false;
  }

  scanning = true;
  Serial.println("Starting WiFi scan...");
  WiFi.scanDelete();
  const int result = WiFi.scanNetworks(true, true);
  if (appMode == AppMode::Wifi) {
    drawCurrentHeader();
  }

  if (result >= 0) {
    collectScanResults(result);
  } else if (result == WIFI_SCAN_FAILED) {
    finishFailedWifiScan();
  }
}

void startBleScan()
{
  if (!bleScan || bleScanning || scanning) {
    return;
  }

  bleScanning = true;
  Serial.printf("Starting BLE %s scan...\n", bleScanModeLabel());
  bleScan->clearResults();
  bleScan->setActiveScan(bleActiveScan);
  if (appMode == AppMode::Bluetooth) {
    drawCurrentHeader();
  }
  if (!bleScan->start(BLE_SCAN_DURATION_MS, false, true)) {
    bleScanning = false;
    lastBleScanMs = millis();
    Serial.println("BLE scan failed to start");
    if (appMode == AppMode::Bluetooth) {
      drawCurrentHeader();
    }
  }
}

void initBleScanner()
{
  NimBLEDevice::init("LilyGO-RF-Scanner");
  bleScan = NimBLEDevice::getScan();
  bleScan->setScanCallbacks(&bleCallbacks, false);
  bleScan->setActiveScan(bleActiveScan);
  bleScan->setInterval(100);
  bleScan->setWindow(80);
}

void toggleBleActiveScan(const char *reason)
{
  if (appMode != AppMode::Bluetooth || !bleScan) {
    return;
  }

  bleActiveScan = !bleActiveScan;
  if (bleScanning) {
    bleScan->stop();
    bleScanning = false;
  }

  Serial.printf("BLE scan mode via %s: %s\n", reason, bleScanModeLabel());
  drawCurrentScreen();
  startBleScan();
}

void selectNextWifiNetwork(const char *reason)
{
  if (networkCount == 0) {
    return;
  }

  selectedNetwork++;
  if (selectedNetwork >= networkCount) {
    selectedNetwork = 0;
  }

  clearHistory();
  Serial.printf("Selected network %d/%d via %s\n", selectedNetwork + 1, networkCount, reason);
  drawCurrentScreen();
}

void selectNextBleDevice(const char *reason)
{
  if (bleDeviceCount == 0) {
    return;
  }

  selectedBleDevice++;
  if (selectedBleDevice >= bleDeviceCount) {
    selectedBleDevice = 0;
  }

  clearBleHistory();
  Serial.printf("Selected BLE device %d/%d via %s\n", selectedBleDevice + 1, bleDeviceCount, reason);
  drawCurrentScreen();
}

void toggleDetailView(const char *reason)
{
  if (appMode == AppMode::Wifi) {
    if (wifiDetailMode == WifiDetailMode::History) {
      wifiDetailMode = WifiDetailMode::Bssid;
    } else if (wifiDetailMode == WifiDetailMode::Bssid) {
      wifiDetailMode = WifiDetailMode::List;
    } else {
      wifiDetailMode = WifiDetailMode::History;
    }

    const char *modeLabel = wifiDetailMode == WifiDetailMode::History ? "RSSI history" :
                            wifiDetailMode == WifiDetailMode::Bssid   ? "BSSID/MAC" :
                                                                         "AP list";
    Serial.printf("WiFi detail view via %s: %s\n", reason, modeLabel);
  } else {
    if (bleDetailMode == BleDetailMode::History) {
      bleDetailMode = BleDetailMode::Address;
    } else if (bleDetailMode == BleDetailMode::Address) {
      bleDetailMode = BleDetailMode::MfgData;
    } else if (bleDetailMode == BleDetailMode::MfgData) {
      bleDetailMode = BleDetailMode::List;
    } else {
      bleDetailMode = BleDetailMode::History;
    }

    const char *modeLabel = bleDetailMode == BleDetailMode::History ? "RSSI history" :
                            bleDetailMode == BleDetailMode::Address ? "address" :
                            bleDetailMode == BleDetailMode::MfgData ? "manufacturer data" :
                                                                      "device list";
    Serial.printf("BLE detail view via %s: %s\n", reason, modeLabel);
  }

  drawCurrentBody();
}

void switchMode()
{
  if (appMode == AppMode::Wifi) {
    if (scanning) {
      WiFi.scanDelete();
      scanning = false;
    }
    appMode = AppMode::Bluetooth;
    Serial.println("Mode switched to Bluetooth devices");
    drawCurrentScreen();
    if (!bleScanning) {
      startBleScan();
    }
    return;
  }

  if (bleScanning && bleScan) {
    bleScan->stop();
    bleScanning = false;
  }
  appMode = AppMode::Wifi;
  Serial.println("Mode switched to WiFi analyzer");
  drawCurrentScreen();
  if (!scanning && (networkCount == 0 || timeReached(nextWifiScanMs))) {
    startWifiScan();
  }
}

bool buttonPressed(ButtonState &button)
{
  return digitalRead(button.pin) == LOW;
}

void handleSingleButton(ButtonState &button)
{
  const bool pressed = buttonPressed(button);
  if (pressed && !button.wasPressed) {
    lastButtonMs = millis();
    Serial.printf("%s press detected, raw=%d\n",
                  button.label,
                  digitalRead(button.pin));

    if (&button == &upperButton) {
      if (appMode == AppMode::Wifi) {
        selectNextWifiNetwork(button.label);
      } else {
        selectNextBleDevice(button.label);
      }
    } else {
      toggleDetailView(button.label);
    }
  }
  button.wasPressed = pressed;
}

void handleButtons()
{
  if (!buttonsArmed) {
    if (millis() < buttonsArmAtMs) {
      return;
    }

    upperButton.wasPressed = buttonPressed(upperButton);
    lowerButton.wasPressed = buttonPressed(lowerButton);
    buttonsArmed = true;
    Serial.printf("Buttons armed: %s=%d %s=%d\n",
                  upperButton.label,
                  digitalRead(upperButton.pin),
                  lowerButton.label,
                  digitalRead(lowerButton.pin));
    return;
  }

  const uint32_t now = millis();
  if (now - lastButtonMs < BUTTON_DEBOUNCE_MS) {
    return;
  }

  const bool upperPressed = buttonPressed(upperButton);
  const bool lowerPressed = buttonPressed(lowerButton);
  if (upperPressed && lowerPressed) {
    if (bothButtonsPressedAtMs == 0) {
      bothButtonsPressedAtMs = now;
    }

    if (!bothButtonsWasPressed && now - bothButtonsPressedAtMs >= MODE_HOLD_MS) {
      switchMode();
      bothButtonsWasPressed = true;
      upperButton.wasPressed = true;
      lowerButton.wasPressed = true;
      lastButtonMs = now;
    }
    return;
  }

  if (bothButtonsPressedAtMs != 0 && !bothButtonsWasPressed) {
    if (appMode == AppMode::Bluetooth) {
      toggleBleActiveScan("both buttons");
    }
    bothButtonsPressedAtMs = 0;
    bothButtonsWasPressed = false;
    upperButton.wasPressed = upperPressed;
    lowerButton.wasPressed = lowerPressed;
    lastButtonMs = now;
    return;
  }

  bothButtonsPressedAtMs = 0;
  bothButtonsWasPressed = false;
  handleSingleButton(upperButton);
  handleSingleButton(lowerButton);
}

void configureButtonPin(uint8_t pin)
{
  gpio_num_t gpio = static_cast<gpio_num_t>(pin);
  gpio_reset_pin(gpio);
  gpio_set_direction(gpio, GPIO_MODE_INPUT);
  gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
}

void initButtons()
{
  configureButtonPin(upperButton.pin);
  configureButtonPin(lowerButton.pin);
  delay(20);

  Serial.printf("Button raw at boot: %s=%d %s=%d\n",
                upperButton.label,
                digitalRead(upperButton.pin),
                lowerButton.label,
                digitalRead(lowerButton.pin));

  buttonsArmed = false;
  buttonsArmAtMs = millis() + 2000;
}

void refreshButtons()
{
  configureButtonPin(upperButton.pin);
  configureButtonPin(lowerButton.pin);
}

void initBattery()
{
  pmuReady = pmu.begin();
  if (pmuReady) {
    batteryPrefsReady = batteryPrefs.begin("battery", false);
    if (batteryPrefsReady) {
      storedBatteryPercent = batteryPrefs.getInt("pct", -1);
      lastStoredBatteryPercent = storedBatteryPercent;
    }
    updateBattery(true);
    Serial.printf("AXP2602 battery: %d%% %.2fV %.3fA%s\n",
                  batteryPercent,
                  batteryVoltage,
                  batteryCurrent,
                  batteryCharging ? " charging" : "");
  } else {
    Serial.println("AXP2602 PMU not found");
  }
}

void setup()
{
  pinMode(LCD_BLK_POWER, OUTPUT);
  digitalWrite(LCD_BLK_POWER, HIGH);

  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("LilyGO T-Display C5 WiFi analyzer booting...");

  initButtons();

  Wire.begin(IIC_SDA_PIN, IIC_SCL_PIN);
  initBattery();

  tft.init();
  tft.setRotation(1);
  tft.setTextSize(1);
  tft.setFont(&fonts::Font2);
  tft.setTextDatum(textdatum_t::top_left);
  drawBootScreen();
  delay(BOOT_SCREEN_MS);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  initBleScanner();

  drawCurrentScreen();
  startWifiScan();
}

void loop()
{
  refreshButtons();
  handleButtons();
  updateBattery();

  if (appMode == AppMode::Wifi && scanning) {
    const int result = WiFi.scanComplete();
    if (result >= 0) {
      collectScanResults(result);
    } else if (result == WIFI_SCAN_FAILED) {
      finishFailedWifiScan();
    }
  } else if (appMode == AppMode::Wifi && timeReached(nextWifiScanMs)) {
    startWifiScan();
  }

  if (appMode == AppMode::Bluetooth && !bleScanning && millis() - lastBleScanMs >= BLE_SCAN_INTERVAL_MS) {
    startBleScan();
  }
}

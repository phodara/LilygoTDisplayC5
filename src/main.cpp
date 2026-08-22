#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/gpio.h>

#include <AXP2602.h>
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
  int32_t rssi = -127;
  uint32_t lastSeenMs = 0;
  bool connectable = false;
};

enum class AppMode {
  Wifi,
  Bluetooth,
};

TDisplayC5 tft;
AXP2602 pmu(AXP2602_I2C_ADDR_DEFAULT, Wire);

static constexpr uint8_t MAX_NETWORKS = 24;
static constexpr uint8_t MAX_BLE_DEVICES = 32;
static constexpr uint8_t HISTORY_SAMPLES = 40;
static constexpr uint32_t SCAN_INTERVAL_MS = 7000;
static constexpr uint32_t BLE_SCAN_INTERVAL_MS = 5000;
static constexpr uint32_t BLE_STALE_MS = 60000;
static constexpr uint32_t BLE_HISTORY_INTERVAL_MS = 700;
static constexpr uint32_t BUTTON_DEBOUNCE_MS = 220;
static constexpr uint32_t MODE_HOLD_MS = 700;
static constexpr uint32_t BATTERY_INTERVAL_MS = 5000;

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
static bool macDetailView = false;
static bool bleDetailView = false;
static bool bothButtonsWasPressed = false;
static bool pmuReady = false;
static uint32_t lastScanMs = 0;
static uint32_t lastBleScanMs = 0;
static uint32_t lastBleHistoryMs = 0;
static uint32_t lastButtonMs = 0;
static uint32_t bothButtonsPressedAtMs = 0;
static uint32_t buttonsArmAtMs = 0;
static uint32_t lastBatteryMs = 0;
static uint32_t scanCount = 0;
static uint32_t bleScanCount = 0;
static float batteryVoltage = 0.0f;
static float batteryCurrent = 0.0f;
static int batteryPercent = -1;
static bool batteryCharging = false;

void drawAnalyzer();
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
  return 36 + (max(1, visibleRowCount()) * 22);
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

void updateBleDevice(const String &address, const String &name, int32_t rssi, bool connectable)
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
    updateBleDevice(address, name, device->getRSSI(), device->isConnectable());
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
    return "BAT --";
  }

  return "BAT " + String(batteryPercent) + "%";
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

  batteryVoltage = pmu.getBatteryVoltage();
  batteryCurrent = pmu.getBatteryCurrent();
  batteryPercent = constrain(static_cast<int>(pmu.getSOC()), 0, 100);
  batteryCharging = batteryCurrent > 0.02f;

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
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString(appMode == AppMode::Wifi ? "Wifi" : "Bluetooth", 8, 6);

  const bool activeScan = appMode == AppMode::Wifi ? scanning : bleScanning;
  if (activeScan) {
    tft.setTextColor(TFT_YELLOW, TFT_NAVY);
    tft.drawString("Scanning", 101, 6);
  } else if (appMode == AppMode::Bluetooth) {
    tft.setTextColor(bleActiveScan ? TFT_YELLOW : TFT_GREEN, TFT_NAVY);
    tft.drawString(bleActiveScan ? "Active" : "Passive", 101, 6);
  } else {
    tft.setTextColor(pmuReady ? TFT_GREEN : TFT_DARKGREY, TFT_NAVY);
    if (batteryCharging) {
      drawChargingBolt(101, 6, TFT_YELLOW, TFT_NAVY);
    }
    tft.drawString(batteryLabel(), batteryCharging ? 112 : 101, 6);
  }

  tft.setTextColor(TFT_CYAN, TFT_NAVY);
  String status;
  if (appMode == AppMode::Wifi) {
    status = String(networkCount) + " APs";
    if (networkCount > 0) {
      status += " " + String(selectedNetwork + 1) + "/" + String(networkCount);
    }
  } else {
    status = String(bleDeviceCount) + " BLE";
    if (bleDeviceCount > 0) {
      status += " " + String(selectedBleDevice + 1) + "/" + String(bleDeviceCount);
    }
  }
  tft.drawRightString(status, tft.width() - 8, 6);
}

void drawNetworkRow(int row, int networkIndex)
{
  const NetworkInfo &network = networks[networkIndex];
  const int y = 34 + (row * 22);
  const bool selected = networkIndex == selectedNetwork;
  const uint16_t bg = selected ? TFT_DARKCYAN : TFT_BLACK;

  tft.fillRect(0, y - 2, tft.width(), 21, bg);
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

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  if (macDetailView) {
    tft.drawRightString("MAC", tft.width() - 8, panelY + 6);
  }

  if (macDetailView) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("BSSID", 8, panelY + 24);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(network.bssid, 76, panelY + 24);

    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("SEC " + String(securityLabel(network.auth)) + "  hidden " + String(network.hidden ? "yes" : "no"), 8, panelY + 42);
    tft.drawString(String(bandLabel(network.channel)) + " ch " + String(network.channel) + "  load " + String(load), 8, panelY + 60);
    tft.drawString("RSSI " + String(network.rssi) + " dBm  " + String(signalPercent(network.rssi)) + "%", 8, panelY + 78);
    if (pmuReady) {
      tft.drawRightString(String(batteryVoltage, 2) + "V", tft.width() - 8, panelY + 78);
    }
    return;
  }

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("RSSI " + String(network.rssi) + " dBm  " + String(signalPercent(network.rssi)) + "%", 8, panelY + 20);
  String details = String(bandLabel(network.channel)) + " ch " + String(network.channel) + "  " + securityLabel(network.auth) +
                   "  load " + String(load);
  if (pmuReady) {
    details += "  " + String(batteryVoltage, 2) + "V";
  }
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
    tft.drawLine(x0, y0, x1, y1, TFT_GREEN);
  }
}

void drawWifiBody()
{
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

void drawBleRow(int row, int deviceIndex)
{
  const BleDeviceInfo &device = bleDevices[deviceIndex];
  const int y = 34 + (row * 20);
  const bool selected = deviceIndex == selectedBleDevice;
  const uint16_t bg = selected ? TFT_DARKCYAN : TFT_BLACK;

  tft.fillRect(0, y - 2, tft.width(), 19, bg);
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

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawRightString(bleDetailView ? "ADDR" : String(selectedBleDevice + 1) + "/" + String(bleDeviceCount),
                      tft.width() - 8,
                      panelY + 6);

  if (bleDetailView) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("ADDR", 8, panelY + 24);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(device.address, 66, panelY + 24);

    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(String("RSSI ") + String(device.rssi) + " dBm  " + String(signalPercent(device.rssi)) + "%", 8, panelY + 42);
    tft.drawString(String("Seen ") + String((millis() - device.lastSeenMs) / 1000) + "s  " +
                     (device.connectable ? "connectable" : "beacon"),
                   8,
                   panelY + 60);
    tft.drawString(String("Scan mode ") + bleScanModeLabel(), 8, panelY + 78);
    if (pmuReady) {
      tft.drawRightString(String(batteryVoltage, 2) + "V", tft.width() - 8, panelY + 78);
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
    tft.drawLine(x0, y0, x1, y1, TFT_GREEN);
  }
}

void drawBleBody()
{
  const int visibleRows = min(bleDeviceCount, 3);
  const int panelY = 38 + (max(1, visibleRows) * 20);
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
  lastScanMs = millis();

  Serial.printf("Scan %lu complete: %d networks\n", static_cast<unsigned long>(scanCount), networkCount);
  for (int i = 0; i < networkCount; i++) {
    Serial.printf("%2d  %4ld dBm  ch %2ld  %-4s  %-8s  %s\n",
                  i + 1,
                  static_cast<long>(networks[i].rssi),
                  static_cast<long>(networks[i].channel),
                  bandLabel(networks[i].channel),
                  securityLabel(networks[i].auth),
                  displaySsid(networks[i]).c_str());
  }

  if (appMode == AppMode::Wifi) {
    drawCurrentScreen();
  }
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
  bleScan->start(3, false, true);
  if (appMode == AppMode::Bluetooth) {
    drawCurrentHeader();
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
    if (networkCount == 0) {
      return;
    }
    macDetailView = !macDetailView;
    Serial.printf("WiFi detail view via %s: %s\n", reason, macDetailView ? "BSSID/MAC" : "RSSI history");
  } else {
    if (bleDeviceCount == 0) {
      return;
    }
    bleDetailView = !bleDetailView;
    Serial.printf("BLE detail view via %s: %s\n", reason, bleDetailView ? "address" : "RSSI history");
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
  if (!scanning && (networkCount == 0 || millis() - lastScanMs >= SCAN_INTERVAL_MS)) {
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
    updateBattery(true);
    Serial.printf("AXP2602 battery: %d%% %.2fV\n", batteryPercent, batteryVoltage);
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
    }
  } else if (appMode == AppMode::Wifi && millis() - lastScanMs >= SCAN_INTERVAL_MS) {
    startWifiScan();
  }

  if (appMode == AppMode::Bluetooth && !bleScanning && millis() - lastBleScanMs >= BLE_SCAN_INTERVAL_MS) {
    startBleScan();
  }
}

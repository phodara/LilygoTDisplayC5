#include <Arduino.h>
#include <LovyanGFX.hpp>
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

TDisplayC5 tft;
AXP2602 pmu(AXP2602_I2C_ADDR_DEFAULT, Wire);

static constexpr uint8_t MAX_NETWORKS = 24;
static constexpr uint8_t HISTORY_SAMPLES = 40;
static constexpr uint32_t SCAN_INTERVAL_MS = 7000;
static constexpr uint32_t BUTTON_DEBOUNCE_MS = 220;
static constexpr uint32_t BATTERY_INTERVAL_MS = 5000;

static NetworkInfo networks[MAX_NETWORKS];
static ButtonState prevButton = {BUTTON_PIN, "GPIO0", false};
static ButtonState nextButton = {BUTTON_BOOT, "GPIO28", false};
static int32_t rssiHistory[HISTORY_SAMPLES];
static uint8_t historyCount = 0;
static String trackedBssid;
static int networkCount = 0;
static int selectedNetwork = 0;
static bool scanning = false;
static bool buttonsArmed = false;
static bool pmuReady = false;
static uint32_t lastScanMs = 0;
static uint32_t lastButtonMs = 0;
static uint32_t buttonsArmAtMs = 0;
static uint32_t lastBatteryMs = 0;
static uint32_t scanCount = 0;
static float batteryVoltage = 0.0f;
static int batteryPercent = -1;

void drawAnalyzer();

int visibleRowCount()
{
  return min(networkCount, 2);
}

int detailPanelY()
{
  return 36 + (visibleRowCount() * 22);
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

  batteryVoltage = pmu.getBatteryVoltage();
  batteryPercent = constrain(static_cast<int>(pmu.getSOC()), 0, 100);

  if (forceRedraw) {
    return;
  }

  drawAnalyzer();
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
  tft.drawString("WiFi Analyzer", 8, 6);

  tft.setTextColor(pmuReady ? TFT_GREEN : TFT_DARKGREY, TFT_NAVY);
  tft.drawString(batteryLabel(), 112, 6);

  tft.setTextColor(scanning ? TFT_YELLOW : TFT_CYAN, TFT_NAVY);
  String status = scanning ? "Scanning" : String(networkCount) + " APs";
  if (!scanning && networkCount > 0) {
    status += " " + String(selectedNetwork + 1) + "/" + String(networkCount);
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
  tft.drawRightString(String(selectedNetwork + 1) + "/" + String(networkCount), tft.width() - 8, panelY + 6);

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

void drawAnalyzer()
{
  tft.fillScreen(TFT_BLACK);
  drawHeader();

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

  drawAnalyzer();
}

void startScan()
{
  scanning = true;
  Serial.println("Starting WiFi scan...");
  WiFi.scanDelete();
  const int result = WiFi.scanNetworks(true, true);
  drawAnalyzer();

  if (result >= 0) {
    collectScanResults(result);
  }
}

void adjustSelection(int delta, const char *reason)
{
  if (networkCount == 0) {
    return;
  }

  selectedNetwork += delta;
  if (selectedNetwork < 0) {
    selectedNetwork = networkCount - 1;
  } else if (selectedNetwork >= networkCount) {
    selectedNetwork = 0;
  }

  clearHistory();
  Serial.printf("Selected network %d/%d via %s\n", selectedNetwork + 1, networkCount, reason);
  drawAnalyzer();
}

bool buttonPressed(ButtonState &button)
{
  return digitalRead(button.pin) == LOW;
}

void updateButton(ButtonState &button, int delta)
{
  const bool pressed = buttonPressed(button);
  if (pressed && !button.wasPressed) {
    lastButtonMs = millis();
    Serial.printf("%s press detected, raw=%d\n",
                  button.label,
                  digitalRead(button.pin));
    adjustSelection(delta, button.label);
  }
  button.wasPressed = pressed;
}

void handleButtons()
{
  if (!buttonsArmed) {
    if (millis() < buttonsArmAtMs) {
      return;
    }

    prevButton.wasPressed = buttonPressed(prevButton);
    nextButton.wasPressed = buttonPressed(nextButton);
    buttonsArmed = true;
    Serial.printf("Buttons armed: %s=%d %s=%d\n",
                  prevButton.label,
                  digitalRead(prevButton.pin),
                  nextButton.label,
                  digitalRead(nextButton.pin));
    drawAnalyzer();
    return;
  }

  if (networkCount == 0) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastButtonMs < BUTTON_DEBOUNCE_MS) {
    return;
  }

  updateButton(prevButton, 1);
  updateButton(nextButton, -1);
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
  configureButtonPin(prevButton.pin);
  configureButtonPin(nextButton.pin);
  delay(20);

  Serial.printf("Button raw at boot: %s=%d %s=%d\n",
                prevButton.label,
                digitalRead(prevButton.pin),
                nextButton.label,
                digitalRead(nextButton.pin));

  buttonsArmed = false;
  buttonsArmAtMs = millis() + 2000;
}

void refreshButtons()
{
  configureButtonPin(prevButton.pin);
  configureButtonPin(nextButton.pin);
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

  drawAnalyzer();
  startScan();
}

void loop()
{
  refreshButtons();
  handleButtons();
  updateBattery();

  if (scanning) {
    const int result = WiFi.scanComplete();
    if (result >= 0) {
      collectScanResults(result);
    }
  } else if (millis() - lastScanMs >= SCAN_INTERVAL_MS) {
    startScan();
  }
}

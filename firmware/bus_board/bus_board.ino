#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>

// ================= SECRETS =================
#include "secrets.h"  // SERVER_URL, TARGET_ROUTE

// ================= PINS =================
#define TFT_CS    7
#define TFT_DC    6
#define TFT_RST   14
#define TFT_MOSI  4
#define TFT_SCLK  5
#define TFT_BL    10
#define LED_PIN   3
#define LED_COUNT 2
#define RESET_BTN 9  // Factory reset button GPIO

// ================= COLORS =================
#define BLACK   0x0000
#define WHITE   0xFFFF
#define GREEN   0x07E0
#define RED     0xF800
#define ORANGE  0xFD20

// ================= DISPLAY =================
Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 1, false, 170, 320, 35, 0, 35, 0);
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);

// ================= PREFERENCES =================
Preferences prefs;
String storedSSID;
String storedPASS;

// ================= BLE =================
BLEServer *pServer = nullptr;
BLECharacteristic *pSSIDChar = nullptr;
BLECharacteristic *pPASSChar = nullptr;
bool bleProvisioned = false;

// BLE Service / Characteristic UUIDs
#define SERVICE_UUID      "12345678-1234-1234-1234-1234567890ab"
#define SSID_CHAR_UUID    "abcd1234-1234-1234-1234-1234567890ab"
#define PASS_CHAR_UUID    "abcd5678-1234-1234-1234-1234567890ab"

struct BusRow {
  String route;
  String dest;
  int due;
};

String getDeviceName() {
  uint64_t mac = ESP.getEfuseMac();
  String macStr = String((mac >> 32) & 0xFFFF, HEX) +
                  String((mac >> 16) & 0xFFFF, HEX) +
                  String(mac & 0xFFFF, HEX);
  macStr.toUpperCase();
  return "Bus-Board-" + macStr;
}

// ================= DISPLAY HELPERS =================
void showMessage(const String &line1, const String &line2 = "") {
  gfx->fillScreen(BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(WHITE);
  gfx->setCursor(10, 50);
  gfx->println(line1);
  if (line2.length()) {
    gfx->println(line2);
  }
}

void showWiFiStatus() {
  gfx->fillScreen(BLACK);
  gfx->setTextSize(2);
  if (WiFi.status() == WL_CONNECTED) {
    gfx->setTextColor(GREEN);
    gfx->setCursor(10, 50);
    gfx->println("WiFi CONNECTED");
    gfx->setTextSize(2);
    gfx->setCursor(10, 90);
    gfx->println(WiFi.localIP().toString());
  } else {
    gfx->setTextColor(RED);
    gfx->setCursor(10, 50);
    gfx->println("WiFi NOT CONNECTED");
  }
}

// ================= BLE CALLBACKS =================
class SSIDCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    storedSSID = pChar->getValue().c_str();
    prefs.begin("wifi", false);
    prefs.putString("ssid", storedSSID);
    prefs.end();
    showMessage("SSID RECEIVED", storedSSID);
  }
};

class PASSCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    storedPASS = pChar->getValue().c_str();
    prefs.begin("wifi", false);
    prefs.putString("pass", storedPASS);
    prefs.end();
    showMessage("PASSWORD RECEIVED", storedPASS);
    bleProvisioned = true;
  }
};

// ================= FACTORY RESET =================
void factoryReset() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  storedSSID = "";
  storedPASS = "";
  WiFi.disconnect(true);

  showMessage("Factory Reset", "Credentials cleared");
  delay(2000);
  ESP.restart();
}

// ================= BUS BOARD STATE =================

BusRow previousRows[5];
int previousCount = 0;
time_t lastFetchTime = 0;  // store last successful fetch time

const int SCREEN_WIDTH = 320;
const int BOTTOM_ROW_Y = 155;
const int BOTTOM_ROW_HEIGHT = 15;

// ================= HELPER FUNCTIONS =================
bool fetchHTTP(String &payload) {
  const int maxRetries = 3;
  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      delay(500);
    }

    HTTPClient http;
    http.setTimeout(5000);
    http.begin(SERVER_URL);
    int httpCode = http.GET();
    if (httpCode > 0) {
      payload = http.getString();
      http.end();
      return true;
    }
    http.end();
    delay(300);
  }
  return false;
}

void displayError(const char* msg) {
  gfx->fillScreen(0x0000);
  gfx->setTextSize(2);
  gfx->setTextColor(RED);
  gfx->setCursor(40, 70);
  gfx->println(msg);
}

void drawRow(BusRow row, int y, int xOffset) {
  gfx->setTextSize(2);
  gfx->setTextColor(0x07FF);
  gfx->setCursor(5 + xOffset, y);
  gfx->print(row.route);

  gfx->setTextSize(2);
  gfx->setTextColor(0xFD20);
  gfx->setCursor(60 + xOffset, y);
  String dest = row.dest;
  if (dest.length() > 16) dest = dest.substring(0, 13) + "...";
  gfx->print(dest);

  String dueText = (row.due == 0) ? "Due" : String(row.due) + "m";
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(dueText, 0, 0, &x1, &y1, &w, &h);
  int dueX = SCREEN_WIDTH - 8 - w + xOffset;
  gfx->setTextSize(2);
  gfx->setTextColor(0xFFE0);
  gfx->setCursor(dueX, y);
  gfx->print(dueText);
}

void updateLED(bool hasTarget, int due) {
  uint32_t color;
  if (!hasTarget) color = strip.Color(0, 255, 0);
  else if (due <= 3) color = strip.Color(255, 0, 0);
  else if (due <= 5) color = strip.Color(255, 165, 0);
  else color = strip.Color(0, 255, 0);

  strip.fill(color);
  strip.show();
}

void drawBottomRow(int countdown) {
  gfx->fillRect(0, BOTTOM_ROW_Y, SCREEN_WIDTH, BOTTOM_ROW_HEIGHT, 0x0000);
  gfx->setTextSize(1);
  gfx->setTextColor(ORANGE);

  // Current time (left)
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  gfx->setCursor(5, BOTTOM_ROW_Y);
  gfx->print(buf);

  // Countdown (center)
  String cdText = "Update " + String(countdown) + "s";
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(cdText, 0, 0, &x1, &y1, &w, &h);
  int centerX = (SCREEN_WIDTH - w) / 2;
  gfx->setCursor(centerX, BOTTOM_ROW_Y);
  gfx->print(cdText);

  // Last fetch (right)
  if (lastFetchTime != 0) {
    localtime_r(&lastFetchTime, &timeinfo);
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    gfx->getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    int rightX = SCREEN_WIDTH - 5 - w;
    gfx->setCursor(rightX, BOTTOM_ROW_Y);
    gfx->print(buf);
  }
}

void fetchAndDisplayBuses(bool forceAnimate) {
  String payload;
  if (!fetchHTTP(payload)) {
    displayError("HTTP Error");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    displayError("JSON Error");
    return;
  }

  int count = min(5, (int)doc.size());
  BusRow currentRows[5];
  for (int i = 0; i < count; i++) {
    JsonObject bus = doc[i];
    currentRows[i].route = bus["route"].as<String>();
    currentRows[i].dest  = bus["destination"].as<String>();
    currentRows[i].due   = bus["due_in_minutes"];
  }

  bool hasChanges = forceAnimate;
  if (!forceAnimate && count != previousCount) hasChanges = true;
  for (int i = 0; i < count && !hasChanges; i++) {
    if (currentRows[i].route != previousRows[i].route ||
        currentRows[i].dest  != previousRows[i].dest) {
      hasChanges = true;
    }
  }

  previousCount = count;
  for (int i = 0; i < count; i++) previousRows[i] = currentRows[i];

  gfx->fillRect(0, 0, SCREEN_WIDTH, BOTTOM_ROW_Y, 0x0000);

  bool hasTargetRoute = false;
  int earliestDue = 999;
  int y = 8;
  const int rowHeight = 30;

  for (int i = 0; i < count; i++) {
    if (currentRows[i].route == TARGET_ROUTE) {
      hasTargetRoute = true;
      if (currentRows[i].due < earliestDue) earliestDue = currentRows[i].due;
    }

    if (hasChanges) {
      for (int xOffset = -40; xOffset <= 0; xOffset += 8) {
        gfx->fillRect(0, y - 2, SCREEN_WIDTH, rowHeight, 0x0000);
        drawRow(currentRows[i], y, xOffset);
        delay(12);
      }
    } else {
      drawRow(currentRows[i], y, 0);
    }

    if (i < count - 1) {
      gfx->drawLine(0, y + rowHeight - 4, 320, y + rowHeight - 4, 0x39E7);
    }

    y += rowHeight;
  }

  lastFetchTime = time(nullptr);
  updateLED(hasTargetRoute, earliestDue);
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  // LCD
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  gfx->begin();
  gfx->invertDisplay(true);
  gfx->fillScreen(BLACK);

  // Reset button
  pinMode(RESET_BTN, INPUT_PULLUP);
  if (digitalRead(RESET_BTN) == LOW) {
    factoryReset();
  }

  // Load stored WiFi credentials
  prefs.begin("wifi", true);
  storedSSID = prefs.getString("ssid", "");
  storedPASS = prefs.getString("pass", "");
  prefs.end();

  strip.begin();
  strip.setBrightness(50);
  strip.fill(strip.Color(0, 255, 0));
  strip.show();

  // If credentials exist, connect WiFi and continue
  if (storedSSID.length() && storedPASS.length()) {
    showMessage("Connecting to WiFi", storedSSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(storedSSID.c_str(), storedPASS.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(500);
    }
    showWiFiStatus();

    if (WiFi.status() == WL_CONNECTED) {
      // Configure NTP
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      setenv("TZ", "Europe/London", 1);
      tzset();

      fetchAndDisplayBuses(true); // initial fetch
    }
  } else {
    // No credentials, start BLE provisioning
    showMessage("BLE Provisioning", "Use app to send WiFi");
    BLEDevice::init(getDeviceName().c_str());
    pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    pSSIDChar = pService->createCharacteristic(
      SSID_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE
    );
    pSSIDChar->setCallbacks(new SSIDCallback());

    pPASSChar = pService->createCharacteristic(
      PASS_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE
    );
    pPASSChar->setCallbacks(new PASSCallback());

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
    showMessage("BLE Ready", "Send SSID+Password");
  }
}

// ================= LOOP =================
void loop() {
  // Handle BLE provisioning
  if (bleProvisioned) {
    showMessage("Connecting to WiFi", storedSSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(storedSSID.c_str(), storedPASS.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(500);
    }
    bleProvisioned = false;
    showWiFiStatus();

    if (WiFi.status() == WL_CONNECTED) {
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      setenv("TZ", "Europe/London", 1);
      tzset();

      fetchAndDisplayBuses(true); // initial fetch
    }
  }

  // If WiFi connected, continue bus board loop
  if (WiFi.status() == WL_CONNECTED) {
    for (int i = 15; i > 0; i--) {
      drawBottomRow(i);
      delay(1000);
    }
    fetchAndDisplayBuses(false);
  }
}

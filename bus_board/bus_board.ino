/*
 * ESP32-C6-LCD-1.9 Bus Arrivals Display
 *
 * Behaviour:
 * - Displays up to 5 bus entries
 * - No title / no table header
 * - Refresh every 15 seconds
 * - LED logic based on TARGET_ROUTE:
 *     • Not present        -> GREEN
 *     • Due ≤ 3 minutes    -> RED
 *     • Due ≤ 5 minutes    -> ORANGE
 *     • Due > 5 minutes    -> GREEN
 * - Animates rows only when the list of routes/destinations changes
 */

#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "secrets.h"  // <--- contains WiFi, server URL, TARGET_ROUTE

// ================= PINS =================
#define TFT_CS    7
#define TFT_DC    6
#define TFT_RST   14
#define TFT_MOSI  4
#define TFT_SCLK  5
#define TFT_BL    10

#define LED_PIN   3
#define LED_COUNT 2

// ================= DISPLAY ==============
Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 1, false, 170, 320, 35, 0, 35, 0);

// ================= LED ==================
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);

// ================= CONSTANTS ===========
const int SCREEN_WIDTH = 320;

// ================= STATE =================
struct BusRow {
  String route;
  String dest;
  int due;
};

BusRow previousRows[5];
int previousCount = 0;

// ================= SETUP ================
void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.setBrightness(50);
  strip.fill(strip.Color(0, 255, 0));
  strip.show();

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  gfx->begin();
  gfx->invertDisplay(true);
  gfx->fillScreen(0x0000);

  // Show connecting message
  gfx->setTextSize(2);
  gfx->setTextColor(0xFD20);
  gfx->setCursor(10, 70);
  gfx->println("Connecting WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  gfx->fillScreen(0x0000);
  gfx->setCursor(10, 70);
  gfx->println("WiFi Connected!");
  Serial.println("\nWiFi Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  delay(1000);

  fetchAndDisplayBuses(true); // initial fetch, animate all
}

// ================= LOOP =================
void loop() {
  for (int i = 15; i > 0; i--) {
    gfx->fillRect(200, 155, 120, 15, 0x0000);
    gfx->setTextSize(1);
    gfx->setTextColor(0xFD20);
    gfx->setCursor(200, 155);
    gfx->print("Update ");
    gfx->print(i);
    gfx->print("s");
    delay(1000);
  }

  fetchAndDisplayBuses(false);
}

// ================= HELPER: FETCH WITH RETRY ============
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

// ========== FETCH & DISPLAY =============
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

  // Determine if animation is needed: only check route + dest
  bool hasChanges = forceAnimate;
  if (!forceAnimate && count != previousCount) hasChanges = true;
  for (int i = 0; i < count && !hasChanges; i++) {
    if (currentRows[i].route != previousRows[i].route ||
        currentRows[i].dest  != previousRows[i].dest) {
      hasChanges = true;
    }
  }

  // Update previous state
  previousCount = count;
  for (int i = 0; i < count; i++) previousRows[i] = currentRows[i];

  // Clear screen
  gfx->fillScreen(0x0000);

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

  updateLED(hasTargetRoute, earliestDue);
}

// ========== DRAW SINGLE ROW ============
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

// ================= LED LOGIC ============
void updateLED(bool hasTarget, int due) {
  uint32_t color;
  if (!hasTarget) color = strip.Color(0, 255, 0);
  else if (due <= 3) color = strip.Color(255, 0, 0);
  else if (due <= 5) color = strip.Color(255, 165, 0);
  else color = strip.Color(0, 255, 0);

  strip.fill(color);
  strip.show();
}

// ================= ERROR ================
void displayError(const char* msg) {
  gfx->fillScreen(0x0000);
  gfx->setTextSize(2);
  gfx->setTextColor(0xF800);
  gfx->setCursor(40, 70);
  gfx->println(msg);
}

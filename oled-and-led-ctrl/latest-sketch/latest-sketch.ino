#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_wifi.h"
#include "esp_system.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const char* ssid     = ""; // insert the nsme of the wifi
const char* password = "p"; /// enter the password
const char* api_url  = "https://thqbko83xj.execute-api.us-east-1.amazonaws.com/state/esp32_main";

const int RED_PIN    = 25;
const int YELLOW_PIN = 26;
const int GREEN_PIN  = 27;
const int SDA_PIN    = 21;
const int SCL_PIN    = 22;

const unsigned long POLL_INTERVAL_MS   = 3000;
const unsigned long RECONNECT_EVERY_MS = 10000;

unsigned long lastPoll = 0;
unsigned long lastReconnectTry = 0;
String currentMessage = "Ready";
bool oledOK = false;

WiFiClientSecure secureClient;
HTTPClient http;

const char* resetReasonText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "Power on";
    case ESP_RST_SW:        return "Software";
    case ESP_RST_PANIC:     return "Crash/panic";
    case ESP_RST_INT_WDT:   return "Int watchdog";
    case ESP_RST_TASK_WDT:  return "Task watchdog";
    case ESP_RST_WDT:       return "Watchdog";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_DEEPSLEEP: return "Deep sleep";
    default:                return "Other";
  }
}

void showStatus(const String& l1, const String& l2 = "") {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(l1);
  if (l2.length()) display.println(l2);
  display.display();
}

void updateOLED() {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(currentMessage);
  display.display();
}

// Scan, find the strongest "QB", connect on its exact channel. Retries 3 times.
bool connectWiFi() {
  WiFi.mode(WIFI_STA);

  // Open channels 1-13 (the hotspot moves between channels 12 and 13)
  wifi_country_t country = {"KE", 1, 13, 20, WIFI_COUNTRY_POLICY_MANUAL};
  esp_wifi_set_country(&country);

  for (int attempt = 1; attempt <= 3; attempt++) {
    Serial.printf("Attempt %d: scanning...\n", attempt);
    showStatus("Scanning...", "attempt " + String(attempt));

    int n = WiFi.scanNetworks(false, true);
    int best = -1, bestRssi = -999;
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == ssid && WiFi.RSSI(i) > bestRssi) {
        best = i;
        bestRssi = WiFi.RSSI(i);
      }
    }

    if (best < 0) {
      Serial.printf("'%s' not found in %d networks\n", ssid, n);
      showStatus("QB not found", "retrying...");
      WiFi.scanDelete();
      delay(1000);
      continue;
    }

    int ch = WiFi.channel(best);
    uint8_t bssid[6];
    memcpy(bssid, WiFi.BSSID(best), 6);
    WiFi.scanDelete();

    Serial.printf("Found QB on channel %d (RSSI %d). Connecting...\n", ch, bestRssi);
    showStatus("Found QB ch " + String(ch), "connecting...");

    WiFi.begin(ssid, password, ch, bssid);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Connected, IP: ");
      Serial.println(WiFi.localIP());
      return true;
    }

    Serial.printf("Connect failed, status %d (4 = wrong password/security)\n", WiFi.status());
    showStatus("Connect failed", "status " + String(WiFi.status()));
    WiFi.disconnect();
    delay(500);
  }
  return false;
}

void fetchState() {
  secureClient.setInsecure();
  http.setReuse(true);
  http.setConnectTimeout(5000);
  http.setTimeout(5000);

  if (!http.begin(secureClient, api_url)) return;

  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();

    JsonDocument doc;
    if (!deserializeJson(doc, payload)) {
      const char* text = doc["oled_text"]["S"] | "";
      bool redState    = doc["led_red"]["BOOL"]    | false;
      bool yellowState = doc["led_yellow"]["BOOL"] | false;
      bool greenState  = doc["led_green"]["BOOL"]  | false;

      digitalWrite(RED_PIN,    redState    ? HIGH : LOW);
      digitalWrite(YELLOW_PIN, yellowState ? HIGH : LOW);
      digitalWrite(GREEN_PIN,  greenState  ? HIGH : LOW);

      if (currentMessage != text) {
        currentMessage = text;
        updateOLED();
      }
    }
  } else {
    Serial.printf("HTTP error: %d\n", code);
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  digitalWrite(RED_PIN, LOW);
  digitalWrite(YELLOW_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);

  esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("Reset reason: %s\n", resetReasonText(reason));

  Wire.begin(SDA_PIN, SCL_PIN);
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOK) Serial.println("OLED init failed - check wiring");

  showStatus("Last reset:", resetReasonText(reason));
  delay(1500);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  if (connectWiFi()) {
    showStatus("Connected!", WiFi.localIP().toString());
  } else {
    showStatus("WiFi failed", "Will keep retrying");
  }
  delay(1000);
  updateOLED();
}

void loop() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastReconnectTry >= RECONNECT_EVERY_MS) {
      lastReconnectTry = now;
      if (connectWiFi()) updateOLED();
    }
    return;
  }

  if (now - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = now;
    fetchState();
  }
}
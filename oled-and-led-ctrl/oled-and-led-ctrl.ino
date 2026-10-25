#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_system.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Set to 1 ONLY as a last-resort workaround if the hardware fixes don't help.
// It hides the symptom; it does not fix the weak power.
#define DISABLE_BROWNOUT 0

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const char* ssid     = "QB";
const char* password = "pixar1012";
const char* api_url  = "https://thqbko83xj.execute-api.us-east-1.amazonaws.com/state/esp32_main";

const int RED_PIN    = 25;
const int YELLOW_PIN = 26;
const int GREEN_PIN  = 27;

const unsigned long POLL_INTERVAL_MS   = 3000;
const unsigned long WIFI_TIMEOUT_MS    = 15000;
const unsigned long RECONNECT_EVERY_MS = 10000;

unsigned long lastPoll = 0;
unsigned long lastReconnectTry = 0;
String currentMessage = "Ready";

WiFiClientSecure secureClient;
HTTPClient http;

const char* resetReasonText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "Power on";
    case ESP_RST_SW:       return "Software";
    case ESP_RST_PANIC:    return "Crash/panic";
    case ESP_RST_INT_WDT:  return "Int watchdog";
    case ESP_RST_TASK_WDT: return "Task watchdog";
    case ESP_RST_WDT:      return "Watchdog";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_DEEPSLEEP:return "Deep sleep";
    default:               return "Other";
  }
}

void showStatus(const String& l1, const String& l2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(l1);
  if (l2.length()) display.println(l2);
  display.display();
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(currentMessage);
  display.display();
}

bool connectWiFi() {
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);   // lower peak current
  WiFi.begin(ssid, password);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);   // re-apply after begin()

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

void fetchState() {
  secureClient.setInsecure();           // fine for a hobby project; see note below
  http.setReuse(true);                  // keep the TLS connection alive
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
#if DISABLE_BROWNOUT
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif

  Serial.begin(115200);
  delay(500);                            // let the supply settle after boot

  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  digitalWrite(RED_PIN, LOW);
  digitalWrite(YELLOW_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);

  esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("Reset reason: %s\n", resetReasonText(reason));

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");   // continue without the display
  }
  showStatus("Last reset:", resetReasonText(reason));
  delay(1500);

  showStatus("Connecting to:", ssid);
  if (connectWiFi()) {
    showStatus("Connected!", WiFi.localIP().toString());
  } else {
    showStatus("WiFi failed", "Will keep retrying");
  }
  delay(1000);
  updateOLED();

  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);                // don't wear flash with credentials
}

void loop() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastReconnectTry >= RECONNECT_EVERY_MS) {
      lastReconnectTry = now;
      showStatus("WiFi lost", "Reconnecting...");
      if (connectWiFi()) updateOLED();
    }
    return;
  }

  if (now - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = now;
    fetchState();
  }
}
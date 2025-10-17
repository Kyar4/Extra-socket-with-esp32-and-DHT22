#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "DHT.h"
#include <math.h>
#include "time.h"   // dùng NTP để hiển thị giờ, ngày

// ===== Wi-Fi (nhiều cấu hình, dò tuần tự) =====
struct WifiCred { const char* ssid; const char* pass; };
static const WifiCred WIFI_LIST[] = {
  {"NekryuHost",    "Nvt@3112004"},
  {"Xiaomi 14T pro","23111124"}
};
static const size_t WIFI_COUNT = sizeof(WIFI_LIST) / sizeof(WIFI_LIST[0]);

// ===== Firebase RTDB (Email/Password) =====
#define API_KEY      "AIzaSyBuifQfSmxRpzXvtxJSTb_Nav46PulUJPo"
#define DATABASE_URL "https://iot-esp32-61821-default-rtdb.asia-southeast1.firebasedatabase.app"

// ===== Firebase account =====
#define USER_EMAIL    "42200368@student.tdtu.edu.vn"
#define USER_PASSWORD "Nvt@3112004"

// ===== DHT22 =====
#define DHTPIN   19
#define DHTTYPE  DHT22
DHT dht(DHTPIN, DHTTYPE);

// ===== Relay pins =====
#define PIN_RL1 14
#define PIN_RL2 12
#define PIN_RL3 13
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

// ===== TFT ST7735 (SPI: CS=5, DC=21, RST=22, MOSI=23, SCK=18) =====
#define TFT_CS   5
#define TFT_DC   21
#define TFT_RST  22
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ===== Firebase objects =====
FirebaseData fbdo;
FirebaseData fbStream;
FirebaseAuth auth;
FirebaseConfig config;

// ===== App state =====
unsigned long lastSend = 0;
const unsigned long SEND_EVERY_MS = 800;
unsigned long lastClock = 0;

float lastT = NAN, lastH = NAN;
bool RL1 = false, RL2 = false, RL3 = false;

// ================== Wi-Fi helpers ==================
bool tryConnectOne(const WifiCred& c, uint32_t timeout_ms) {
  Serial.printf("\n[WiFi] Thử kết nối SSID: %s\n", c.ssid);
  WiFi.begin(c.ssid, c.pass);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeout_ms) {
    delay(250);
    Serial.print('.');
  }
  bool ok = WiFi.status() == WL_CONNECTED;
  Serial.println(ok ? "\n[WiFi] ĐÃ KẾT NỐI" : "\n[WiFi] THẤT BẠI");
  return ok;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);

  // làm sạch trạng thái trước khi bắt đầu dò
  WiFi.disconnect(true, true);
  delay(200);

  // vòng lặp đến khi kết nối thành công
  while (WiFi.status() != WL_CONNECTED) {
    for (size_t passIdx = 0; passIdx < 2 && WiFi.status() != WL_CONNECTED; ++passIdx) { // thử 2 vòng để chắc
      for (size_t i = 0; i < WIFI_COUNT && WiFi.status() != WL_CONNECTED; ++i) {
        // mỗi mạng cho tối đa ~12 giây (48 * 250 ms)
        if (tryConnectOne(WIFI_LIST[i], 12000)) break;
      }
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Không mạng nào khả dụng, chờ 3s và dò lại...");
      delay(3000);
    }
  }

  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());
}

// ================== Relay ==================
void setRelay(int pin, bool on) { digitalWrite(pin, on ? RELAY_ON : RELAY_OFF); }

// ================== TFT layout ==================
const int WIDTH = 160;
const int MARGIN_X = 6;
const int LINE_H = 20;
const int LINE_TEMP_Y  =  6;
const int LINE_HUM_Y   = 28;
const int LINE_RL1_Y   = 50;
const int LINE_RL2_Y   = 72;
const int LINE_RL3_Y   = 94;
const int VALUE_TH_X  = 70;
const int VALUE_COL_X  = 50;
const int RIGHT_COL_X  = 94;

void drawLabelAt(int y, const char* label) {
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(MARGIN_X, y);
  tft.print(label);
}

void drawValueTextAt(int y, const char* text, uint16_t color) {
  tft.fillRect(VALUE_COL_X, y, WIDTH - VALUE_COL_X, LINE_H, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(color, ST77XX_BLACK);
  tft.setCursor(VALUE_COL_X, y);
  tft.print(text);
}
void drawValueTextAt2(int y, const char* text, uint16_t color) {
  tft.fillRect(VALUE_TH_X , y, WIDTH - VALUE_TH_X , LINE_H, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(color, ST77XX_BLACK);
  tft.setCursor(VALUE_TH_X , y);
  tft.print(text);
}

void drawValueNumAt(int y, float v, const char* unit) {
  tft.fillRect(VALUE_TH_X , y, WIDTH - VALUE_TH_X , LINE_H, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(VALUE_TH_X, y);
  tft.print(v, 1);
  if (unit && unit[0]) tft.print(unit);
}

void drawStaticLayout() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setRotation(1);
  drawLabelAt(LINE_TEMP_Y, "Temp");
  drawLabelAt(LINE_HUM_Y,  "Humid");
  drawLabelAt(LINE_RL1_Y,  "RL1");
  drawLabelAt(LINE_RL2_Y,  "RL2");
  drawLabelAt(LINE_RL3_Y,  "RL3");

  drawValueTextAt2(LINE_TEMP_Y, "--.-C", ST77XX_YELLOW);
  drawValueTextAt2(LINE_HUM_Y,  "--.-%", ST77XX_YELLOW);
  drawValueTextAt(LINE_RL1_Y,  "OFF",  ST77XX_RED);
  drawValueTextAt(LINE_RL2_Y,  "OFF",  ST77XX_RED);
  drawValueTextAt(LINE_RL3_Y,  "OFF",  ST77XX_RED);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(RIGHT_COL_X, LINE_RL2_Y);
  tft.print("--:--");

  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(RIGHT_COL_X, LINE_RL3_Y);
  tft.print("DD/MM");
}

void updateRelayLines() {
  drawValueTextAt(LINE_RL1_Y, RL1 ? "ON" : "OFF", RL1 ? ST77XX_GREEN : ST77XX_RED);
  drawValueTextAt(LINE_RL2_Y, RL2 ? "ON" : "OFF", RL2 ? ST77XX_GREEN : ST77XX_RED);
  drawValueTextAt(LINE_RL3_Y, RL3 ? "ON" : "OFF", RL3 ? ST77XX_GREEN : ST77XX_RED);
}

// ================== Firebase stream ==================
void streamCallback(FirebaseStream data) {
  String sub = data.dataPath();
  if (data.dataTypeEnum() != fb_esp_rtdb_data_type_string) return;
  String state = data.stringData(); state.toUpperCase();
  bool on = (state == "ON");

  if (sub == "/RL1") { RL1 = on; setRelay(PIN_RL1, RL1); }
  else if (sub == "/RL2") { RL2 = on; setRelay(PIN_RL2, RL2); }
  else if (sub == "/RL3") { RL3 = on; setRelay(PIN_RL3, RL3); }

  updateRelayLines();
  String path = String("/ESP32/RelayState") + sub;
  Firebase.RTDB.setString(&fbdo, path.c_str(), on ? "ON" : "OFF");
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) Firebase.RTDB.readStream(&fbStream);
}

// ================== Firebase setup ==================
void setupFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);
  while (auth.token.uid.length() == 0) delay(300);
  Firebase.RTDB.beginStream(&fbStream, "/ESP32/Relay");
  Firebase.RTDB.setStreamCallback(&fbStream, streamCallback, streamTimeoutCallback);
}

// ================== Time (NTP) ==================
void setupTime() {
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // GMT+7 VN
}

void drawTimeAtRL2() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  if (!t) return;
  char hhmm[6];
  strftime(hhmm, sizeof(hhmm), "%H:%M", t);

  tft.fillRect(RIGHT_COL_X, LINE_RL2_Y, WIDTH - RIGHT_COL_X, LINE_H, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(RIGHT_COL_X, LINE_RL2_Y);
  tft.print(hhmm);
}

void drawDateAtRL3() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  if (!t) return;
  char ddmm[12];
  strftime(ddmm, sizeof(ddmm), "%d/%m", t);

  tft.fillRect(RIGHT_COL_X, LINE_RL3_Y, WIDTH - RIGHT_COL_X, LINE_H, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(RIGHT_COL_X, LINE_RL3_Y);
  tft.print(ddmm);
}

// ================== Setup ==================
void setup() {
  Serial.begin(115200);
  connectWiFi();

  SPI.begin(18, -1, 23);
  tft.initR(INITR_BLACKTAB);
  tft.setSPISpeed(27000000);
  tft.setRotation(1);
  drawStaticLayout();

  pinMode(PIN_RL1, OUTPUT);
  pinMode(PIN_RL2, OUTPUT);
  pinMode(PIN_RL3, OUTPUT);
  setRelay(PIN_RL1, false);
  setRelay(PIN_RL2, false);
  setRelay(PIN_RL3, false);
  dht.begin();

  setupFirebase();
  setupTime();
}

// ================== Loop ==================
void loop() {
  // Nếu mất Wi-Fi, tự chạy lại quy trình dò
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Mất kết nối, đổi mạng và kết nối lại...");
    connectWiFi();
  }

  if (Firebase.ready() && millis() - lastSend >= SEND_EVERY_MS) {
    lastSend = millis();
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      Firebase.RTDB.setFloat(&fbdo, "/ESP32/DHT22/Temp", t);
      Firebase.RTDB.setFloat(&fbdo, "/ESP32/DHT22/Humid", h);
      if (isnan(lastT) || fabsf(t - lastT) >= 0.1f) { drawValueNumAt(LINE_TEMP_Y, t, "C"); lastT = t; }
      if (isnan(lastH) || fabsf(h - lastH) >= 0.5f) { drawValueNumAt(LINE_HUM_Y, h, "%"); lastH = h; }
    }
  }

  if (millis() - lastClock > 1000) {
    lastClock = millis();
    drawTimeAtRL2();
    drawDateAtRL3();
  }
}

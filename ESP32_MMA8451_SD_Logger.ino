/*
  ESP32 + MMA8451 Accelerometer + SD (SPI) CSV Logger with Web UI (served from LittleFS)

  CSV columns: timedelta_ms, Xacc, Yacc, Zacc
  Units: m/s^2 (from Adafruit Unified Sensor event.acceleration.*)

  Web UI files are stored in /data and uploaded to LittleFS (not embedded in code).

  Dependencies (Arduino Library Manager):
  - Adafruit MMA8451 Library
  - Adafruit Unified Sensor
  - ESP Async WebServer
  - AsyncTCP
  - ArduinoJson (6.x)

  Notes:
  - MMA8451 is an accelerometer (no gyro).
  - SD card is accessed via SPI (VSPI default pins on ESP32 unless changed).
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ESPmDNS.h>

#include <Adafruit_MMA8451.h>
#include <Adafruit_Sensor.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// ------------------------- User config -------------------------
#include "config.h"
#include "secrets.h"
// --------------------------------------------------------------

Adafruit_MMA8451 mma = Adafruit_MMA8451();
AsyncWebServer server(80);

// Recording state
static bool g_recording = false;
static File g_logFile;
static String g_currentFile = "";
static uint32_t g_recordStartMs = 0;
static uint32_t g_lastSampleMs = 0;
static uint32_t g_lastFlushMs = 0;
static uint32_t g_samples = 0;
static uint32_t g_intervalMs = SAMPLE_INTERVAL_MS_DEFAULT;

// Protect file writes (Async callbacks + loop)
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

// ------------------------- Helpers -------------------------
static String sanitizeFilename(const String &in) {
  // Keep only [A-Za-z0-9._-], replace spaces with underscore, strip leading slashes
  String out;
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == ' ') c = '_';
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '.' || c == '_' || c == '-') {
      out += c;
    }
  }
  while (out.startsWith("/")) out.remove(0, 1);
  if (out.length() == 0) out = "log.csv";
  if (!out.endsWith(".csv")) out += ".csv";
  return out;
}

static String pickUniqueFilename(const String &sanitized) {
  // If file exists, append _001, _002, ...
  String base = sanitized;
  String name = base;
  if (!SD.exists(("/" + name).c_str())) return name;

  int dot = base.lastIndexOf('.');
  String stem = (dot > 0) ? base.substring(0, dot) : base;
  String ext  = (dot > 0) ? base.substring(dot) : ".csv";

  for (int i = 1; i <= 999; i++) {
    char buf[8];
    snprintf(buf, sizeof(buf), "_%03d", i);
    name = stem + buf + ext;
    if (!SD.exists(("/" + name).c_str())) return name;
  }
  return "log_overflow.csv";
}

static void jsonStatus(AsyncWebServerRequest *request) {
  StaticJsonDocument<384> doc;
  doc["recording"] = g_recording;
  doc["file"] = g_currentFile;
  doc["samples"] = (uint32_t)g_samples;
  doc["interval_ms"] = (uint32_t)g_intervalMs;
  doc["uptime_ms"] = (uint32_t)millis();
  doc["ip"] = WiFi.isConnected() ? WiFi.localIP().toString() : (WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : "");
  doc["mode"] = WiFi.isConnected() ? "STA" : (WiFi.getMode() == WIFI_AP ? "AP" : "NONE");

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

static void listSdFiles(AsyncWebServerRequest *request) {
  StaticJsonDocument<4096> doc;
  JsonArray arr = doc.to<JsonArray>();

  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    request->send(500, "application/json", "[]");
    return;
  }

  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      JsonObject o = arr.createNestedObject();
      o["name"] = String(f.name());
      o["size"] = (uint32_t)f.size();
    }
    f = root.openNextFile();
  }

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

static bool startRecordingInternal(const String &requestedName, uint32_t intervalMs, String &outErr) {
  if (g_recording) {
    outErr = "Already recording";
    return false;
  }
  if (!SD.cardType()) {
    outErr = "SD not mounted";
    return false;
  }

  if (intervalMs < 5) intervalMs = 5;        // 200 Hz max by default
  if (intervalMs > 5000) intervalMs = 5000;  // 0.2 Hz min

  String sanitized = sanitizeFilename(requestedName);
  String unique = pickUniqueFilename(sanitized);
  String path = "/" + unique;
  
  g_logFile = SD.open(path.c_str(), FILE_WRITE);
  
  if (!g_logFile) {
    outErr = "Failed to open file on SD";
    return false;
  }

  g_currentFile = unique;
  g_intervalMs = intervalMs;
  g_recordStartMs = millis();
  g_lastSampleMs = g_recordStartMs;
  g_lastFlushMs = g_recordStartMs;
  g_samples = 0;

  // CSV header
  g_logFile.println("timedelta_ms,Xacc,Yacc,Zacc");
  g_logFile.flush();
  g_recording = true;
  return true;
}

static void stopRecordingInternal() {
  if (!g_recording) return;

  if (g_logFile) {
    g_logFile.flush();
    g_logFile.close();
  }

  g_recording = false;
  g_currentFile = "";
}

static void handleStart(AsyncWebServerRequest *request) {
  String file = request->hasParam("file") ? request->getParam("file")->value() : "log.csv";
  uint32_t interval = g_intervalMs;
  if (request->hasParam("interval_ms")) {
    interval = (uint32_t)request->getParam("interval_ms")->value().toInt();
  }

  String err;
  bool ok = startRecordingInternal(file, interval, err);
  StaticJsonDocument<256> doc;
  doc["ok"] = ok;
  doc["error"] = err;
  doc["file"] = g_currentFile;
  doc["interval_ms"] = (uint32_t)g_intervalMs;

  AsyncResponseStream *resp = request->beginResponseStream("application/json");
  serializeJson(doc, *resp);
  request->send(resp);
}

static void handleStop(AsyncWebServerRequest *request) {
  stopRecordingInternal();
  request->send(200, "application/json", "{\"ok\":true}");
}

static void handleDownload(AsyncWebServerRequest *request) {
  if (!request->hasParam("file")) {
    request->send(400, "text/plain", "Missing ?file=");
    return;
  }
  String file = request->getParam("file")->value();
  file = sanitizeFilename(file);
  String path = "/" + file;

  if (!SD.exists(path.c_str())) {
    request->send(404, "text/plain", "File not found");
    return;
  }

  // Serve as attachment
  request->send(SD, path.c_str(), "text/csv", true);
}

static void handleDelete(AsyncWebServerRequest *request) {
  if (!request->hasParam("file")) {
    request->send(400, "application/json", "{\"ok\":false,\"error\":\"Missing ?file=\"}");
    return;
  }
  if (g_recording) {
    request->send(409, "application/json", "{\"ok\":false,\"error\":\"Stop recording first\"}");
    return;
  }
  String file = sanitizeFilename(request->getParam("file")->value());
  String path = "/" + file;
  bool ok = SD.remove(path.c_str());

  StaticJsonDocument<256> doc;
  doc["ok"] = ok;
  doc["file"] = file;
  doc["error"] = ok ? "" : "Remove failed";
  AsyncResponseStream *resp = request->beginResponseStream("application/json");
  serializeJson(doc, *resp);
  request->send(resp);
}

static void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  if (strlen(WIFI_SSID) > 0) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < WIFI_CONNECT_TIMEOUT_MS) {
      delay(200);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi STA connected: %s\n", WiFi.localIP().toString().c_str());
    if (strlen(MDNS_NAME) > 0) {
      if (MDNS.begin(MDNS_NAME)) {
        Serial.printf("mDNS started: http://%s.local/\n", MDNS_NAME);
      }
    }
    return;
  }

  // Fallback to Access Point
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("WiFi AP %s: %s (ip=%s)\n", ok ? "started" : "failed",
                AP_SSID, WiFi.softAPIP().toString().c_str());
}

static bool initAccel() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  delay(10);

  if (!mma.begin(0x1C)) {
    Serial.println("ERROR: MMA8451 not detected on I2C. Check wiring/address.");
    // return false;
  }

  // Range options: MMA8451_RANGE_2_G, _4_G, _8_G
  mma.setRange(MMA_RANGE);

  // Data rate: library supports these values (Hz): 800, 400, 200, 100, 50, 12.5, 6.25, 1.56
  mma.setDataRate(MMA_DATARATE);

  Serial.println("MMA8451 initialized.");
  return true;
}

static bool initSd() {
  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("ERROR: SD.begin() failed. Check wiring/CS/power.");
    return false;
  }
  uint8_t type = SD.cardType();
  if (type == CARD_NONE) {
    Serial.println("ERROR: No SD card attached.");
    return false;
  }
  Serial.printf("SD mounted. Card type=%u, size=%.2fGB\n", type, SD.cardSize() / 1024.0 / 1024.0 / 1024.0);
  return true;
}

static bool initLittleFs() {
  if (!LittleFS.begin(true)) {
    Serial.println("ERROR: LittleFS mount failed.");
    return false;
  }
  Serial.println("LittleFS mounted.");
  return true;
}

static void initWebServer() {
  // Serve UI (static)
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // API routes
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) { jsonStatus(req); });
  server.on("/api/start", HTTP_GET, handleStart);
  server.on("/api/stop", HTTP_GET, handleStop);
  server.on("/api/list", HTTP_GET, listSdFiles);
  server.on("/api/download", HTTP_GET, handleDownload);
  server.on("/api/delete", HTTP_GET, handleDelete);

  // Basic health
  server.on("/api/ping", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/plain", "pong");
  });

  server.onNotFound([](AsyncWebServerRequest *req) {
    req->send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("Web server started.");
}

// ------------------------- Arduino -------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println("\n=== ESP32 MMA8451 SD Logger ===");

  if (!initLittleFs()) {
    Serial.println("LittleFS error; web UI won't load until fixed.");
  }

  initWiFi();

  if (!initSd()) {
    Serial.println("SD init failed; logging disabled until SD works.");
  }

  initAccel();

  initWebServer();
}

void loop() {
  if (!g_recording) {
    delay(5);
    return;
  }

  uint32_t now = millis();
  if ((now - g_lastSampleMs) < g_intervalMs) {
    delay(1);
    return;
  }
  g_lastSampleMs = now;

  sensors_event_t event;
  mma.getEvent(&event);

  uint32_t dt = now - g_recordStartMs;
  float ax = event.acceleration.x;
  float ay = event.acceleration.y;
  float az = event.acceleration.z;
  // Serial.print("X: \t"); Serial.print(event.acceleration.x); Serial.print("\t");
  // Serial.print("Y: \t"); Serial.print(event.acceleration.y); Serial.print("\t");
  // Serial.print("Z: \t"); Serial.print(event.acceleration.z); Serial.print("\t");
  // Serial.println("m/s^2 ");
  
  // Write CSV line
  if (g_logFile) {
    g_logFile.printf("%lu,%.6f,%.6f,%.6f\n", (unsigned long)dt, ax, ay, az);
    g_samples++;
  }

  // Periodic flush to reduce data loss risk
  if ((now - g_lastFlushMs) >= FILE_FLUSH_MS) {
    g_lastFlushMs = now;
  
    if (g_logFile) g_logFile.flush();

  }
}

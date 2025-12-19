#pragma once
#include <Arduino.h>
#include <Adafruit_MMA8451.h>

// ----------------- I2C (MMA8451) -----------------
// Default ESP32 I2C pins are often SDA=21, SCL=22.
static constexpr int I2C_SDA_PIN = 17;
static constexpr int I2C_SCL_PIN = 18;

// ----------------- SD (SPI) -----------------
// VSPI defaults on ESP32: SCK=18, MISO=19, MOSI=23 (common on dev boards)
static constexpr int SPI_SCK_PIN  = 18;
static constexpr int SPI_MISO_PIN = 19;
static constexpr int SPI_MOSI_PIN = 23;

// SD card chip-select pin (commonly 5 or 15 depending on module)
static constexpr int SD_CS_PIN = 5;

// SD SPI frequency (Hz). 20MHz is usually safe for many modules.
static constexpr uint32_t SD_SPI_FREQ = 20 * 1000 * 1000;

// ----------------- Sampling -----------------
static constexpr uint32_t SAMPLE_INTERVAL_MS_DEFAULT = 5; // 50 Hz
static constexpr uint32_t FILE_FLUSH_MS = 1000;           // flush every 1s

// ----------------- MMA8451 settings -----------------
static constexpr mma8451_range_t MMA_RANGE = MMA8451_RANGE_2_G;

// Data rate enum uses Adafruit_MMA8451 values; choose one of:
// MMA8451_DATARATE_800HZ, _400HZ, _200HZ, _100HZ, _50HZ, _12_5HZ, _6_25HZ, _1_56HZ
static constexpr mma8451_dataRate_t MMA_DATARATE = MMA8451_DATARATE_100_HZ;

// ----------------- WiFi behavior -----------------
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10 * 1000; // 10s
static constexpr char MDNS_NAME[] = "esp32-acc";               // http://esp32-acc.local/
static constexpr char AP_SSID[] = "ESP32-ACC-LOGGER";
static constexpr char AP_PASS[] = "esp32logger"; // >= 8 chars

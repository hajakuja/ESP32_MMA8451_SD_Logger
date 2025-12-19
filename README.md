# ESP32 MMA8451 SD CSV Logger (Web UI from LittleFS)

This Arduino project reads **accelerometer** data from an **MMA8451** over I2C and logs it as CSV to an **SD card via SPI**.
A simple web UI lets you **start/stop recording**, set **file name**, and **download/delete** CSV files.

## CSV format
Header: `timedelta_ms,Xacc,Yacc,Zacc`
- `timedelta_ms` = milliseconds since you pressed **Start**
- `Xacc/Yacc/Zacc` = acceleration in **m/s²** (Adafruit Unified Sensor `event.acceleration.*`)

## Wiring (typical)
### MMA8451 (I2C)
- VCC → 3V3
- GND → GND
- SDA → GPIO 21 (default in `config.h`)
- SCL → GPIO 22 (default in `config.h`)
- INT pins not required

### SD card module (SPI)
- VCC → 3V3 (use 3.3V-safe module!)
- GND → GND
- SCK → GPIO 18
- MISO → GPIO 19
- MOSI → GPIO 23
- CS → GPIO 5

> If your pins differ, edit `config.h`.

## Libraries to install (Arduino Library Manager)
- Adafruit MMA8451 Library
- Adafruit Unified Sensor
- ESP Async WebServer
- AsyncTCP
- ArduinoJson (6.x)

## Upload steps (Arduino IDE)
1. Open the sketch folder `ESP32_MMA8451_SD_Logger` in Arduino IDE.
2. Edit `secrets.h` and set `WIFI_SSID` + `WIFI_PASS`.
   - If left empty, the ESP32 will create an AP: **ESP32-ACC-LOGGER** / **esp32logger**
3. **Upload the sketch** as usual.
4. Upload web UI files (LittleFS):
   - Install the **ESP32 LittleFS Data Upload** plugin (or use Arduino CLI).
   - In Arduino IDE: `Tools → ESP32 LittleFS Data Upload`
   - This uploads the contents of the `data/` folder to LittleFS.
5. Open Serial Monitor to see the IP.
6. Browse to `http://<ip>/` or (if mDNS works) `http://esp32-acc.local/`

## API endpoints
- `GET /api/status`
- `GET /api/start?file=NAME.csv&interval_ms=20`
- `GET /api/stop`
- `GET /api/list`
- `GET /api/download?file=NAME.csv`
- `GET /api/delete?file=NAME.csv`

## Notes / troubleshooting
- If SD init fails, check CS pin and module voltage level shifting.
- If MMA8451 not detected, check SDA/SCL and that the sensor is powered at 3.3V.
- Data rate (`MMA_DATARATE`) and range (`MMA_RANGE`) can be changed in `config.h`.

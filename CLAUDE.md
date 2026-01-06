# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 fingerprint attendance system with R307 sensor, Directus CMS backend, and MQTT remote control. Vietnamese attendance/access control device that enrolls/verifies fingerprints locally and syncs with cloud CMS.

## Build & Development Commands

```bash
# Build project
pio run

# Upload to ESP32-S3
pio run --target upload

# Serial monitor (115200 baud)
pio device monitor

# Build + upload + monitor
pio run --target upload && pio device monitor

# Quick test script
./quick-test.sh
```

## Architecture

### Module Structure
```
src/
├── main.cpp               # Entry point, menu system, main loop
├── config.h               # WiFi, MQTT, Directus, GPIO configs
├── fingerprint-handler.*  # R307 sensor: enroll, verify, template I/O
├── wifi-manager.*         # WiFi connection management
├── http-client.*          # HTTP request wrapper
├── directus-client.*      # Directus REST API client
├── mqtt-client.*          # MQTT pub/sub for remote commands
└── command-handler.*      # Execute MQTT commands (enroll, delete, etc)
```

### Data Flow
1. **Local verification**: R307 scan → match ID + confidence → get template
2. **CMS verification**: Template (Base64) + device MAC → Directus API → access response
3. **Remote control**: MQTT command → CommandHandler → FingerprintHandler → MQTT status

### Key Classes
- `FingerprintHandler`: R307 sensor interface via Adafruit library
- `DirectusClient`: Directus REST API (verify, enroll, device registration, attendance logs)
- `MQTTClient`: PubSubClient wrapper with command/status topics
- `CommandHandler`: Maps MQTT commands to fingerprint operations

## Hardware Configuration

R307 on UART2:
- GPIO16 (RX) ← R307 TX (green wire)
- GPIO17 (TX) → R307 RX (white wire)
- Baud: 57600

## Configuration

Edit `src/config.h`:
- `WIFI_SSID`, `WIFI_PASSWORD`
- `DIRECTUS_URL`, `DIRECTUS_TOKEN`
- `MQTT_BROKER`, `MQTT_PORT`, `MQTT_USERNAME`, `MQTT_PASSWORD`

Use `src/config.example.h` as template.

## Libraries (platformio.ini)

- `Adafruit Fingerprint Sensor Library@^2.1.3` - R307 communication
- `ArduinoJson@^7.2.1` - JSON serialization
- `WebSockets@^2.4.0` - WebSocket client
- `PubSubClient@^2.8` - MQTT client

## Key Technical Notes

- Fingerprint templates: 512 bytes, Base64 encoded for HTTP transmission
- R307 stores up to 1000 templates, Adafruit library limits ID range to 1-127
- WiFi: 2.4GHz only (ESP32-S3 limitation)
- Auto-login mode pauses during MQTT command execution

## Serial Menu Commands

`e` enroll | `d` delete | `r` restore from Directus | `l` list | `t` test login | `a` toggle auto-login | `w` WiFi config | `i` sensor info | `h` help

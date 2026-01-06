#ifndef CONFIG_H
#define CONFIG_H

// ==========================================
// WiFi Configuration
// ==========================================
#define WIFI_SSID "your-wifi-ssid"        // Thay bằng WiFi SSID của bạn
#define WIFI_PASSWORD "your-wifi-password" // Thay bằng WiFi password của bạn
#define WIFI_TIMEOUT_MS 20000              // Timeout kết nối WiFi (20 giây)

// ==========================================
// Directus API Configuration
// ==========================================
#define DIRECTUS_URL "http://192.168.1.xxx:8055"  // Directus URL (LAN IP của máy chạy Directus)
#define DIRECTUS_TOKEN "your-directus-token"      // Static access token từ Directus

// ==========================================
// R307 Fingerprint Sensor Configuration
// ==========================================
#define R307_RX_PIN 16       // ESP32 GPIO16 (RX) -> R307 TX (Xanh lá)
#define R307_TX_PIN 17       // ESP32 GPIO17 (TX) -> R307 RX (Trắng)
#define R307_BAUD_RATE 57600 // Baud rate của R307

// ==========================================
// Device Configuration
// ==========================================
#define DEVICE_ID "ESP32-001"              // Định danh thiết bị (có thể dùng MAC address)
#define DEVICE_BRANCH_ID "your-branch-uuid" // Directus branch UUID

// ==========================================
// MQTT Configuration
// ==========================================
#define MQTT_BROKER "192.168.1.xxx"      // MQTT broker IP
#define MQTT_PORT 1883                   // MQTT port (1883 for TCP, 8883 for TLS)
#define MQTT_USERNAME ""                 // MQTT username (để trống nếu không cần auth)
#define MQTT_PASSWORD ""                 // MQTT password (để trống nếu không cần auth)
#define MQTT_KEEPALIVE_INTERVAL 60       // MQTT keepalive interval (seconds)
#define MQTT_QOS_COMMANDS 1              // QoS level for commands (0, 1, or 2)
#define MQTT_QOS_TELEMETRY 0             // QoS level for telemetry (0 for best effort)

// MQTT Topic Prefix and Templates
#define MQTT_TOPIC_PREFIX "monkey-muaythai"
#define MQTT_TOPIC_COMMANDS_TEMPLATE MQTT_TOPIC_PREFIX "/device/%s/commands"
#define MQTT_TOPIC_STATUS_TEMPLATE MQTT_TOPIC_PREFIX "/device/%s/status"
#define MQTT_TOPIC_TELEMETRY_TEMPLATE MQTT_TOPIC_PREFIX "/device/%s/telemetry"
#define MQTT_TOPIC_ATTENDANCE_LIVE MQTT_TOPIC_PREFIX "/attendance/live"

// ==========================================
// Timing Configuration
// ==========================================
#define FINGERPRINT_CHECK_INTERVAL 1000 // Kiểm tra vân tay mỗi 1 giây (auto-login mode)
#define HTTP_TIMEOUT_MS 10000           // HTTP request timeout (10 giây)

#endif

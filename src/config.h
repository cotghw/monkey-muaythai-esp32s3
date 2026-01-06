#ifndef CONFIG_H
#define CONFIG_H

// ==========================================
// WiFi Configuration
// ==========================================
#define WIFI_SSID "SAN SAN"        // Thay bằng WiFi SSID của bạn
#define WIFI_PASSWORD "0399492511" // Thay bằng WiFi password của bạn
#define WIFI_TIMEOUT_MS 20000      // Timeout kết nối WiFi (20 giây)

// ==========================================
// Directus API Configuration
// ==========================================
#define DIRECTUS_URL "https://optimization-kind-diverse-appeared.trycloudflare.com" // Directus URL (không có trailing slash)
#define DIRECTUS_TOKEN "ux--xiAf1QRI7PcVXdTlNw1FhblowiwU"                           // Static access token từ Directus

// ==========================================
// R307 Fingerprint Sensor Configuration
// ==========================================
#define R307_RX_PIN 16       // ESP32 GPIO16 (RX) -> R307 TX (Xanh lá)
#define R307_TX_PIN 17       // ESP32 GPIO17 (TX) -> R307 RX (Trắng)
#define R307_BAUD_RATE 57600 // Baud rate của R307

// ==========================================
// Device Configuration
// ==========================================
#define DEVICE_ID "ESP32-001" // Định danh thiết bị (có thể dùng MAC address)

// ==========================================
// MQTT Configuration
// ==========================================
#define MQTT_BROKER "broker.hivemq.com"  // Free public MQTT broker (hoặc thay bằng broker riêng)
#define MQTT_PORT 1883                    // MQTT port (1883 for TCP, 8883 for TLS)
#define MQTT_USERNAME ""                  // MQTT username (để trống nếu không cần auth)
#define MQTT_PASSWORD ""                  // MQTT password (để trống nếu không cần auth)
#define MQTT_KEEPALIVE_INTERVAL 60        // MQTT keepalive interval (seconds)
#define MQTT_QOS_COMMANDS 1               // QoS level for commands (0, 1, or 2)
#define MQTT_QOS_TELEMETRY 0              // QoS level for telemetry (0 for best effort)

// ==========================================
// Timing Configuration
// ==========================================
#define FINGERPRINT_CHECK_INTERVAL 1000 // Kiểm tra vân tay mỗi 1 giây (auto-login mode)
#define HTTP_TIMEOUT_MS 10000           // HTTP request timeout (10 giây)

#endif
